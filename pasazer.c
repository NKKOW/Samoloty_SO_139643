#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "pasazer.h"

int bagaz_waga()
{
    return (rand() % 13) + 1;
}

void* pasazer_func(void* arg){
    long pasazerNum = (long) arg; //numer pasazera
    unsigned long tid = (unsigned long) pthread_self(); //ID wątku

    //Odprawa bagażowa
    int pasazer_bagaz = bagaz_waga();
    sem_wait(&bagaz_wagaSem);
    int bagaz_checktime = rand() % 3;
    printf("Pasazer %ld [TID = %lu] rozpoczyna kontrole bagazu. Waga bagazu %d.\n", pasazerNum, tid, pasazer_bagaz);
    sleep(bagaz_checktime);

    if (pasazer_bagaz > M)
    {
        printf("Pasazer %ld [TID = %lu] odrzucony. Waga bagazu %d większa od dopuszczalnego %d. Odprawa do terminala.\n", pasazerNum, tid, pasazer_bagaz, M);
        //zwolnienie stanowiska
        sem_post(&bagaz_wagaSem);

        pthread_mutex_lock(&mutex);
        licznik_pasazer++;
        // sprawdzamy, czy to koniec
        if (licznik_pasazer == N || capacity == 0) 
        {
            pthread_cond_signal(&samolotCond);
        }
        pthread_mutex_unlock(&mutex);
        
        return NULL;
    }
    else 
    {
        printf("Pasażer %ld [TID = %lu] zakończył kontrolę bagażu (OK). Waga=%d\n", pasazerNum, tid, pasazer_bagaz);
        sem_post(&bagaz_wagaSem);
    }


    //Przypisywanie płci pasażera
    char plec = ((rand() % 2) == 0) ? 'K' : 'M';
    printf("Pasazer %ld [TID = %lu] przechodzi do kontroli bezpieczenstwa (plec = %c)\n", pasazerNum, tid, plec);
    //Pasażer przechodzi kontrolę
    kontrola_bezpieczenstwa(pasazerNum, plec); 

    //Pętla sprawdzająca liczbę wolnych miejsc
    pthread_mutex_lock(&mutex);
    if (capacity > 0 )
    {
        capacity --;
        printf("Pasazer %ld [TID = %lu] wsiadl do samolotu. Liczba wolnych miejsc %d.\n",pasazerNum, tid, capacity);
        if (capacity == 0) 
        {
            pthread_cond_signal(&samolotCond);
        }
    }
    else
    {
        printf("Pasazer %ld [TID = %lu] nie wsiadl. Samolot zapelniony\n", pasazerNum, tid);
    }

    licznik_pasazer++;
    if (licznik_pasazer == N || capacity == 0) 
    {
    pthread_cond_signal(&samolotCond);
    }
    
    pthread_mutex_unlock(&mutex);
    return NULL;

}