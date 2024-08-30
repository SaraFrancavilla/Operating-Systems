#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#define SEM_NAME "/train_lock_semaphore"

sem_t *sem;

void createLock() {
    // Open semaphore
    sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
}

// Close and unlink the semaphore
void cleanupLock() {
    if (sem_close(sem) == -1) {
        perror("sem_close");
    }
    if (sem_unlink(SEM_NAME) == -1) {
        perror("sem_unlink");
    }
}

// Acquire the semaphore
void lock() {
    if (sem_wait(sem) == -1) {
        perror("sem_wait");
        exit(EXIT_FAILURE);
    }
}

// Release the semaphore
void unlock() {
    if (sem_post(sem) == -1) {
        perror("sem_post");
        exit(EXIT_FAILURE);
    }
}
