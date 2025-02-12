#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <sys/wait.h>
#include <errno.h>
#include "global.h"
#include "samolot.h"

int msg_queue_id;
volatile sig_atomic_t keep_running = 1;
volatile sig_atomic_t early_depart = 0;

static int shm_id = -1;
static SharedData* shm_ptr = NULL;

static int g_myPlaneIndex = 0;
static int g_myPlaneMd    = 10;
static int g_myPlaneP     = 25;
static int g_myPlaneTi    = 5;

void* stairs_thread_func(void* arg) {
    (void)arg;
    while (1) {
        pthread_mutex_lock(&mutex);
        int finished = (!keep_running);
        pthread_mutex_unlock(&mutex);
        if (finished) {
            break;
        }
        sleep(1);
    }
    printf("[Samolot %d][Schody] -> koniec wątku (planeIndex=%d).\n", getpid(), g_myPlaneIndex);
    return NULL;
}

void sigalrm_handler(int sig) {
    (void)sig;
    printf("[Samolot %d] Czas T1 upłynął, koniec boardingu.\n", getpid());
    early_depart = 0;
    keep_running = 0;
    stopBoarding = 1;
    pthread_cond_broadcast(&samolotCond);
    pthread_cond_broadcast(&stairsCond);
}

void sigtstp_handler(int sig) {
    printf("[Samolot %d] SIGTSTP (%d).\n", getpid(), sig);
    struct sigaction sa_default;
    sa_default.sa_handler = SIG_DFL;
    sigemptyset(&sa_default.sa_mask);
    sa_default.sa_flags = 0;
    sigaction(SIGTSTP, &sa_default, NULL);
    raise(SIGTSTP);
}

void sigusr_handler(int sig) {
    if (sig == SIGUSR1) {
        printf("[Samolot %d] Wczesny odlot (SIGUSR1)\n", getpid());
        early_depart = 1;
        keep_running = 0;
        stopBoarding = 1;
        pthread_cond_broadcast(&samolotCond);
        pthread_cond_broadcast(&stairsCond);
    } else if (sig == SIGUSR2) {
        printf("[Samolot %d] Zakaz dalszego boardingu (SIGUSR2)\n", getpid());
        keep_running = 0;
        stopBoarding = 1;
        pthread_cond_broadcast(&samolotCond);
        pthread_cond_broadcast(&stairsCond);
    }
}

void ignore_sigint() {
    struct sigaction sa_ignore;
    sa_ignore.sa_handler = SIG_IGN;
    sigemptyset(&sa_ignore.sa_mask);
    sa_ignore.sa_flags = 0;
    sigaction(SIGINT, &sa_ignore, NULL);
}

static void init_shared_memory() {
    key_t key = ftok(MSG_QUEUE_PATH, 'S');
    if (key == -1) {
        perror("Samolot: ftok");
        exit(EXIT_FAILURE);
    }
    shm_id = shmget(key, sizeof(SharedData), 0666 | IPC_CREAT);
    if (shm_id < 0) {
        perror("Samolot: shmget");
        exit(EXIT_FAILURE);
    }
    shm_ptr = (SharedData*) shmat(shm_id, NULL, 0);
    if (shm_ptr == (void*)-1) {
        perror("Samolot: shmat");
        exit(EXIT_FAILURE);
    }
}

static void detach_shared_memory() {
    if (shm_ptr) {
        shmdt(shm_ptr);
        shm_ptr = NULL;
    }
}

void notify_dyspozytor(rodzaj_wiadomosc msg_type) {
    wiadomosc_buf msg;
    msg.mtype = 1;
    msg.rodzaj = msg_type;
    msg.samolot_pid = getpid();
    msg.gate_id = -1;
    if (msgsnd(msg_queue_id, &msg, sizeof(wiadomosc_buf) - sizeof(long), 0) == -1) {
        perror("Samolot: msgsnd");
        exit(EXIT_FAILURE);
    }
}

void simulate_flight_cycle(int planeIndex, int myPlaneMd, int myPlaneP, int myPlaneTi) {
    printf("[Samolot %d] Start cyklu (planeIndex=%d, Md=%d, P=%d, Ti=%d)\n",
           getpid(), planeIndex, myPlaneMd, myPlaneP, myPlaneTi);
    ignore_sigint();

    struct sigaction sa_usr, sa_alarm, sa_tstp;
    memset(&sa_usr, 0, sizeof(sa_usr));
    sa_usr.sa_handler = sigusr_handler;
    sigaction(SIGUSR1, &sa_usr, NULL);
    sigaction(SIGUSR2, &sa_usr, NULL);

    memset(&sa_alarm, 0, sizeof(sa_alarm));
    sa_alarm.sa_handler = sigalrm_handler;
    sigaction(SIGALRM, &sa_alarm, NULL);

    memset(&sa_tstp, 0, sizeof(sa_tstp));
    sa_tstp.sa_handler = sigtstp_handler;
    sigaction(SIGTSTP, &sa_tstp, NULL);

    init_shared_memory();

    sem_init(&stairsSemNormal, 0, 1);
    sem_init(&stairsSemVip, 0, 1);
    sem_init(&bagaz_wagaSem, 0, 1);

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&samolotCond, NULL);

    key_t msg_key = ftok(MSG_QUEUE_PATH, MSG_QUEUE_PROJ);
    if (msg_key == -1) {
        perror("Samolot: ftok");
        exit(EXIT_FAILURE);
    }
    msg_queue_id = msgget(msg_key, 0666);
    if (msg_queue_id == -1) {
        perror("Samolot: msgget");
        exit(EXIT_FAILURE);
    }

    notify_dyspozytor(MSG_GATE_REQUEST);
    wiadomosc_buf reply_msg;
    printf("[Samolot %d] czeka na gate...\n", getpid());
    while (1) {
        ssize_t r = msgrcv(msg_queue_id, &reply_msg, sizeof(wiadomosc_buf) - sizeof(long), getpid(), 0);
        if (r == -1) {
            if (errno == EINTR) {
                continue;
            } else {
                perror("Samolot: msgrcv gate");
                return;
            }
        }
        break;
    }
    if (reply_msg.rodzaj != MSG_GATE_ASSIGN || reply_msg.gate_id == -1) {
        notify_dyspozytor(MSG_SAMOLOT_POWROT);
        return;
    }
    printf("[Samolot %d] Otrzymano gate %d.\n", getpid(), reply_msg.gate_id);

    pthread_t stairs_tid;
    pthread_create(&stairs_tid, NULL, stairs_thread_func, NULL);

    alarm(T1);
    sleep(2);

    printf("[Samolot %d] -> Start boardingu: SIGUSR2 do grupy.\n", getpid());
    kill(0, SIGUSR2);

    while (keep_running) {
        sleep(1);
        pthread_mutex_lock(&shm_ptr->shm_mutex);
        int current_passengers = shm_ptr->licznik_pasazer;
        pthread_mutex_unlock(&shm_ptr->shm_mutex);
        if (current_passengers >= myPlaneP) {
            printf("[Samolot %d] Osiągnięto pojemność samolotu (%d). Kończymy boarding.\n",
                   getpid(), myPlaneP);
            break;
        }
    }

    pthread_mutex_lock(&mutex);
    keep_running = 0;
    pthread_cond_broadcast(&samolotCond);
    pthread_mutex_unlock(&mutex);

    pthread_join(stairs_tid, NULL);

    pthread_mutex_lock(&shm_ptr->shm_mutex);
    int boarded_here = (shm_ptr->licznik_pasazer > myPlaneP) ? myPlaneP : shm_ptr->licznik_pasazer;
    shm_ptr->total_boarded += boarded_here;
    int rejected_here = myPlaneP - boarded_here;
    if (rejected_here < 0) rejected_here = 0;
    shm_ptr->total_rejected += rejected_here;
    pthread_mutex_unlock(&shm_ptr->shm_mutex);

    printf("[Samolot %d] Boarding zakończony. Weszło=%d, Odrzuconych=%d.\n",
           getpid(), boarded_here, rejected_here);

    {
        wiadomosc_buf done_msg;
        done_msg.mtype = 1;
        done_msg.rodzaj = MSG_BOARDING_FINISHED;
        done_msg.samolot_pid = getpid();
        done_msg.gate_id = -1;
        msgsnd(msg_queue_id, &done_msg, sizeof(wiadomosc_buf) - sizeof(long), 0);
    }

    sem_destroy(&bagaz_wagaSem);
    sem_destroy(&stairsSemNormal);
    sem_destroy(&stairsSemVip);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&samolotCond);

    if (early_depart) {
        printf("[Samolot %d] Wczesny odlot.\n", getpid());
    } else {
        printf("[Samolot %d] Odlot po T1 lub zapełnieniu.\n", getpid());
    }

    sleep(2);
    printf("[Samolot %d] Ląduję u celu, wracam... (czas powrotu %d sek)\n",
           getpid(), myPlaneTi);
    sleep(myPlaneTi);

    printf("[Samolot %d] Powrót\n", getpid());
    notify_dyspozytor(MSG_SAMOLOT_POWROT);

    detach_shared_memory();
}

int main(int argc, char* argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Użycie: %s <planeIndex> <Md> <P> <Ti>\n", argv[0]);
        exit(1);
    }
    g_myPlaneIndex = atoi(argv[1]);
    g_myPlaneMd    = atoi(argv[2]);
    g_myPlaneP     = atoi(argv[3]);
    g_myPlaneTi    = atoi(argv[4]);

    simulate_flight_cycle(g_myPlaneIndex, g_myPlaneMd, g_myPlaneP, g_myPlaneTi);
    sleep(1);
    return 0;
}
