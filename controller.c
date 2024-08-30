#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/shm.h> 
#include <cstring>
#include <errno.h>
#include <time.h>
#include "controller.h"
#include "journey.h"
#include "lock.h"

char *shared_memory;  

void startController() {

    createSharedMemory();
    createLock(); // Initialize the lock file

    // Creating the trains
    for (int i = 0; i < NUM_PROCESSES; ++i) {
        pid_t train_pid = fork();
        if (train_pid == 0) {
            // Train process
            char* journey = askJourney(i);

            removeNewlines(journey);

            char *position = strtok(journey, " ");
            char previous_destination[10] = "";
            char next_destination[10] = "";
            bool modified = false;

            position = strtok(NULL, " ");

            while (position != NULL) {
                
                strncpy(previous_destination, position, sizeof(previous_destination) - 1);
                previous_destination[sizeof(previous_destination) - 1] = '\0';

                position = strtok(NULL, " ");

                if (position != NULL) {
                    strncpy(next_destination, position, sizeof(next_destination) - 1);
                    next_destination[sizeof(next_destination) - 1] = '\0'; 
                } else {
                    next_destination[0] = '\0'; // No more destinations
                }

                int index_destination = extractIndex(next_destination);
                int index_current_position = extractIndex(previous_destination);                

                logTrainActivity(i+1, previous_destination, next_destination);

                while (!modified){
                    modified = true;
                    modified = modifySharedMemory(index_current_position, index_destination);
                }
                modified = false;
                sleep(2);
                
            }

            // Free allocated memory
            free(journey);

            exit(0); // Child process exits after its work is done
        } else if (train_pid < 0) {
            perror("fork train");
            exit(1);
        }
    }

    for (int i = 0; i < NUM_PROCESSES; ++i) {
        wait(NULL);
    }

    cleanupSharedMemory(); // Clean up shared memory when done
    cleanupLock();     // Clean up the lock file

    stopJourney();
}

void createSharedMemory() {
    int shmid;
    
    // Create the shared memory segment
    shmid = shmget(SHM_KEY, SHM_SIZE, IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget");
        exit(1);
    }
    
    // Attach to the shared memory segment
    shared_memory = (char*) shmat(shmid, NULL, 0);
    if (shared_memory == (char *) -1) {
        perror("shmat");
        exit(1);
    }
    
    // Initialize shared memory
    memset(shared_memory, '0', SHM_SIZE);
    
}

void cleanupSharedMemory() {
    int shmid;
    
    // Get the ID of the shared memory segment
    shmid = shmget(SHM_KEY, SHM_SIZE, 0666);
    if (shmid < 0) {
        perror("shmget");
        exit(1);
    }
    
    // Detach from the shared memory segment
    if (shmdt(shared_memory) == -1) {
        perror("shmdt");
        exit(1);
    }
    
    // Remove the shared memory segment
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        exit(1);
    }
    
}

void stopJourney(){

    char write_buffer[BUFFER_SIZE];

    int request_fd;

    // Open request FIFO for writing
    if ((request_fd = open(REQUEST_FIFO, O_WRONLY)) < 0) {
        perror("open request FIFO");
        exit(EXIT_FAILURE);
    }

    // Send a request
    snprintf(write_buffer, sizeof(write_buffer), "STOP");
    while (write(request_fd, write_buffer, strlen(write_buffer)) < 0) {
        if (errno != EAGAIN) {
            perror("write request FIFO");
            break;
        }
        usleep(100000);
    }

    close(request_fd);
}


char* askJourney(int i){
    char read_buffer[BUFFER_SIZE];
            char write_buffer[BUFFER_SIZE];

            int request_fd, response_fd;

            // Open request FIFO for writing
            if ((request_fd = open(REQUEST_FIFO, O_WRONLY)) < 0) {
                perror("open request FIFO");
                exit(EXIT_FAILURE);
            }

            // Create a modifiable array for the FIFO path
            char fifo_path[BUFFER_SIZE];
            snprintf(fifo_path, sizeof(fifo_path), "%s%d", RESPONSE_FIFO, i + 1);

            // Open response FIFO for reading
            if ((response_fd = open(fifo_path, O_RDONLY | O_NONBLOCK)) < 0) {
                perror("open response FIFO");
                exit(EXIT_FAILURE);
            }

            // Send a request
            snprintf(write_buffer, sizeof(write_buffer), "%d\n", i + 1);
            while (write(request_fd, write_buffer, strlen(write_buffer)) < 0) {
                if (errno != EAGAIN) {
                    perror("write request FIFO");
                    break;
                }
                usleep(100000);
            }

            // Attempt to read a response
            while (1) {
                ssize_t bytes_read = read(response_fd, read_buffer, sizeof(read_buffer) - 1);
                if (bytes_read > 0) {
                    read_buffer[bytes_read] = '\0';
                    if ((read_buffer[1] - '0') == i + 1) {  // The train receives only the buffer it needs
                        break;
                    }
                } else if (bytes_read < 0) {
                    if (errno != EAGAIN) {
                        perror("read response FIFO");
                        break;
                    }
                    usleep(100000);
                }
            }

            close(request_fd);
            close(response_fd);

    // Allocate memory for the string to be returned
    char* return_buffer = (char*) malloc(strlen(read_buffer) + 1);
    if (return_buffer == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    strcpy(return_buffer, read_buffer);

    return return_buffer;
}

bool modifySharedMemory(int current_index, int next_index) {

    lock();

    // Access shared memory and modify the specified segment
    bool modified = false;

    // If next_index is occupied return false
    if (next_index > 0 && shared_memory[next_index - 1] == '1') {
        modified = false;
    } 
    // If next_index is 0, and current_index is non-zero
    else if (next_index == 0 && current_index > 0) {
        shared_memory[current_index - 1] = '0';
        modified = true;
    }
    // If current_index is 0, and next_index is non-zero
    else if (current_index == 0 && next_index > 0) {
        if (shared_memory[next_index - 1] == '0') {
            shared_memory[next_index - 1] = '1';
            modified = true;
        }
    }
    // If both current_index and next_index are non-zero
    else if (current_index > 0 && next_index > 0) {
        if (shared_memory[next_index - 1] == '0') {
            shared_memory[next_index - 1] = '1';
            shared_memory[current_index - 1] = '0';
            modified = true;
        }
    }else if (current_index == 0 && next_index == 0) {
        modified = true;
    }

    unlock();

    return modified;
}

int extractIndex(const char* str) {
    
    if (str[0] == 'S' || str[0] == 'T') {
        return 0; // "Sx" or "Tx" 
    }

    int number = atoi(str + 2); // Convert part after "MA" to an integer
    return number;
    
}

void getCurrentTime(char* buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info); // Format: 2024-07-28 22:33:10
}

void logTrainActivity(int train_number, const char* current_position, const char* next_position) {
    char filename[20];
    snprintf(filename, sizeof(filename), "T%d.log", train_number);
    
    FILE *logfile = fopen(filename, "a");
    if (logfile == NULL) {
        perror("Error opening log file");
        exit(EXIT_FAILURE);
    }

    char time_buffer[20];
    getCurrentTime(time_buffer, sizeof(time_buffer));

    fprintf(logfile, "[Current: %s], [Next: %s], %s\n", current_position, next_position, time_buffer);
    
    fclose(logfile);
}

void removeNewlines(char *str) {
    char *src = str, *dst = str;

    while (*src) {
        if (*src != '\n' && *src != '\r') {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0'; // Null-terminate the string
}
