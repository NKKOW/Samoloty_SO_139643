#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include "kontrola.h"
#include "pasazer.h"
#include "global.h"

// Stanowiska
StanowiskoBezp stanowiska[3] = {
    {0, '\0'},
    {0, '\0'},
    {0, '\0'}
};

pthread_mutex_t stanowsikoMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t stanowiskoCond   = PTHREAD_COND_INITIALIZER;

// W celach pokazowych tworzymy FIFO, by logować przejścia przez kontrolę
// (można też zwykły plik open/write/close).
static const char* KONTROLA_FIFO = "kontrola_fifo";

static void init_fifo_if_needed() {
    // Tworzymy FIFO jeśli nie istnieje
    mkfifo(KONTROLA_FIFO, 0666);
}

// Funkcja pomocnicza: zapis do FIFO
static void log_to_fifo(const char* msg) {
    int fd = open(KONTROLA_FIFO, O_WRONLY | O_NONBLOCK);
    if (fd >= 0) {
        write(fd, msg, strlen(msg));
        close(fd);
    }
}

// Kontrola bezpieczeństwa z uwzględnieniem VIP i możliwości przepuszczenia 3 osób
void kontrola_bezpieczenstwa(long pasazer_id, char plec, int isVIP){
    init_fifo_if_needed();

    pthread_mutex_lock(&stanowsikoMutex);

    int skip_count = 0;

    while(1) {
        int wolneStanowisko = -1;
        for (int i = 0; i < 3; i++) {
            // Jeżeli stanowisko nie jest zapchane
            // i (albo puste, albo ta sama płeć) -> pasujemy
            if (stanowiska[i].occupied < 2) {
                if (stanowiska[i].occupied == 0 ||
                    stanowiska[i].plec == plec) {
                    wolneStanowisko = i;
                    break;
                }
            }
        }
        // VIP zawsze wskakuje natychmiast, bez czekania
        if (isVIP) {
            for (int i = 0; i < 3; i++) {
                // jeżeli jest jakiekolwiek wolne stanowisko
                if (stanowiska[i].occupied < 2 &&
                    (stanowiska[i].occupied == 0 ||
                     stanowiska[i].plec == plec)) {
                    wolneStanowisko = i;
                    break;
                }
            }
        }

        if (wolneStanowisko >= 0) {
            // rezerwujemy stanowisko
            stanowiska[wolneStanowisko].occupied++;
            if (stanowiska[wolneStanowisko].occupied == 1) {
                stanowiska[wolneStanowisko].plec = plec;
            }
            pthread_mutex_unlock(&stanowsikoMutex);

            // log do FIFO
            char buf[128];
            snprintf(buf, sizeof(buf),
                     "Pasażer %ld (plec=%c, VIP=%d) -> stanowisko %d\n",
                     pasazer_id, plec, isVIP, wolneStanowisko);
            log_to_fifo(buf);

            // Symulacja kontroli
            int kontrola_checktime = (rand() % 3) + 1;
            sleep(kontrola_checktime);

            // Losowa szansa na przedmiot zabroniony (10% = co 30%? W oryg. 1/30 to ~3.3%)
            int niebezpieczny = (rand() % 10 == 0);
            if (niebezpieczny) {
                pthread_mutex_lock(&stanowsikoMutex);
                stanowiska[wolneStanowisko].occupied--;
                if (stanowiska[wolneStanowisko].occupied == 0) {
                    stanowiska[wolneStanowisko].plec = '\0';
                }
                pthread_cond_broadcast(&stanowiskoCond);
                pthread_mutex_unlock(&stanowsikoMutex);

                // Zwiększamy licznik pasażerów i sygnalizujemy ewentualne
                pthread_mutex_lock(&mutex);
                licznik_pasazer++;
                if (licznik_pasazer == N || capacity == 0) {
                    pthread_cond_signal(&samolotCond);
                }
                pthread_mutex_unlock(&mutex);

                printf("[K] Pasazer %ld odrzucony w kontroli (przedmiot zabroniony)\n",
                       pasazer_id);
                return;
            }

            // Zwolnienie stanowiska
            pthread_mutex_lock(&stanowsikoMutex);
            stanowiska[wolneStanowisko].occupied--;
            if (stanowiska[wolneStanowisko].occupied == 0) {
                stanowiska[wolneStanowisko].plec = '\0';
            }

            printf("[K] Pasazer %ld (plec %c, VIP=%d) zwalnia stanowisko %d (occ=%d)\n",
                   pasazer_id, plec, isVIP, wolneStanowisko,
                   stanowiska[wolneStanowisko].occupied);

            pthread_cond_broadcast(&stanowiskoCond);
            pthread_mutex_unlock(&stanowsikoMutex);
            break;
        } else {
            // nie ma wolnego stanowiska
            // pasażer sprawdza czy może jeszcze "przepuścić" kogoś
            if (!isVIP && skip_count < MAX_SKIP) {
                // przepuszczamy (de facto czekamy, ale skip_count++)
                skip_count++;
                pthread_cond_wait(&stanowiskoCond, &stanowsikoMutex);
            } else {
                // frustruje się, rezygnuje
                pthread_mutex_unlock(&stanowsikoMutex);

                // Inkrementujemy licznik pasażerów globalnie
                pthread_mutex_lock(&mutex);
                licznik_pasazer++;
                if (licznik_pasazer == N || capacity == 0) {
                    pthread_cond_signal(&samolotCond);
                }
                pthread_mutex_unlock(&mutex);

                printf("[K] Pasazer %ld zbyt dlugo czekal (skip=%d), REZYGNACJA\n",
                       pasazer_id, skip_count);
                return;
            }
        }
    }
}
