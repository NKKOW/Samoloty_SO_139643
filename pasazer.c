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
    // unsigned long tid = (unsigned long) pthread_self(); // Usunięto: niepotrzebny TID

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
    printf("[P] Pasażer %ld -> kontrola bagażu (VIP=%d). Waga=%d\n",
           pasazerNum, isVIP, pasazer_bagaz); // Zmieniono: usunięto TID
    sleep(bagaz_checktime);

    if (pasazer_bagaz > M) {
        printf("[P] Pasażer %ld ODRZUCONY (bagaż=%d > %d)\n",
               pasazerNum, pasazer_bagaz, M); // Zmieniono: usunięto TID
        sem_post(&bagaz_wagaSem);

        pthread_mutex_lock(&mutex);
        licznik_pasazer++;
        if (licznik_pasazer == N || (capacityNormal == 0 && capacityVip == 0)) {
            pthread_cond_signal(&samolotCond);
        }
        pthread_mutex_unlock(&mutex);

        return NULL;
    } else {
        printf("[P] Pasażer %ld bagaż OK (%d <= %d)\n",
               pasazerNum, pasazer_bagaz, M); // Zmieniono: usunięto TID
        sem_post(&bagaz_wagaSem);
    }

    // Ustalenie płci pasażera (np. do rozdziału w stanowiskach)
    char plec = ((rand_r(&seed) % 2) == 0) ? 'K' : 'M';
    printf("[P] Pasażer %ld -> kontrola bezpieczeństwa, płeć=%c\n",
           pasazerNum, plec); // Zmieniono: usunięto TID

    // Osobna kontrola:
    if (isVIP) {
        kontrola_bezpieczenstwa_vip(pasazerNum, plec);
    } else {
        kontrola_bezpieczenstwa(pasazerNum, plec);
    }

    // Sprawdzenie, czy boarding został zatrzymany
    if (stopBoarding) {
        printf("[P] Pasażer %ld BOARDING ZATRZYMANY.\n", pasazerNum); // Zmieniono: usunięto TID
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
        printf("[P] Pasażer %ld (VIP) próbuje wejść na schody VIP.\n", pasazerNum); // Zmieniono: usunięto TID
        sem_wait(&stairsSemVip);
        printf("[P] Pasażer %ld (VIP) przechodzi przez schody VIP.\n", pasazerNum); // Zmieniono: usunięto TID
        
        // Aktualizacja liczby pasażerów na schodach
        pthread_mutex_lock(&stairsMutex);
        passengers_on_stairs++;
        pthread_mutex_unlock(&stairsMutex);

        // Symulacja przejścia przez schody
        sleep(1);

        pthread_mutex_lock(&stairsMutex);
        passengers_on_stairs--;
        pthread_cond_broadcast(&stairsCond);
        pthread_mutex_unlock(&stairsMutex);

        printf("[P] Pasażer %ld (VIP) dotarł do samolotu.\n", pasazerNum); // Zmieniono: usunięto TID
        sem_post(&stairsSemVip);
    } else {
        // Pasażer zwykły korzysta z stairsSemNormal
        printf("[P] Pasażer %ld (Zwykły) próbuje wejść na schody normalne.\n", pasazerNum); // Zmieniono: usunięto TID
        sem_wait(&stairsSemNormal);
        printf("[P] Pasażer %ld (Zwykły) przechodzi przez schody normalne.\n", pasazerNum); // Zmieniono: usunięto TID
        
        // Aktualizacja liczby pasażerów na schodach
        pthread_mutex_lock(&stairsMutex);
        passengers_on_stairs++;
        pthread_mutex_unlock(&stairsMutex);

        // Symulacja przejścia przez schody
        sleep(1);

        pthread_mutex_lock(&stairsMutex);
        passengers_on_stairs--;
        pthread_cond_broadcast(&stairsCond);
        pthread_mutex_unlock(&stairsMutex);

        printf("[P] Pasażer %ld (Zwykły) dotarł do samolotu.\n", pasazerNum); // Zmieniono: usunięto TID
        sem_post(&stairsSemNormal);
    }

    // Próba wejścia do samolotu (schody pasażerskie):
    pthread_mutex_lock(&mutex);
    pid_t airplane_pid = getpid(); // PID samolotu

    if (isVIP) {
        // VIP-y korzystają z capacityVip
        if (capacityVip > 0) {
            capacityVip--;
            printf("[P] Pasażer %ld WSZEDŁ do samolotu (VIP) PID=%d. Pozostało %d VIP miejsc.\n",
                   pasazerNum, airplane_pid, capacityVip); // Zmieniono: usunięto TID
        } else {
            printf("[P] Pasażer %ld NIE WSZEDŁ (brak VIP miejsc) PID=%d\n",
                   pasazerNum, airplane_pid); // Zmieniono: usunięto TID
        }
    } else {
        // Pasażer zwykły
        if (capacityNormal > 0) {
            capacityNormal--;
            printf("[P] Pasażer %ld WSZEDŁ do samolotu (Zwykły) PID=%d. Pozostało %d zwykłych miejsc.\n",
                   pasazerNum, airplane_pid, capacityNormal); // Zmieniono: usunięto TID
        } else {
            printf("[P] Pasażer %ld NIE WSZEDŁ (brak zwykłych miejsc) PID=%d\n",
                   pasazerNum, airplane_pid); // Zmieniono: usunięto TID
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
