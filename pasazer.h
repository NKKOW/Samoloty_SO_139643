// (Nie zmieniono)

#ifndef PASAZER_H
#define PASAZER_H

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>

extern volatile sig_atomic_t stopBoarding;

void* kontrola_thread_func(void* arg);
int bagaz_waga();

#endif
