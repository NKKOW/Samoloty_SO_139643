#ifndef SAMOLOT_H
#define SAMOLOT_H

#include <pthread.h>

//Deklaracja zmiennych globalnych extern z pliku głównego symulacji
extern pthread_mutex_t mutex; 
extern int capacity; // pojemność samolotu obecnie  
extern int P;  // pojemność samolotu ogólnie

void* samolot_func(void* arg);

#endif
