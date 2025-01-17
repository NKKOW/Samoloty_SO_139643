#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "pasazer.h"

int bagaz_waga()
{
    return (rand() % 13) + 1;
}

void* pasazer_func(void* arg){
    long id = (long) arg;

    //Odprawa bagażowa
    int pasazer_bagaz = bagaz_waga();
    sem_wait(&bagaz_wagaSem);
    int bagaz_checktime = rand() % 3;
    printf("Pasazer %ld rozpoczyna kontole bagazu. Waga bagazu %d.\n", id, pasazer_bagaz);
    sleep(bagaz_checktime);

    if (pasazer_bagaz > M)
    {
        printf("Pasazer %ld odrzucony. Waga bagazu %d większa od dopuszczalnego %d. Odprawa do terminala.\n", id, pasazer_bagaz, M);
        //zwolnienie stanowiska
        sem_post(&bagaz_wagaSem);

        pthread_mutex_lock(&mutex);
        licznik_pasazer++;
        pthread_mutex_unlock(&mutex);
        
        return NULL;
    }
    else 
    {
        printf("Pasażer %ld zakończył kontrolę bagażu (OK). Waga=%d\n", id, pasazer_bagaz);
        sem_post(&bagaz_wagaSem);
    }


    //Przypisywanie płci pasażera
    char plec = ((rand() % 2) == 0) ? 'K' : 'M';
    printf("Pasazer %ld przechodzi do kontroli bezpieczenstwa (plec = %c)\n", id, plec);
    //Pasażer przechodzi kontrolę
    kontrola_bezpieczenstwa(id, plec); 

    //Pętla sprawdzająca liczbę wolnych miejsc
    pthread_mutex_lock(&mutex);
    if (capacity > 0 )
    {
        capacity --;
        printf("Pasazer %ld wsiadl do samolotu. Liczba wolnych miejsc %d.\n", id, capacity);
    }
    else
    {
        printf("Pasazer %ld nie wsiadl. Samolot zapelniony\n", id);
    }
    licznik_pasazer++;
    pthread_mutex_unlock(&mutex);
    return NULL;

}