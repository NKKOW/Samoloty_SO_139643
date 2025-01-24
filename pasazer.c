#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "pasazer.h"
#include "global.h"

// Definicje zmiennych globalnych:
pthread_mutex_t mutex;
pthread_cond_t samolotCond;
int capacity;
int licznik_pasazer;
sem_t bagaz_wagaSem;

int bagaz_waga()
{
    return (rand() % 13) + 1; // waga 1..13
}

void* pasazer_func(void* arg){
    long pasazerNum = (long) arg; // numer pasażera
    unsigned long tid = (unsigned long) pthread_self(); // ID wątku

    // Losujemy czy to VIP
    int isVIP = 0;
    if ((rand() % 100) < VIP_PERCENT) {
        isVIP = 1;
    }

    // Odprawa bagażowa:
    int pasazer_bagaz = bagaz_waga();
    sem_wait(&bagaz_wagaSem);
    int bagaz_checktime = rand() % 3;
    printf("[P] Pasazer %ld [TID=%lu] -> kontrola bagazu (VIP=%d). Waga=%d\n",
           pasazerNum, tid, isVIP, pasazer_bagaz);
    sleep(bagaz_checktime);

    if (pasazer_bagaz > M) {
        printf("[P] Pasazer %ld [TID=%lu] ODRZUCONY (bagaż=%d > %d)\n",
               pasazerNum, tid, pasazer_bagaz, M);
        sem_post(&bagaz_wagaSem);

        pthread_mutex_lock(&mutex);
        licznik_pasazer++;
        if (licznik_pasazer == N || capacity == 0) {
            pthread_cond_signal(&samolotCond);
        }
        pthread_mutex_unlock(&mutex);

        return NULL;
    } else {
        printf("[P] Pasazer %ld [TID=%lu] bagaz OK (%d <= %d)\n",
               pasazerNum, tid, pasazer_bagaz, M);
        sem_post(&bagaz_wagaSem);
    }

    // Ustalenie płci pasażera
    char plec = ((rand() % 2) == 0) ? 'K' : 'M';
    printf("[P] Pasazer %ld [TID=%lu] -> kontrola bezpieczeństwa, plec=%c\n",
           pasazerNum, tid, plec);

    // Kontrola bezpieczeństwa (z ewentualnym pominięciem kolejki)
    kontrola_bezpieczenstwa(pasazerNum, plec, isVIP);

    // Po przejściu kontroli próbujemy "wejść" do samolotu (schody pasażerskie)
    pthread_mutex_lock(&mutex);
    if (capacity > 0) {
        capacity--;
        printf("[P] Pasazer %ld [TID=%lu] WSZEDŁ do samolotu (pozostało %d miejsc)\n",
               pasazerNum, tid, capacity);

        if (capacity == 0) {
            pthread_cond_signal(&samolotCond);
        }
    } else {
        printf("[P] Pasazer %ld [TID=%lu] NIE WSZEDŁ (brak miejsc)\n",
               pasazerNum, tid);
    }

    licznik_pasazer++;
    if (licznik_pasazer == N || capacity == 0) {
        pthread_cond_signal(&samolotCond);
    }
    pthread_mutex_unlock(&mutex);

    return NULL;
}
