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

    // Pobranie PID samolotu
    pid_t airplane_pid = getpid();

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
    printf("[P][PID=%d] Pasażer %ld -> kontrola bagażu (VIP=%d). Waga=%d\n",
           airplane_pid, pasazerNum, isVIP, pasazer_bagaz); // Dodano PID
    sleep(bagaz_checktime);

    // Sprawdzenie flagi zatrzymania boardingu
    if (stopBoarding) {
        printf("[P][PID=%d] Pasażer %ld BOARDING ZATRZYMANY.\n", airplane_pid, pasazerNum); // Dodano PID
        sem_post(&bagaz_wagaSem);

        pthread_mutex_lock(&mutex);
        licznik_pasazer++;
        if (licznik_pasazer == N || (capacityNormal == 0 && capacityVip == 0)) {
            pthread_cond_signal(&samolotCond);
        }
        pthread_mutex_unlock(&mutex);

        return NULL;
    }

    if (pasazer_bagaz > M) {
        printf("[P][PID=%d] Pasażer %ld ODRZUCONY (bagaż=%d > %d)\n",
               airplane_pid, pasazerNum, pasazer_bagaz, M); // Dodano PID
        sem_post(&bagaz_wagaSem);

        pthread_mutex_lock(&mutex);
        licznik_pasazer++;
        if (licznik_pasazer == N || (capacityNormal == 0 && capacityVip == 0)) {
            pthread_cond_signal(&samolotCond);
        }
        pthread_mutex_unlock(&mutex);

        return NULL;
    } else {
        printf("[P][PID=%d] Pasażer %ld bagaż OK (%d <= %d)\n",
               airplane_pid, pasazerNum, pasazer_bagaz, M); // Dodano PID
        sem_post(&bagaz_wagaSem);
    }

    // Sprawdzenie flagi zatrzymania boardingu
    if (stopBoarding) {
        printf("[P][PID=%d] Pasażer %ld BOARDING ZATRZYMANY.\n", airplane_pid, pasazerNum); // Dodano PID

        pthread_mutex_lock(&mutex);
        licznik_pasazer++;
        if (licznik_pasazer == N || (capacityNormal == 0 && capacityVip == 0)) {
            pthread_cond_signal(&samolotCond);
        }
        pthread_mutex_unlock(&mutex);

        pthread_exit(NULL);
        return NULL;
    }

    // Ustalenie płci pasażera (np. do rozdziału w stanowiskach)
    char plec = ((rand_r(&seed) % 2) == 0) ? 'K' : 'M';
    printf("[P][PID=%d] Pasażer %ld -> kontrola bezpieczeństwa, płeć=%c\n",
           airplane_pid, pasazerNum, plec); // Dodano PID

    // Osobna kontrola:
    if (isVIP) {
        kontrola_bezpieczenstwa_vip(pasazerNum, plec);
    } else {
        kontrola_bezpieczenstwa(pasazerNum, plec);
    }

    // Sprawdzenie, czy boarding został zatrzymany
    if (stopBoarding) {
        printf("[P][PID=%d] Pasażer %ld BOARDING ZATRZYMANY.\n", airplane_pid, pasazerNum); // Dodano PID
        pthread_mutex_lock(&mutex);
        licznik_pasazer++;
        if (licznik_pasazer == N || (capacityNormal == 0 && capacityVip == 0)) {
            pthread_cond_signal(&samolotCond);
        }
        pthread_mutex_unlock(&mutex);
        return NULL;
    }

    // Próba wejścia na schody pasażerskie
    
    if (isVIP) {
        // VIP-y korzystają z stairsSemVip
        printf("[P][PID=%d] Pasażer %ld (VIP) próbuje wejść na schody VIP.\n", airplane_pid, pasazerNum); // Dodano PID
        if (sem_wait(&stairsSemVip) == -1) {
            perror("[P] blad sem_wait stairsSemVip");
            return NULL;
        }
        printf("[P][PID=%d] Pasażer %ld (VIP) przechodzi przez schody VIP.\n", airplane_pid, pasazerNum); // Dodano PID

        // Aktualizacja liczby pasażerów na schodach
        pthread_mutex_lock(&stairsMutex);
        passengers_on_stairs++;
        pthread_mutex_unlock(&stairsMutex);

        // Symulacja przejścia przez schody
        sleep(1);

        // Sprawdzenie flagi zatrzymania boardingu
        if (stopBoarding) {
            printf("[P][PID=%d] Pasażer %ld BOARDING ZATRZYMANY podczas schodów.\n", airplane_pid, pasazerNum);
            pthread_mutex_lock(&stairsMutex);
            passengers_on_stairs--;
            pthread_cond_broadcast(&stairsCond);
            pthread_mutex_unlock(&stairsMutex);
            sem_post(&stairsSemVip);
            return NULL;
        }

        pthread_mutex_lock(&stairsMutex);
        passengers_on_stairs--;
        pthread_cond_broadcast(&stairsCond);
        pthread_mutex_unlock(&stairsMutex);

        printf("[P][PID=%d] Pasażer %ld (VIP) dotarł do samolotu.\n", airplane_pid, pasazerNum); // Dodano PID
        sem_post(&stairsSemVip);
    } else {
        // Pasażer zwykły korzysta z stairsSemNormal
        printf("[P][PID=%d] Pasażer %ld (Zwykły) próbuje wejść na schody normalne.\n", airplane_pid, pasazerNum); // Dodano PID
        if (sem_wait(&stairsSemNormal) == -1) {
            perror("[P] blad sem_wait stairsSemNormal");
            return NULL;
        }
        printf("[P][PID=%d] Pasażer %ld (Zwykły) przechodzi przez schody normalne.\n", airplane_pid, pasazerNum); // Dodano PID

        // Aktualizacja liczby pasażerów na schodach
        pthread_mutex_lock(&stairsMutex);
        passengers_on_stairs++;
        pthread_mutex_unlock(&stairsMutex);

        // Symulacja przejścia przez schody
        sleep(1);

        // Sprawdzenie flagi zatrzymania boardingu
        if (stopBoarding) {
            printf("[P][PID=%d] Pasażer %ld BOARDING ZATRZYMANY podczas schodów.\n", airplane_pid, pasazerNum);
            pthread_mutex_lock(&stairsMutex);
            passengers_on_stairs--;
            pthread_cond_broadcast(&stairsCond);
            pthread_mutex_unlock(&stairsMutex);
            sem_post(&stairsSemNormal);
            return NULL;
        }

        pthread_mutex_lock(&stairsMutex);
        passengers_on_stairs--;
        pthread_cond_broadcast(&stairsCond);
        pthread_mutex_unlock(&stairsMutex);

        printf("[P][PID=%d] Pasażer %ld (Zwykły) dotarł do samolotu.\n", airplane_pid, pasazerNum); // Dodano PID
        sem_post(&stairsSemNormal);
    }

    // Próba wejścia do samolotu (schody pasażerskie):
    pthread_mutex_lock(&mutex);

    if (isVIP) {
        // VIP-y korzystają z capacityVip
        if (capacityVip > 0) {
            capacityVip--;
            printf("[P][PID=%d] Pasażer %ld WSZEDŁ do samolotu (VIP) PID=%d. Pozostało %d VIP miejsc.\n",
                   airplane_pid, pasazerNum, airplane_pid, capacityVip); // Dodano PID
        } else {
            printf("[P][PID=%d] Pasażer %ld NIE WSZEDŁ (brak VIP miejsc) PID=%d\n",
                   airplane_pid, pasazerNum, airplane_pid); // Dodano PID
        }
    } else {
        // Pasażer zwykły
        if (capacityNormal > 0) {
            capacityNormal--;
            printf("[P][PID=%d] Pasażer %ld WSZEDŁ do samolotu (Zwykły) PID=%d. Pozostało %d zwykłych miejsc.\n",
                   airplane_pid, pasazerNum, airplane_pid, capacityNormal); // Dodano PID
        } else {
            printf("[P][PID=%d] Pasażer %ld NIE WSZEDŁ (brak zwykłych miejsc) PID=%d\n",
                   airplane_pid, pasazerNum, airplane_pid); // Dodano PID
        }
    }

    // Sprawdzenie flagi zatrzymania boardingu przed zwiększeniem licznika
    if (stopBoarding) {
        printf("[P][PID=%d] Pasażer %ld BOARDING ZATRZYMANY przed zwiększeniem licznika.\n", airplane_pid, pasazerNum);
        pthread_mutex_unlock(&mutex);
        return NULL;
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
