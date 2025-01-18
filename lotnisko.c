#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>

#include "samolot.h"
#include "pasazer.h"
#include "kontrola.h"

int P = 10; //pojemność samolotu
int N = 10; // liczba pasażerów
int M = 10; //limit wagi bagażu
int capacity; // liczba wolnych miejsc
int licznik_pasazer = 0; //licznik pasażerów w kontroli

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t samolotCond = PTHREAD_COND_INITIALIZER;
sem_t bagaz_wagaSem;

int main(){

    srand(time(NULL));

    capacity = P;

    //Inicjalizacja semafora do sprawdzania bagażu
    sem_init(&bagaz_wagaSem, 0, 3);

    //Tworzenie wątku samolotu
    pthread_t samolot_tid;
    pthread_create(&samolot_tid, NULL, samolot_func, NULL);

    //Tworzenie wątku pasażerów
    pthread_t pasazer_tid[N];
    for (long i = 1; i <= N; i++)
    {
        pthread_create(&pasazer_tid[i - 1], NULL, pasazer_func, (void*) i);
    }

    for (int i = 0; i < N; i++)
    {
        pthread_join(pasazer_tid[i], NULL);
    }

    pthread_join(samolot_tid, NULL);

    sem_destroy(&bagaz_wagaSem);

    printf("Koniec symulacji. Pozostało %d wolnych miejsc.Liczba pasażerów na pokładzie = %d.\n",capacity, P - capacity);

    return 0;
}