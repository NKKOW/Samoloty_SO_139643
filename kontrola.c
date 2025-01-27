#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>
#include "kontrola.h"
#include "pasazer.h"
#include "global.h"

StanowiskoBezp stanowiska[3] = {
    {0, '\0'},
    {0, '\0'},
    {0, '\0'}
};

StanowiskoBezp vipStanowiska[3] = {
    {0, '\0'},
    {0, '\0'},
    {0, '\0'}
};

pthread_mutex_t stanowiskoMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t stanowiskoCond   = PTHREAD_COND_INITIALIZER;

pthread_mutex_t vipStanowiskoMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  vipStanowiskoCond  = PTHREAD_COND_INITIALIZER;

// W celach pokazowych tworzymy FIFO, by logować przejścia przez kontrolę
static const char* KONTROLA_FIFO = "kontrola_fifo_REG";
static const char* KONTROLA_FIFO_VIP    = "kontrola_fifo_vip";

// Funkcja pomocnicza: zapis do FIFO
static void log_to_fifo(const char* path, const char* msg) {
    // Sprawdzenie, czy FIFO istnieje, jeśli nie, to tworzymy
    if (access(path, F_OK) == -1) {
        if (mkfifo(path, 0666) == -1 && errno != EEXIST) {
            perror("Kontrola: blad tworzenia FIFO");
            return;
        }
    }

    int fd = open(path, O_WRONLY | O_NONBLOCK);
    if (fd >= 0) {
        write(fd, msg, strlen(msg));
        close(fd);
    } else {
        // Możliwe, że brak czytelnika, logowanie jest opcjonalne
        // Możesz dodać inny sposób logowania, np. plik
    }
}

// ---------------------------------------------
// Kontrola zwykła z mechanizmem skip_count
// ---------------------------------------------
void kontrola_bezpieczenstwa(long pasazer_id, char plec)
{
    // Logowanie do FIFO
    // Funkcja log_to_fifo zajmuje się tworzeniem FIFO, jeśli nie istnieje
    pthread_mutex_lock(&stanowiskoMutex);

    int skip_count = 0;

    while(1) {
        int wolneStanowisko = -1;
        // Szukamy wolnego stanowiska (occupied < 2), a jeśli occupied==1, to sprawdzamy płeć
        for (int i = 0; i < 3; i++) {
            if (stanowiska[i].occupied < 2) {
                if (stanowiska[i].occupied == 0 ||
                    stanowiska[i].plec == plec) {
                    wolneStanowisko = i;
                    break;
                }
            }
        }

        if (wolneStanowisko >= 0) {
            // Rezerwujemy stanowisko
            stanowiska[wolneStanowisko].occupied++;
            if (stanowiska[wolneStanowisko].occupied == 1) {
                stanowiska[wolneStanowisko].plec = plec;
            }
            pthread_mutex_unlock(&stanowiskoMutex);

            // Log do FIFO (zwykłe)
            char buf[128];
            snprintf(buf, sizeof(buf),"Pasażer %ld (plec=%c) -> stanowisko %d [NORMAL]\n", pasazer_id, plec, wolneStanowisko);
            log_to_fifo(KONTROLA_FIFO, buf);

            // Symulacja kontroli
            int kontrola_checktime = (rand() % 3) + 1;
            sleep(kontrola_checktime);

            // Losowa szansa na przedmiot zabroniony (10%)
            int niebezpieczny = (rand() % 10 == 0);
            if (niebezpieczny) {
                pthread_mutex_lock(&stanowiskoMutex);
                stanowiska[wolneStanowisko].occupied--;
                if (stanowiska[wolneStanowisko].occupied == 0) {
                    stanowiska[wolneStanowisko].plec = '\0';
                }
                pthread_cond_broadcast(&stanowiskoCond);
                pthread_mutex_unlock(&stanowiskoMutex);

                // Zwiększamy licznik pasażerów i sygnalizujemy
                pthread_mutex_lock(&mutex);
                licznik_pasazer++;
                if (licznik_pasazer == N || (capacityNormal == 0 && capacityVip == 0)) {
                    pthread_cond_signal(&samolotCond);
                }
                pthread_mutex_unlock(&mutex);

                printf("[K] Pasazer %ld odrzucony w kontroli NORMAL (przedmiot zabroniony)\n",
                       pasazer_id);
                return;
            }

            // Zwalniamy stanowisko
            pthread_mutex_lock(&stanowiskoMutex);
            stanowiska[wolneStanowisko].occupied--;
            if (stanowiska[wolneStanowisko].occupied == 0) {
                stanowiska[wolneStanowisko].plec = '\0';
            }

            printf("[K] Pasazer %ld (plec=%c) zwalnia stanowisko %d (NORMAL). occ=%d\n",
                   pasazer_id, plec, wolneStanowisko,
                   stanowiska[wolneStanowisko].occupied);

            pthread_cond_broadcast(&stanowiskoCond);
            pthread_mutex_unlock(&stanowiskoMutex);
            break;
        } else {
            // Nie ma wolnego stanowiska
            // Sprawdzenie, czy możemy przepuścić kolejną osobę
            if (skip_count < MAX_SKIP) {
                // Przepuszczenie osoby z kolejki
                // W praktyce, wymagałoby to zarządzania kolejką pasażerów.
                // Tutaj symulujemy to przez zwiększenie skip_count i czekanie.

                // Zwiększamy skip_count tylko jeśli osoba za nami nie jest tej samej płci
                // Jednak w tej symulacji nie mamy informacji o kolejce, więc zakładamy, że możemy przepuścić
                // Maksymalnie 3 razy
                skip_count++;
                printf("[K] Pasazer %ld przepuszcza %d osobę/osoby z kolejki (NORMAL)\n",
                       pasazer_id, skip_count);
                pthread_cond_wait(&stanowiskoCond, &stanowiskoMutex);
            } else {
                // Przekroczono maksymalną liczbę przepuszczeń
                // Blokujemy przepuszczanie i czekamy aż stanowisko całkowicie się zwolni
                printf("[K] Pasazer %ld przekroczył limit przepuszczeń. Blokuje przepuszczanie (NORMAL)\n",
                       pasazer_id);
                // Możemy tutaj ustawić flagę blokady, jeśli chcemy uniemożliwić innym pasażerom przepuszczanie
                // W tej symulacji po prostu czekamy, aż stanowisko się zwolni
                pthread_cond_wait(&stanowiskoCond, &stanowiskoMutex);

                // Resetujemy skip_count po zwolnieniu stanowiska
                skip_count = 0;
            }
        }
    }
}


// ---------------------------------------------
// Kontrola VIP z osobnymi stanowiskami
// ---------------------------------------------
void kontrola_bezpieczenstwa_vip(long pasazer_id, char plec)
{
    // Logowanie do FIFO
    // Funkcja log_to_fifo zajmuje się tworzeniem FIFO, jeśli nie istnieje
    pthread_mutex_lock(&vipStanowiskoMutex);

    int skip_count = 0; // Dodajemy skip_count dla VIP

    while(1) {
        int wolneStanowisko = -1;
        for (int i = 0; i < 3; i++) {
            if (vipStanowiska[i].occupied < 2) {
                // VIP może się dzielić stanowiskiem tylko z tą samą płcią
                if (vipStanowiska[i].occupied == 0 ||
                    vipStanowiska[i].plec == plec) {
                    wolneStanowisko = i;
                    break;
                }
            }
        }

        if (wolneStanowisko >= 0) {
            vipStanowiska[wolneStanowisko].occupied++;
            if (vipStanowiska[wolneStanowisko].occupied == 1) {
                vipStanowiska[wolneStanowisko].plec = plec;
            }
            pthread_mutex_unlock(&vipStanowiskoMutex);

            // Log do FIFO (VIP)
            char buf[128];
            snprintf(buf, sizeof(buf),
                     "Pasażer %ld (plec=%c) -> stanowisko %d [VIP]\n",
                     pasazer_id, plec, wolneStanowisko);
            log_to_fifo(KONTROLA_FIFO_VIP, buf);

            // Symulacja kontroli
            int kontrola_checktime = (rand() % 3) + 1;
            sleep(kontrola_checktime);

            // Losowa szansa na przedmiot zabroniony (10%)
            int niebezpieczny = (rand() % 10 == 0);
            if (niebezpieczny) {
                pthread_mutex_lock(&vipStanowiskoMutex);
                vipStanowiska[wolneStanowisko].occupied--;
                if (vipStanowiska[wolneStanowisko].occupied == 0) {
                    vipStanowiska[wolneStanowisko].plec = '\0';
                }
                pthread_cond_broadcast(&vipStanowiskoCond);
                pthread_mutex_unlock(&vipStanowiskoMutex);

                // Zwiększamy licznik pasażerów i sygnalizujemy
                pthread_mutex_lock(&mutex);
                licznik_pasazer++;
                if (licznik_pasazer == N || (capacityNormal == 0 && capacityVip == 0)) {
                    pthread_cond_signal(&samolotCond);
                }
                pthread_mutex_unlock(&mutex);

                printf("[K] Pasazer %ld odrzucony w kontroli VIP (przedmiot zabroniony)\n",
                       pasazer_id);
                return;
            }

            // Zwalniamy stanowisko
            pthread_mutex_lock(&vipStanowiskoMutex);
            vipStanowiska[wolneStanowisko].occupied--;
            if (vipStanowiska[wolneStanowisko].occupied == 0) {
                vipStanowiska[wolneStanowisko].plec = '\0';
            }

            printf("[K] Pasazer %ld (plec=%c) zwalnia stanowisko %d (VIP). occ=%d\n",
                   pasazer_id, plec, wolneStanowisko,
                   vipStanowiska[wolneStanowisko].occupied);

            pthread_cond_broadcast(&vipStanowiskoCond);
            pthread_mutex_unlock(&vipStanowiskoMutex);
            break;
        } else {
            // Nie ma wolnego stanowiska VIP
            // Implementujemy mechanizm przepuszczania dla VIP podobnie jak dla normalnych pasażerów

            if (skip_count < MAX_SKIP) {
                // Przepuszczenie osoby z kolejki VIP
                skip_count++;
                printf("[K] Pasazer VIP %ld przepuszcza %d osobę/osoby z kolejki (VIP)\n",
                       pasazer_id, skip_count);
                pthread_cond_wait(&vipStanowiskoCond, &vipStanowiskoMutex);
            } else {
                // Przekroczono maksymalną liczbę przepuszczeń VIP
                printf("[K] Pasazer VIP %ld przekroczył limit przepuszczeń. Blokuje przepuszczanie (VIP)\n",
                       pasazer_id);
                // Blokujemy przepuszczanie VIP do czasu zwolnienia stanowiska
                pthread_cond_wait(&vipStanowiskoCond, &vipStanowiskoMutex);

                // Resetujemy skip_count po zwolnieniu stanowiska
                skip_count = 0;
            }
        }
    }
}
