#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>

#include "pasazer.h"
#include "global.h"
#include "kontrola.h"

// Funkcja wątku pasażera
void* pasazer_func(void* arg){
    long pasazerNum = (long) arg; // Numer pasażera
    unsigned long tid = (unsigned long) pthread_self(); // ID wątku

    // Inicjalizacja lokalnego seed do rand_r
    unsigned int seed = time(NULL) ^ pthread_self();

    // Losujemy czy to VIP
    int isVIP = 0;
    if ((rand_r(&seed) % 100) < VIP_PERCENT) {
        isVIP = 1;
    }

    // Odprawa bagażowa:
    int pasazer_bagaz = bagaz_waga();
    sem_wait(&bagaz_wagaSem);
    int bagaz_checktime = rand_r(&seed) % 3;
    printf("[P] Pasazer %ld [TID=%lu] -> kontrola bagażu (VIP=%d). Waga=%d\n",
           pasazerNum, tid, isVIP, pasazer_bagaz);
    sleep(bagaz_checktime);

    if (pasazer_bagaz > M) {
        printf("[P] Pasazer %ld [TID=%lu] ODRZUCONY (bagaż=%d > %d)\n",
               pasazerNum, tid, pasazer_bagaz, M);
        sem_post(&bagaz_wagaSem);

        pthread_mutex_lock(&mutex);
        licznik_pasazer++;
        if (licznik_pasazer == N || (capacityNormal == 0 && capacityVip == 0)) {
            pthread_cond_signal(&samolotCond);
        }
        pthread_mutex_unlock(&mutex);

        return NULL;
    } else {
        printf("[P] Pasazer %ld [TID=%lu] bagaż OK (%d <= %d)\n",
               pasazerNum, tid, pasazer_bagaz, M);
        sem_post(&bagaz_wagaSem);
    }

    // Ustalenie płci pasażera (np. do rozdziału w stanowiskach)
    char plec = ((rand_r(&seed) % 2) == 0) ? 'K' : 'M';
    printf("[P] Pasazer %ld [TID=%lu] -> kontrola bezpieczeństwa, plec=%c\n",
           pasazerNum, tid, plec);

    // Osobna kontrola:
    if (isVIP) {
        kontrola_bezpieczenstwa_vip(pasazerNum, plec);
    } else {
        kontrola_bezpieczenstwa(pasazerNum, plec);
    }

    // Sprawdzenie, czy boarding został zatrzymany
    if (stopBoarding) {
        printf("[P] Pasazer %ld [TID=%lu] BOARDING ZATRZYMANY.\n", pasazerNum, tid);
        pthread_mutex_lock(&mutex);
        licznik_pasazer++;
        if (licznik_pasazer == N || (capacityNormal == 0 && capacityVip == 0)) {
            pthread_cond_signal(&samolotCond);
        }
        pthread_mutex_unlock(&mutex);
        return NULL;
    }

    // Próba wejścia na schody pasażerskie
    // Implementowanie FIFO przez semafory

    if (isVIP) {
        // VIP-y korzystają z stairsSemVip
        printf("[P] Pasazer %ld [TID=%lu] (VIP) próbuje wejść na schody VIP.\n", pasazerNum, tid);
        sem_wait(&stairsSemVip);
        printf("[P] Pasazer %ld [TID=%lu] (VIP) przechodzi przez schody VIP.\n", pasazerNum, tid);
        // Symulacja przejścia przez schody
        sleep(1);
        printf("[P] Pasazer %ld [TID=%lu] (VIP) dotarł do samolotu.\n", pasazerNum, tid);
        sem_post(&stairsSemVip);
    } else {
        // Pasażer zwykły korzysta z stairsSemNormal
        printf("[P] Pasazer %ld [TID=%lu] (Zwykły) próbuje wejść na schody normalne.\n", pasazerNum, tid);
        sem_wait(&stairsSemNormal);
        printf("[P] Pasazer %ld [TID=%lu] (Zwykły) przechodzi przez schody normalne.\n", pasazerNum, tid);
        // Symulacja przejścia przez schody
        sleep(1);
        printf("[P] Pasazer %ld [TID=%lu] (Zwykły) dotarł do samolotu.\n", pasazerNum, tid);
        sem_post(&stairsSemNormal);
    }

    // Próba wejścia do samolotu (schody pasażerskie):
    pthread_mutex_lock(&mutex);
    pid_t airplane_pid = getpid(); // PID samolotu

    if (isVIP) {
        // VIP-y korzystają z capacityVip
        if (capacityVip > 0) {
            capacityVip--;
            printf("[P] Pasazer %ld [TID=%lu] WSZEDŁ do samolotu (VIP) PID=%d. Pozostało %d VIP miejsc.\n",
                   pasazerNum, tid, airplane_pid, capacityVip);
        } else {
            printf("[P] Pasazer %ld [TID=%lu] NIE WSZEDŁ (brak VIP miejsc) PID=%d\n",
                   pasazerNum, tid, airplane_pid);
        }
    } else {
        // Pasażer zwykły
        if (capacityNormal > 0) {
            capacityNormal--;
            printf("[P] Pasazer %ld [TID=%lu] WSZEDŁ do samolotu (Zwykły) PID=%d. Pozostało %d zwykłych miejsc.\n",
                   pasazerNum, tid, airplane_pid, capacityNormal);
        } else {
            printf("[P] Pasazer %ld [TID=%lu] NIE WSZEDŁ (brak zwykłych miejsc) PID=%d\n",
                   pasazerNum, tid, airplane_pid);
        }
    }

    licznik_pasazer++;
    if (licznik_pasazer == N || (capacityNormal == 0 && capacityVip == 0)) {
        pthread_cond_signal(&samolotCond);
    }
    pthread_mutex_unlock(&mutex);

    return NULL;
}

// Funkcja pomocnicza - losowanie wagi bagażu
int bagaz_waga()
{
    return (rand() % 13) + 1; // Waga 1..13
}
