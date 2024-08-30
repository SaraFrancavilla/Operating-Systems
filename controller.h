#ifndef CONTROLLER_H
#define CONTROLLER_H

#define NUM_PROCESSES 5
#define NUM_MAX_SEGMENTS 16
#define SHM_SIZE (NUM_MAX_SEGMENTS * sizeof(char))
#define SHM_KEY 1234 

void startController();

void createSharedMemory();

void cleanupSharedMemory();

char* askJourney(int i);

bool modifySharedMemory(int current_index, int next_index);

int extractIndex(const char* str);

void stopJourney();

void getCurrentTime(char* buffer, size_t size);


void logTrainActivity(int train_number, const char* current_position, const char* next_position);

void removeNewlines(char *str);

#endif