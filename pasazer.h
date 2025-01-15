#ifndef PASAZER_H
#define PASAZER_H

#include <pthread.h>

extern pthread_mutex_t mutex;
extern int capacity; // liczba wolnych miejsc


void* pasazer_func(void* arg);


#endif