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

// Sygnalizacja w Holu
static volatile sig_atomic_t canGoToStairs = 0;
static pid_t plane_pid = 0; // PID samolotu, przekazywany w argv

// Handler sygnału z samolotu (SIGUSR2)
static void hol_signal_handler(int sig) {
    if (sig == SIGUSR2) {
        canGoToStairs = 1;
    }
}

// Wątek kontroli bezpieczeństwa
void* kontrola_thread_func(void* arg) {
    KontrolaArgs* data = (KontrolaArgs*)arg;
    if (data->isVIP) {
        kontrola_bezpieczenstwa_vip(data->pasazerNum, data->plec);
    } else {
        kontrola_bezpieczenstwa(data->pasazerNum, data->plec);
    }
    return NULL;
}

// Pomocnicze – waga bagażu
int bagaz_waga() {
    return (rand() % 13) + 1;
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        fprintf(stderr, "Użycie: %s <pasazer_id> <plane_pid>\n", argv[0]);
        exit(1);
    }
    long pasazerNum = atol(argv[1]);
    plane_pid = atol(argv[2]);

    srand(time(NULL) ^ getpid());

    printf("[P] PID=%d -> Pasażer %ld, samolot PID=%ld\n",
           getpid(), pasazerNum, (long)plane_pid);

    // Etap: Kupno biletu – de facto zapamiętanie plane_pid
    // (już mamy w zmiennej plane_pid)

    // Etap: Odprawa bagażu
    int waga = bagaz_waga();
    printf("[P][PID=%d] Pasażer %ld -> Odprawa bagażu, waga=%d\n",
           getpid(), pasazerNum, waga);
    sleep(rand()%2 + 1);

    if (waga > M) {
        printf("[P][PID=%d] Pasażer %ld ODRZUCONY (bagaż za ciężki)\n",
               getpid(), pasazerNum);
        exit(0);
    }
    printf("[P][PID=%d] Pasażer %ld: Bagaż OK\n", getpid(), pasazerNum);

    // Etap: Kontrola bezpieczeństwa (wątek)
    KontrolaArgs args;
    args.pasazerNum = pasazerNum;
    args.isVIP = ((rand()%100) < VIP_PERCENT) ? 1 : 0;
    args.plec = ((rand()%2)==0) ? 'K' : 'M';

    pthread_t tid;
    if (pthread_create(&tid, NULL, kontrola_thread_func, &args) != 0) {
        perror("pthread_create");
        exit(1);
    }
    pthread_join(tid, NULL);

    // Po kontroli bezpieczeństwa -> HOL
    printf("[HOL][PID=%d] Pasażer %ld -> czeka na SIGUSR2 (plane_pid=%ld)\n",
           getpid(), pasazerNum, (long)plane_pid);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = hol_signal_handler;
    sigaction(SIGUSR2, &sa, NULL);

    while (!canGoToStairs) {
        pause(); 
    }
    printf("[HOL][PID=%d] Pasażer %ld: Otrzymano SIGUSR2, ruszam na schody.\n",
           getpid(), pasazerNum);

    // Etap: Schody
    sleep(1);
    printf("[P][PID=%d] Pasażer %ld: Dotarłem do samolotu (koniec).\n",
           getpid(), pasazerNum);

    return 0;
}
