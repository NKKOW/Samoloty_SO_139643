#ifndef PASAZER_H
#define PASAZER_H

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include "kontrola.h"

// Rozdzielenie na schody zwykłe i VIP.
extern int capacityNormal;    // Liczba wolnych miejsc na schodach zwykłych
extern int capacityVip;       // Liczba wolnych miejsc na schodach VIP
extern int licznik_pasazer;   // Licznik pasażerów (tych co wsiedli lub odpadli)

// Sygnalizacja dla samolotu (gdy ktoś wejdzie lub zrezygnuje)
extern pthread_mutex_t mutex;
extern pthread_cond_t samolotCond;

// Semafor do wagi bagażowej
extern sem_t bagaz_wagaSem;

// Semafory do schodów
extern sem_t stairsSemNormal;
extern sem_t stairsSemVip;

// Flaga zatrzymująca boarding
extern volatile sig_atomic_t stopBoarding;

// Funkcja wątku pasażera
void* pasazer_func(void* arg);

// Funkcja pomocnicza - losowanie wagi bagażu
int bagaz_waga();

#endif   
