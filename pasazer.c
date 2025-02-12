#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "global.h"
#include "pasazer.h"
#include "kontrola.h"

typedef struct {
    long pasazerNum;
    int isVIP;
    char plec;
} KontrolaArgs;

static volatile sig_atomic_t canGoToStairs = 0;
static pid_t plane_pid = 0;

static void hol_signal_handler(int sig) {
    if (sig == SIGUSR2) {
        canGoToStairs = 1;
    }
}

void* kontrola_thread_func(void* arg) {
    KontrolaArgs* data = (KontrolaArgs*)arg;
    if (data->isVIP) {
        kontrola_bezpieczenstwa_vip(data->pasazerNum, data->plec);
    } else {
        kontrola_bezpieczenstwa(data->pasazerNum, data->plec);
    }
    return NULL;
}

int bagaz_waga() {
    return (rand() % 13) + 1;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "Użycie: %s <pasazer_id> <plane_pid> <planeMd>\n", argv[0]);
        exit(1);
    }

    long pasazerNum = atol(argv[1]);
    plane_pid = atol(argv[2]);
    int planeMd = atoi(argv[3]);

    srand(time(NULL) ^ getpid());

    printf("[P] PID=%d -> Pasażer %ld, samolot PID=%ld, limitMd=%d\n",
           getpid(), pasazerNum, (long)plane_pid, planeMd);

    int waga = bagaz_waga();
    printf("[P][PID=%d] Pasażer %ld -> Odprawa bagażu, waga=%d (limit=%d)\n",
           getpid(), pasazerNum, waga, planeMd);
    sleep(rand()%2 + 1);

    if (waga > planeMd) {
        printf("[P][PID=%d] Pasażer %ld ODRZUCONY (bagaż za ciężki)\n",
               getpid(), pasazerNum);
        exit(0);
    }
    printf("[P][PID=%d] Pasażer %ld: Bagaż OK\n", getpid(), pasazerNum);

    KontrolaArgs args;
    args.pasazerNum = pasazerNum;
    args.isVIP = ((rand()%100) < VIP_PERCENT) ? 1 : 0;
    args.plec = ((rand()%2) == 0) ? 'K' : 'M';

    pthread_t tid;
    if (pthread_create(&tid, NULL, kontrola_thread_func, &args) != 0) {
        perror("pthread_create");
        exit(1);
    }
    pthread_join(tid, NULL);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = hol_signal_handler;
    sigaction(SIGUSR2, &sa, NULL);

    printf("[HOL][PID=%d] Pasażer %ld: Czekam w holu na sygnał SIGUSR2...\n",
           getpid(), pasazerNum);

    while (!canGoToStairs) {
        pause();
    }
    printf("[HOL][PID=%d] Pasażer %ld: Otrzymano SIGUSR2, ruszam na schody.\n",
           getpid(), pasazerNum);

    sleep(1);
    printf("[P][PID=%d] Pasażer %ld: Dotarłem do samolotu (koniec).\n",
           getpid(), pasazerNum);

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
    if (shm_ptr == (void*) -1) {
        perror("Pasazer: shmat");
        exit(EXIT_FAILURE);
    }

    pthread_mutex_lock(&shm_ptr->shm_mutex);
    shm_ptr->licznik_pasazer++;
    pthread_mutex_unlock(&shm_ptr->shm_mutex);

    shmdt(shm_ptr);

    return 0;
}
