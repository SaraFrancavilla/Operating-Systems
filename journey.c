#include <stdio.h>
#include <string.h>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include "journey.h"
#include "controller.h"

void startJourney(char *input) {
    if (strcmp(input, "MAP1") != 0 && strcmp(input, "MAP2") != 0) {
        exit(0);
    }

    char **journeys = readFromFile(strcat(input, ".txt"));

    int request_fd, response_fd[NUM_PROCESSES];
    char read_buffer[BUFFER_SIZE];
    char write_buffer[BUFFER_SIZE];

    // Open request FIFO in non-blocking mode for reading
    if ((request_fd = open(REQUEST_FIFO, O_RDONLY | O_NONBLOCK)) < 0) {
        perror("open request FIFO");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < NUM_PROCESSES; i++) {
        // Create a modifiable array for the FIFO path
        char fifo_path[BUFFER_SIZE];
        snprintf(fifo_path, sizeof(fifo_path), "%s%d", RESPONSE_FIFO, i + 1);

        // Open response FIFO for writing
        if ((response_fd[i] = open(fifo_path, O_WRONLY)) < 0) {
            perror("open response FIFO");
            exit(EXIT_FAILURE);
        }
    }

    while (1) {
        ssize_t bytes_read = read(request_fd, read_buffer, sizeof(read_buffer) - 1);
        if (bytes_read > 0) {
            read_buffer[bytes_read] = '\0';

            // Divide the buffer into lines
            char *line = strtok(read_buffer, "\n");
            
            if (strcmp(line, "STOP") == 0) {    //Check if controller has finished
                break;
            }

            while (line != NULL) {
                int train_num = atoi(line); 

                snprintf(write_buffer, sizeof(write_buffer), "%s\n", journeys[train_num - 1]);
                while (write(response_fd[train_num - 1], write_buffer, strlen(write_buffer)) < 0) {
                    if (errno != EAGAIN) {
                        perror("write response FIFO");
                        break;
                    }
                    usleep(100000);
                }

                line = strtok(NULL, "\n");
            }
        } else if (bytes_read < 0) {
            if (errno != EAGAIN) {
                perror("read request FIFO");
                break;
            }
            usleep(100000);
        }

        // Small sleep to reduce CPU usage
        usleep(100000);
    }

    close(request_fd);
    for (int i = 0; i < NUM_PROCESSES; i++) {
        close(response_fd[i]);
    }

    // Freeing memory
    for (int i = 0; i < MAX_LINES; i++) {
        free(journeys[i]);
    }
    free(journeys); 

    exit(0);
}

char **readFromFile(char *filename) {
    FILE *file = fopen(filename, "r");

    if (file == NULL) {
        perror("Unable to open file");
        exit(0);
    }

    // Allocating memory for the array
    char **lines = (char **)malloc(MAX_LINES * sizeof(char *));
    if (lines == NULL) {
        perror("Memory allocation failed");
        fclose(file);
        return NULL;
    }

    int line_count = 0;
    char buffer[MAX_LINE_LENGTH];

    // Read from file
    while (fgets(buffer, MAX_LINE_LENGTH, file) && line_count < MAX_LINES) {
        buffer[strcspn(buffer, "\n")] = '\0';

        lines[line_count] = (char *)malloc((strlen(buffer) + 1) * sizeof(char));
        if (lines[line_count] == NULL) {
            perror("Memory allocation failed");
            fclose(file);
            exit(EXIT_FAILURE);
        }
        strcpy(lines[line_count], buffer);
        line_count++;
    }

    fclose(file);

    return lines;
}
