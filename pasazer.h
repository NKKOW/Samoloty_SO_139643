#ifndef PASAZER_H
#define PASAZER_H

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>

// Flaga sygnalizująca zatrzymanie
extern volatile sig_atomic_t stopBoarding;

// Funkcja wątku kontroli
void* kontrola_thread_func(void* arg);

// Funkcja do obliczania wagi bagażu
int bagaz_waga();

#endif
