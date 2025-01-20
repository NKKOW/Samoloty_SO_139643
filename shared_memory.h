#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

typedef struct{
    int akt_samoloty; //liczba aktywnych samolotów
    //inne pola
} shared_data;

extern shared_data *shm_ptr;

//Funkcje zarządzające pamięcią wspóldzieloną
int shared_memory_init();
shared_data* shared_memory_att();
void shared_memory_detach(shared_data *shm_ptr);
void shared_memory_cleanup();

//Funkcje zarządzające semaforami
int semaphore_init();
int semaphore_att();
int semaphore_lock();
int semaphore_unlock();
void semaphore_cleanup();

#endif