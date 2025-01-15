#include <stdio.h>
#include <pthread.h>
#include "pasazer.h"

extern pthread_mutex_t mutex;
extern int capacity; // liczba wolnych miejsc

void* pasazer_func(void* arg){
    long id = (long) arg;

    pthread_mutex_lock(&mutex);
    if (capacity > 0 )
    {
        capacity --;
        printf("Pasazer %ld wsiadl do samolotu. Liczba wolnych miejsc %d.\n", id, capacity);
    }
    else
    {
        printf("Pasazer %ld nie wsiadl. Samolot zapelniony", id);
    }
    pthread_mutex_unlock(&mutex);
    return NULL;

}