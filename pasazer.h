#ifndef PASAZER_H
#define PASAZER_H

#include <pthread.h>
#include <semaphore.h>

extern pthread_mutex_t mutex;
extern int capacity; // liczba wolnych miejsc
extern int licznik_pasazer;
extern int M;
extern sem_t bagaz_wagaSem;

void* pasazer_func(void* arg);
int bagaz_waga();

#endif