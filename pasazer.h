#ifndef PASAZER_H
#define PASAZER_H

#include <pthread.h>
#include <semaphore.h>
#include "kontrola.h"

// Zmienne globalne (współdzielone w obrębie samolotu)
extern pthread_mutex_t mutex;
extern pthread_cond_t samolotCond;
extern int capacity;        // liczba wolnych miejsc (schody pasażerskie)
extern int licznik_pasazer; // licznik pasażerów (tych co wsiedli lub odpadli)
extern sem_t bagaz_wagaSem; // semafor do wagi bagażowej

// Funkcja wątku pasażera
void* pasazer_func(void* arg);

// Funkcja pomocnicza - losowanie wagi bagażu
int bagaz_waga();

#endif
