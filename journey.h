#ifndef JOURNEY_H
#define JOURNEY_H

#define REQUEST_FIFO "/tmp/request_fifo"
#define RESPONSE_FIFO "/tmp/train_fifo"
#define MAX_LINE_LENGTH 256
#define MAX_LINES 5
#define BUFFER_SIZE 512
#define err(mess) { fprintf(stderr,"Error: %s.", mess); exit(1); }


void startJourney(char *input);

char** readFromFile(char *filename);

#endif