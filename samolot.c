#include "samolot.h"
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

//Deklaracja zmiennych globalnych extern z pliku głoównego symulacji
extern pthread_mutex_t mutex; 
extern int capacity; // pojemność samolotu obecnie  
extern int P;  // pojemność samolotu ogólnie

void* samolot_func(void* arg){
    print("Start watku samolotu. Pojemnosc = %d\n", capacity);

    //Jeżeli capacity == 0, to wszystkie miejsca są zapełnione

    while(1)
    {
        sleep(1);

        pthread_mutex_lock(&mutex);
        if(capacity == 0)
        {
            pthread_mutex_unlock(&mutex);
            break;
        }

        pthread_mutex_unlock(&mutex);
    }

    printf("Start samolotu! (Wszystkie %d miejsca zajete)", P);
    return NULL;
}