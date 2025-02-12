#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/stat.h>
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
pthread_cond_t vipStanowiskoCond   = PTHREAD_COND_INITIALIZER;

static PassengerQueue passengerQueue;
static PassengerQueue vipPassengerQueue;

__attribute__((constructor))
static void initialize_queues() {
    init_queue(&passengerQueue);
    init_queue(&vipPassengerQueue);
}

static const char* KONTROLA_FIFO = "kontrola_fifo_REG";
static const char* KONTROLA_FIFO_VIP = "kontrola_fifo_vip";

static void log_to_fifo(const char* path, const char* msg) {
    if (access(path, F_OK) == -1) {
        if (mkfifo(path, 0666) == -1 && errno != EEXIST) {
            perror("Kontrola: mkfifo");
            return;
        }
    }
    int fd = open(path, O_WRONLY | O_NONBLOCK);
    if (fd >= 0) {
        write(fd, msg, strlen(msg));
        close(fd);
    }
}

void init_queue(PassengerQueue* queue) {
    queue->front = NULL;
    queue->rear = NULL;
    pthread_mutex_init(&queue->mutex, NULL);
}

int is_queue_empty(PassengerQueue* queue) {
    return (queue->front == NULL);
}

void enqueue_passenger(PassengerQueue* queue, PassengerNode* node) {
    pthread_mutex_lock(&queue->mutex);
    node->next = NULL;
    if (queue->rear == NULL) {
        queue->front = node;
        queue->rear = node;
    } else {
        queue->rear->next = node;
        queue->rear = node;
    }
    pthread_mutex_unlock(&queue->mutex);
}

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

PassengerNode* find_and_remove_passenger(PassengerQueue* queue, char plec) {
    pthread_mutex_lock(&queue->mutex);
    PassengerNode* prev = NULL;
    PassengerNode* current = queue->front;
    while (current != NULL) {
        if (current->plec == plec && !current->has_been_skipped) {
            if (prev == NULL) {
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
        prev = current;
        current = current->next;
    }
    pthread_mutex_unlock(&queue->mutex);
    return NULL;
}

void kontrola_bezpieczenstwa(long pasazer_id, char plec) {
    pid_t airplane_pid = getpid();
    PassengerNode* node = (PassengerNode*)malloc(sizeof(PassengerNode));
    node->pasazer_id = pasazer_id;
    node->plec = plec;
    node->skip_count = 0;
    node->has_been_skipped = false;
    pthread_cond_init(&node->cond, NULL);
    node->next = NULL;

    enqueue_passenger(&passengerQueue, node);
    printf("[K][PID=%d] Pasażer %ld w kolejce NORMAL (płeć=%c)\n", airplane_pid, pasazer_id, plec);

    pthread_mutex_lock(&stanowiskoMutex);
    while (1) {
        if (passengerQueue.front == node) {
            int wolneStanowisko = -1;
            for (int i = 0; i < 3; i++) {
                if (stanowiska[i].occupied < 2) {
                    if (stanowiska[i].occupied == 0 || stanowiska[i].plec == plec) {
                        wolneStanowisko = i;
                        break;
                    }
                }
            }
            if (wolneStanowisko >= 0) {
                stanowiska[wolneStanowisko].occupied++;
                if (stanowiska[wolneStanowisko].occupied == 1) {
                    stanowiska[wolneStanowisko].plec = plec;
                }
                dequeue_passenger(&passengerQueue);
                pthread_mutex_unlock(&stanowiskoMutex);

                char buf[128];
                snprintf(buf, sizeof(buf), "Pasażer %ld (płeć=%c) -> stanowisko %d [NORMAL]\n", pasazer_id, plec, wolneStanowisko);
                log_to_fifo(KONTROLA_FIFO, buf);

                int kontrola_checktime = (rand() % 3) + 1;
                sleep(kontrola_checktime);
                int niebezpieczny = (rand() % 10 == 0);
                if (niebezpieczny) {
                    pthread_mutex_lock(&stanowiskoMutex);
                    stanowiska[wolneStanowisko].occupied--;
                    if (stanowiska[wolneStanowisko].occupied == 0) {
                        stanowiska[wolneStanowisko].plec = '\0';
                    }
                    pthread_cond_broadcast(&stanowiskoCond);
                    pthread_mutex_unlock(&stanowiskoMutex);

                    pthread_mutex_lock(&mutex);
                    pthread_mutex_unlock(&mutex);

                    printf("[K][PID=%d] Pasażer %ld odrzucony (przedmiot).\n", airplane_pid, pasazer_id);
                    pthread_cond_destroy(&node->cond);
                    free(node);
                    return;
                }
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
            PassengerNode* target = find_and_remove_passenger(&passengerQueue, 'K');
            if (target && target != node) {
                target->has_been_skipped = true;
                printf("[K][PID=%d] Pasażer %ld przepuszcza %ld (K)\n", airplane_pid, pasazer_id, target->pasazer_id);
                pthread_cond_signal(&target->cond);
            } else {
                if (node->skip_count < MAX_SKIP) {
                    node->skip_count++;
                    printf("[K][PID=%d] Pasażer %ld skip %d (NORMAL)\n", airplane_pid, pasazer_id, node->skip_count);
                    pthread_cond_wait(&stanowiskoCond, &stanowiskoMutex);
                } else {
                    printf("[K][PID=%d] Pasażer %ld limit skip. Czeka (NORMAL)\n", airplane_pid, pasazer_id);
                    pthread_cond_wait(&stanowiskoCond, &stanowiskoMutex);
                    node->skip_count = 0;
                }
            }
        }
    }
    pthread_mutex_unlock(&stanowiskoMutex);
    pthread_cond_destroy(&node->cond);
    free(node);
}

void kontrola_bezpieczenstwa_vip(long pasazer_id, char plec) {
    pid_t airplane_pid = getpid();
    PassengerNode* node = (PassengerNode*)malloc(sizeof(PassengerNode));
    node->pasazer_id = pasazer_id;
    node->plec = plec;
    node->skip_count = 0;
    node->has_been_skipped = false;
    pthread_cond_init(&node->cond, NULL);
    node->next = NULL;

    enqueue_passenger(&vipPassengerQueue, node);
    printf("[K][PID=%d] Pasażer VIP %ld w kolejce (płeć=%c)\n", airplane_pid, pasazer_id, plec);

    pthread_mutex_lock(&vipStanowiskoMutex);
    while (1) {
        if (vipPassengerQueue.front == node) {
            int wolneStanowisko = -1;
            for (int i = 0; i < 3; i++) {
                if (vipStanowiska[i].occupied < 2) {
                    if (vipStanowiska[i].occupied == 0 || vipStanowiska[i].plec == plec) {
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
                dequeue_passenger(&vipPassengerQueue);
                pthread_mutex_unlock(&vipStanowiskoMutex);

                char buf[128];
                snprintf(buf, sizeof(buf), "Pasażer %ld (płeć=%c) -> stanowisko %d [VIP]\n", pasazer_id, plec, wolneStanowisko);
                log_to_fifo(KONTROLA_FIFO_VIP, buf);

                int kontrola_checktime = (rand() % 3) + 1;
                sleep(kontrola_checktime);
                int niebezpieczny = (rand() % 10 == 0);
                if (niebezpieczny) {
                    pthread_mutex_lock(&vipStanowiskoMutex);
                    vipStanowiska[wolneStanowisko].occupied--;
                    if (vipStanowiska[wolneStanowisko].occupied == 0) {
                        vipStanowiska[wolneStanowisko].plec = '\0';
                    }
                    pthread_cond_broadcast(&vipStanowiskoCond);
                    pthread_mutex_unlock(&vipStanowiskoMutex);

                    pthread_mutex_lock(&mutex);
                    pthread_mutex_unlock(&mutex);

                    printf("[K][PID=%d] Pasażer VIP %ld odrzucony (przedmiot).\n", airplane_pid, pasazer_id);
                    pthread_cond_destroy(&node->cond);
                    free(node);
                    return;
                }
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
            PassengerNode* target = find_and_remove_passenger(&vipPassengerQueue, 'K');
            if (target && target != node) {
                target->has_been_skipped = true;
                printf("[K][PID=%d] VIP %ld przepuszcza %ld (K)\n", airplane_pid, pasazer_id, target->pasazer_id);
                pthread_cond_signal(&target->cond);
            } else {
                if (node->skip_count < MAX_SKIP) {
                    node->skip_count++;
                    printf("[K][PID=%d] VIP %ld skip %d\n", airplane_pid, pasazer_id, node->skip_count);
                    pthread_cond_wait(&vipStanowiskoCond, &vipStanowiskoMutex);
                } else {
                    printf("[K][PID=%d] VIP %ld limit skip\n", airplane_pid, pasazer_id);
                    pthread_cond_wait(&vipStanowiskoCond, &vipStanowiskoMutex);
                    node->skip_count = 0;
                }
            }
        }
    }
    pthread_mutex_unlock(&vipStanowiskoMutex);
    pthread_cond_destroy(&node->cond);
    free(node);
}
