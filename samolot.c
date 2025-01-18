#include <stdio.h>
#include <unistd.h>
#include "samolot.h"

void* samolot_func(void* arg){
    printf("Start watku samolotu. Pojemnosc = %d\n", capacity);
    
    pthread_mutex_lock(&mutex);
    //Jeżeli capacity == 0, to wszystkie miejsca są zapełnione

    while (capacity != 0 && licznik_pasazer != N)
    {
        pthread_cond_wait(&samolotCond, &mutex);
    }
        
    //czy samolot jest zapełniony? czy pasażerowie skończyli odprawę?
    if(capacity == 0)
    {
        printf("Start samolotu! (wszystkie %d miejsc zajętch)\n", P);
    }
    else if (licznik_pasazer == N)
    {
        printf("Start samolotu! (Pozostalo %d wolnych miejsc)\n", capacity);
    }

    pthread_mutex_unlock(&mutex);


    return NULL;
}