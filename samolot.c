#include <stdio.h>
#include <unistd.h>
#include "samolot.h"

void* samolot_func(void* arg){
    printf("Start watku samolotu. Pojemnosc = %d\n", capacity);

    //Jeżeli capacity == 0, to wszystkie miejsca są zapełnione

    while(1)
    {
        sleep(1);

        pthread_mutex_lock(&mutex);

        //czy samolot jest zapełniony?
        if(capacity == 0)
        {
            pthread_mutex_unlock(&mutex);
            printf("Start samolotu! (wszystkie %d miejsc zajętch)\n", P);
            break;
        }
        

        //czy wszyscy psażerowie skończyli odprawę?
        if (licznik_pasazer == N)
        {
            pthread_mutex_unlock(&mutex);
            printf("Start samolotu! (Pozostalo %d wolnych miejsc)\n", capacity);
            break;
        }

        pthread_mutex_unlock(&mutex);
    }

    return NULL;
}