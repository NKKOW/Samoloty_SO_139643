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

// Inicjalizacja stanowisk
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

// Definicja FIFO
static PassengerQueue passengerQueue;
static PassengerQueue vipPassengerQueue;

// Inicjalizacja kolejki FIFO
__attribute__((constructor))
static void initialize_queues() {
    init_queue(&passengerQueue);
    init_queue(&vipPassengerQueue);
}

// W celach pokazowych tworzymy FIFO, by logować przejścia przez kontrolę
static const char* KONTROLA_FIFO = "kontrola_fifo_REG";
static const char* KONTROLA_FIFO_VIP = "kontrola_fifo_vip";

// Funkcja pomocnicza: zapis do FIFO
static void log_to_fifo(const char* path, const char* msg) {
    // Sprawdzenie, czy FIFO istnieje, jeśli nie, to tworzymy
    if (access(path, F_OK) == -1) {
        if (mkfifo(path, 0666) == -1 && errno != EEXIST) {
            perror("Kontrola: błąd tworzenia FIFO");
            return;
        }
    }

    int fd = open(path, O_WRONLY | O_NONBLOCK);
    if (fd >= 0) {
        write(fd, msg, strlen(msg));
        close(fd);
    } else {
        // logowanie opcjonalne
    }
}

// Inicjalizacja kolejki
void init_queue(PassengerQueue* queue) {
    queue->front = NULL;
    queue->rear  = NULL;
    pthread_mutex_init(&queue->mutex, NULL);
}

// Sprawdzenie, czy kolejka jest pusta
int is_queue_empty(PassengerQueue* queue) {
    return (queue->front == NULL);
}

// Enqueue pasażera
void enqueue_passenger(PassengerQueue* queue, PassengerNode* node) {
    pthread_mutex_lock(&queue->mutex);
    node->next = NULL;
    if (queue->rear == NULL) {
        queue->front = node;
        queue->rear  = node;
    } else {
        queue->rear->next = node;
        queue->rear       = node;
    }
    pthread_mutex_unlock(&queue->mutex);
}

// Dequeue pasażera
PassengerNode* dequeue_passenger(PassengerQueue* queue) {
    pthread_mutex_lock(&queue->mutex);
    PassengerNode* node = queue->front;
    if (node != NULL) {
        queue->front = node->next;
        if (queue->front == NULL) {
            queue->rear = NULL;
        }
    }
    pthread_mutex_unlock(&queue->mutex);
    return node;
}

// Znajdź i usuń pasażera o określonej płci z kolejki
PassengerNode* find_and_remove_passenger(PassengerQueue* queue, char plec) {
    pthread_mutex_lock(&queue->mutex);
    PassengerNode* prev    = NULL;
    PassengerNode* current = queue->front;

    while (current != NULL) {
        if (current->plec == plec && !current->has_been_skipped) {
            // Usuń z kolejki
            if (prev == NULL) {
                // Pierwszy element
                queue->front = current->next;
                if (queue->front == NULL) {
                    queue->rear = NULL;
                }
            } else {
                prev->next = current->next;
                if (current == queue->rear) {
                    queue->rear = prev;
                }
            }
            pthread_mutex_unlock(&queue->mutex);
            return current;
        }
        prev    = current;
        current = current->next;
    }

    pthread_mutex_unlock(&queue->mutex);
    return NULL; // Nie znaleziono
}

// Funkcja kontroli bezpieczeństwa dla pasażerów normalnych
void kontrola_bezpieczenstwa(long pasazer_id, char plec)
{
    // Pobranie PID samolotu
    pid_t airplane_pid = getpid();

    // Stworzenie nowego węzła pasażera
    PassengerNode* node = (PassengerNode*)malloc(sizeof(PassengerNode));
    node->pasazer_id        = pasazer_id;
    node->plec              = plec;
    node->skip_count        = 0;
    node->has_been_skipped  = false; 
    pthread_cond_init(&node->cond, NULL);
    node->next = NULL;

    // Dodanie pasażera do kolejki
    enqueue_passenger(&passengerQueue, node);
    printf("[K][PID=%d] Pasażer %ld dodany do kolejki NORMAL (płeć=%c)\n",
           airplane_pid, pasazer_id, plec);

    pthread_mutex_lock(&stanowiskoMutex);

    while (1) {
        // Sprawdzenie, czy pasażer jest na początku kolejki
        if (passengerQueue.front == node) {
            // Szukanie wolnego stanowiska
            int wolneStanowisko = -1;
            for (int i = 0; i < 3; i++) {
                if (stanowiska[i].occupied < 2) {
                    // Albo wolne, albo ta sama płeć
                    if (stanowiska[i].occupied == 0 ||
                        stanowiska[i].plec == plec)
                    {
                        wolneStanowisko = i;
                        break;
                    }
                }
            }

            if (wolneStanowisko >= 0) {
                // Rezerwacja stanowiska
                stanowiska[wolneStanowisko].occupied++;
                if (stanowiska[wolneStanowisko].occupied == 1) {
                    stanowiska[wolneStanowisko].plec = plec;
                }

                // Usunięcie pasażera z kolejki
                dequeue_passenger(&passengerQueue);

                pthread_mutex_unlock(&stanowiskoMutex);

                // Log do FIFO (zwykłe)
                char buf[128];
                snprintf(buf, sizeof(buf),
                         "Pasażer %ld (płeć=%c) -> stanowisko %d [NORMAL]\n",
                         pasazer_id, plec, wolneStanowisko);
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
                    // ---> USUNIĘTO odwołanie do N
                    if ((capacityNormal == 0 && capacityVip == 0)) {
                        pthread_cond_signal(&samolotCond);
                    }
                    pthread_mutex_unlock(&mutex);

                    printf("[K][PID=%d] Pasażer %ld odrzucony w kontroli NORMAL (przedmiot zabroniony)\n",
                           airplane_pid, pasazer_id);
                    pthread_cond_destroy(&node->cond);
                    free(node);
                    return;
                }

                // Kontynuacja procesu kontroli
                printf("[K][PID=%d] Pasażer %ld (płeć=%c) zwalnia stanowisko %d (NORMAL). occ=%d\n",
                       airplane_pid, pasazer_id, plec, wolneStanowisko,
                       stanowiska[wolneStanowisko].occupied);

                pthread_mutex_lock(&stanowiskoMutex);
                stanowiska[wolneStanowisko].occupied--;
                if (stanowiska[wolneStanowisko].occupied == 0) {
                    stanowiska[wolneStanowisko].plec = '\0';
                }

                pthread_cond_broadcast(&stanowiskoCond);
                pthread_mutex_unlock(&stanowiskoMutex);
                break;
            }

        } else {
            // Pasażer nie jest na początku kolejki, sprawdzenie możliwości przepuszczenia
            // (np. logika "kobiety przed mężczyznami")

            PassengerNode* target = find_and_remove_passenger(&passengerQueue, 'K');
            if (target && target != node) {
                target->has_been_skipped = true;
                printf("[K][PID=%d] Pasażer %ld przepuszcza pasażera %ld (Kobieta przechodzi przed mężczyzną)\n",
                       airplane_pid, pasazer_id, target->pasazer_id);
                pthread_cond_signal(&target->cond);
            } else {
                if (node->skip_count < MAX_SKIP) {
                    node->skip_count++;
                    printf("[K][PID=%d] Pasażer %ld przepuszcza %d osobę/osoby z kolejki (NORMAL)\n",
                           airplane_pid, pasazer_id, node->skip_count);
                    pthread_cond_wait(&stanowiskoCond, &stanowiskoMutex);
                } else {
                    // Przekroczono maksymalną liczbę przepuszczeń
                    printf("[K][PID=%d] Pasażer %ld przekroczył limit przepuszczeń. "
                           "Czeka na wolne stanowisko (NORMAL)\n",
                           airplane_pid, pasazer_id);
                    pthread_cond_wait(&stanowiskoCond, &stanowiskoMutex);
                    // Resetujemy skip_count po zwolnieniu stanowiska
                    node->skip_count = 0;
                }
            }
        }
    }

    pthread_mutex_unlock(&stanowiskoMutex);
    pthread_cond_destroy(&node->cond);
    free(node);
}

// Funkcja kontroli bezpieczeństwa dla pasażerów VIP
void kontrola_bezpieczenstwa_vip(long pasazer_id, char plec)
{
    // Pobranie PID samolotu
    pid_t airplane_pid = getpid();

    // Stworzenie nowego węzła pasażera VIP
    PassengerNode* node = (PassengerNode*)malloc(sizeof(PassengerNode));
    node->pasazer_id       = pasazer_id;
    node->plec             = plec;
    node->skip_count       = 0;
    node->has_been_skipped = false;
    pthread_cond_init(&node->cond, NULL);
    node->next = NULL;

    // Dodanie pasażera do kolejki VIP
    enqueue_passenger(&vipPassengerQueue, node);
    printf("[K][PID=%d] Pasażer VIP %ld dodany do kolejki VIP (płeć=%c)\n",
           airplane_pid, pasazer_id, plec);

    pthread_mutex_lock(&vipStanowiskoMutex);

    while (1) {
        // Sprawdzenie, czy pasażer jest na początku kolejki VIP
        if (vipPassengerQueue.front == node) {
            // Szukanie wolnego stanowiska VIP
            int wolneStanowisko = -1;
            for (int i = 0; i < 3; i++) {
                if (vipStanowiska[i].occupied < 2) {
                    if (vipStanowiska[i].occupied == 0 ||
                        vipStanowiska[i].plec == plec)
                    {
                        wolneStanowisko = i;
                        break;
                    }
                }
            }

            if (wolneStanowisko >= 0) {
                // Rezerwacja stanowiska VIP
                vipStanowiska[wolneStanowisko].occupied++;
                if (vipStanowiska[wolneStanowisko].occupied == 1) {
                    vipStanowiska[wolneStanowisko].plec = plec;
                }

                // Usunięcie pasażera z kolejki VIP
                dequeue_passenger(&vipPassengerQueue);

                pthread_mutex_unlock(&vipStanowiskoMutex);

                // Log do FIFO (VIP)
                char buf[128];
                snprintf(buf, sizeof(buf),
                         "Pasażer %ld (płeć=%c) -> stanowisko %d [VIP]\n",
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
                    // ---> USUNIĘTO odwołanie do N
                    if ((capacityNormal == 0 && capacityVip == 0)) {
                        pthread_cond_signal(&samolotCond);
                    }
                    pthread_mutex_unlock(&mutex);

                    printf("[K][PID=%d] Pasażer VIP %ld odrzucony w kontroli VIP (przedmiot zabroniony)\n",
                           airplane_pid, pasazer_id);
                    pthread_cond_destroy(&node->cond);
                    free(node);
                    return;
                }

                // Kontynuacja procesu kontroli VIP
                printf("[K][PID=%d] Pasażer VIP %ld (płeć=%c) zwalnia stanowisko %d (VIP). occ=%d\n",
                       airplane_pid, pasazer_id, plec, wolneStanowisko,
                       vipStanowiska[wolneStanowisko].occupied);

                pthread_mutex_lock(&vipStanowiskoMutex);
                vipStanowiska[wolneStanowisko].occupied--;
                if (vipStanowiska[wolneStanowisko].occupied == 0) {
                    vipStanowiska[wolneStanowisko].plec = '\0';
                }

                pthread_cond_broadcast(&vipStanowiskoCond);
                pthread_mutex_unlock(&vipStanowiskoMutex);
                break;
            }

        } else {
            // Pasażer VIP nie jest na początku kolejki, sprawdzenie możliwości przepuszczenia
            // (np. przepuszczanie kobiet przed mężczyznami)

            PassengerNode* target = find_and_remove_passenger(&vipPassengerQueue, 'K');
            if (target && target != node) {
                // Znaleziono kobietę w kolejce, przepuszczenie jej
                target->has_been_skipped = true;
                printf("[K][PID=%d] Pasażer VIP %ld przepuszcza pasażera %ld (Kobieta przechodzi przed mężczyzną)\n",
                       airplane_pid, pasazer_id, target->pasazer_id);
                pthread_cond_signal(&target->cond);
            } else {
                // Brak kobiety do przepuszczenia lub pasażer już na czele
                if (node->skip_count < MAX_SKIP) {
                    node->skip_count++;
                    printf("[K][PID=%d] Pasażer VIP %ld przepuszcza %d osobę/osoby z kolejki (VIP)\n",
                           airplane_pid, pasazer_id, node->skip_count);
                    pthread_cond_wait(&vipStanowiskoCond, &vipStanowiskoMutex);
                } else {
                    // Przekroczono maksymalną liczbę przepuszczeń VIP
                    printf("[K][PID=%d] Pasażer VIP %ld przekroczył limit przepuszczeń. "
                           "Czeka na wolne stanowisko (VIP)\n",
                           airplane_pid, pasazer_id);
                    pthread_cond_wait(&vipStanowiskoCond, &vipStanowiskoMutex);
                    // Resetujemy skip_count po zwolnieniu stanowiska
                    node->skip_count = 0;
                }
            }
        }
    }

    pthread_mutex_unlock(&vipStanowiskoMutex);
    pthread_cond_destroy(&node->cond);
    free(node);
}
