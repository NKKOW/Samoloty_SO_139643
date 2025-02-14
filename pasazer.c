#define _POSIX_C_SOURCE 200809L
#include "global.h"
#include "pasazer.h"
#include "kontrola.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdint.h>

typedef struct {
    long pasazerNum;
    int isVIP;
    char plec;
} KontrolaArgs;

int bagaz_waga() {
    return (rand() % 13) + 1;
}

void* kontrola_thread_func(void* arg) {
    KontrolaArgs* data = (KontrolaArgs*) arg;
    if (data->isVIP)
        return (void*)(intptr_t)kontrola_bezpieczenstwa_vip(data->pasazerNum, data->plec);
    else
        return (void*)(intptr_t)kontrola_bezpieczenstwa(data->pasazerNum, data->plec);
}

#define WAIT_TIMEOUT 20

int main(int argc, char** argv) {
    if (argc < 5) {
        fprintf(stderr, "Użycie: %s <pasazer_id> <plane_pid> <planeMd> <plane_index>\n", argv[0]);
        exit(1);
    }
    long pasazerNum = atol(argv[1]);
    /* Zmienna plane_pid nie jest używana – przekazujemy ją z dyspozytora,
       więc tutaj pozostawiamy argument, ale nie przypisujemy do zmiennej */
    int planeMd = atoi(argv[3]);
    int plane_index = atoi(argv[4]);

    srand(time(NULL) ^ getpid());
    printf("[P] PID=%d -> Pasażer %ld\n", getpid(), pasazerNum);

    /* Podłączenie do pamięci współdzielonej */
    key_t key = ftok(MSG_QUEUE_PATH, 'S');
    if (key == -1) {
        perror("Pasazer: ftok");
        exit(EXIT_FAILURE);
    }
    int shm_id = shmget(key, sizeof(SharedData), 0666);
    if (shm_id < 0) {
        perror("Pasazer: shmget");
        exit(EXIT_FAILURE);
    }
    SharedData* shm_ptr = (SharedData*) shmat(shm_id, NULL, 0);
    if (shm_ptr == (void*)-1) {
        perror("Pasazer: shmat");
        exit(EXIT_FAILURE);
    }

    int waga = bagaz_waga();
    printf("[P][PID=%d] Pasażer %ld -> Odprawa bagażu, waga=%d (limit=%d)\n",
           getpid(), pasazerNum, waga, planeMd);
    sleep(rand() % 2 + 1);
    if (waga > planeMd) {
        printf("[P][PID=%d] Pasażer %ld: Odrzucony (bagaż za ciężki)\n", getpid(), pasazerNum);
        pthread_mutex_lock(&shm_ptr->shm_mutex);
        shm_ptr->total_rejected++;
        pthread_mutex_unlock(&shm_ptr->shm_mutex);
        shmdt(shm_ptr);
        exit(0);
    }
    printf("[P][PID=%d] Pasażer %ld: Bagaż OK\n", getpid(), pasazerNum);

    /* Przeprowadzenie kontroli bezpieczeństwa */
    KontrolaArgs args;
    args.pasazerNum = pasazerNum;
    args.isVIP = ((rand() % 100) < VIP_PERCENT) ? 1 : 0;
    args.plec = ((rand() % 2) == 0) ? 'K' : 'M';

    pthread_t tid;
    if (pthread_create(&tid, NULL, kontrola_thread_func, &args) != 0) {
        perror("Pasazer: pthread_create");
        shmdt(shm_ptr);
        exit(1);
    }
    void* kontrola_result = NULL;
    pthread_join(tid, &kontrola_result);
    if ((intptr_t)kontrola_result == 0) {
        printf("[P][PID=%d] Pasażer %ld: Odrzucony w kontroli, kończę proces.\n", getpid(), pasazerNum);
        pthread_mutex_lock(&shm_ptr->shm_mutex);
        shm_ptr->total_rejected++;
        pthread_mutex_unlock(&shm_ptr->shm_mutex);
        shmdt(shm_ptr);
        exit(0);
    }

    /* Oczekiwanie na otwarcie gate'a przez samolot */
    int waited = 0;
    while (1) {
        pthread_mutex_lock(&shm_ptr->shm_mutex);
        int gateStatus = shm_ptr->gate_open[plane_index];
        pthread_mutex_unlock(&shm_ptr->shm_mutex);
        if (gateStatus == 1)
            break;
        sleep(1);
        waited++;
        if (waited >= WAIT_TIMEOUT) {
            printf("[P][PID=%d] Pasażer %ld: Timeout oczekiwania – opuszczam boarding.\n", getpid(), pasazerNum);
            pthread_mutex_lock(&shm_ptr->shm_mutex);
            shm_ptr->total_rejected++;
            pthread_mutex_unlock(&shm_ptr->shm_mutex);
            shmdt(shm_ptr);
            exit(0);
        }
    }

    printf("[P][PID=%d] Pasażer %ld: Gate otwarty, ruszam na boarding.\n", getpid(), pasazerNum);
    pthread_mutex_lock(&shm_ptr->shm_mutex);
    shm_ptr->boarded[plane_index]++;
    pthread_mutex_unlock(&shm_ptr->shm_mutex);

    /* Sygnalizacja wejścia pasażera poprzez anonimowy semafor */
    sem_post(&shm_ptr->boarding_sem[plane_index]);

    shmdt(shm_ptr);
    printf("[P][PID=%d] Pasażer %ld: Dotarłem do samolotu (koniec).\n", getpid(), pasazerNum);
    exit(0);
}
