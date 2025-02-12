#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>

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

// Sygnał z samolotu -> start wchodzenia
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
    return (rand() % 13) + 1; // 1..13
}

int main(int argc, char** argv)
{
    if (argc < 4) {
        fprintf(stderr, "Użycie: %s <pasazer_id> <plane_pid> <planeMd>\n", argv[0]);
        exit(1);
    }
    long pasazerNum = atol(argv[1]);
    plane_pid       = atol(argv[2]);
    int planeMd     = atoi(argv[3]); // rzeczywisty limit bagażu dla samolotu

    srand(time(NULL) ^ getpid());

    printf("[P] PID=%d -> Pasażer %ld, samolot PID=%ld, limitMd=%d\n",
           getpid(), pasazerNum, (long)plane_pid, planeMd);

    // Odprawa bagażu
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

    // Kontrola bezpieczeństwa
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

    // Hol - czekamy na SIGUSR2
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = hol_signal_handler;
    sigaction(SIGUSR2, &sa, NULL);

    while (!canGoToStairs) {
        pause();
    }
    printf("[HOL][PID=%d] Pasażer %ld: Otrzymano SIGUSR2, ruszam na schody.\n",
           getpid(), pasazerNum);

    // Schody -> finalnie do samolotu
    sleep(1);
    printf("[P][PID=%d] Pasażer %ld: Dotarłem do samolotu (koniec).\n",
           getpid(), pasazerNum);

    // Zwiększamy globalny licznik (licznik_pasazer) – normalnie:
    pthread_mutex_lock(&mutex);
    licznik_pasazer++;
    pthread_mutex_unlock(&mutex);

    return 0;
}
