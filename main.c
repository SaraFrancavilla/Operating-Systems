#include <cstdio>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "journey.h"
#include "controller.h"

int main(int argc, char *argv[]) {
    // Check if the correct number of arguments is provided
    if (argc != 2) {
        printf("Please insert: ./a.exe MAP1 or ./a.exe MAP2\n");
        return 1;
    }

    // Remove existing FIFOs to prevent "File exists" error
    unlink(REQUEST_FIFO);

    // Creating request pipe
    if (mkfifo(REQUEST_FIFO, 0666) == -1) {
        perror("mkfifo /tmp/request_fifo");
        exit(EXIT_FAILURE);
    }

    for (int i = 1; i <= NUM_PROCESSES; i++) {
        char fifo_path[BUFFER_SIZE];
        snprintf(fifo_path, sizeof(fifo_path), "%s%d", RESPONSE_FIFO, i);
        unlink(fifo_path);

        // Creating response pipe
        if (mkfifo(fifo_path, 0666) == -1) {
            perror("mkfifo response_fifo");
            exit(EXIT_FAILURE);
        }
    }

    pid_t controller_pid, journey_pid;

    // Fork journey process
    journey_pid = fork();
    if (journey_pid == 0) {
        startJourney(argv[1]);
        exit(0);
    } else if (journey_pid < 0) {
        perror("fork journey");
        exit(1);
    }

    // Fork controller process
    controller_pid = fork();
    if (controller_pid == 0) {
        startController();
        exit(0);
    } else if (controller_pid < 0) {
        perror("fork controller");
        exit(1);
    }

    // Waits for child processes to complete
    wait(NULL);
    wait(NULL);

    return 0;
}
