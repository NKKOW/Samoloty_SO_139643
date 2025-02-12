#ifndef GLOBAL_H
#define GLOBAL_H
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdbool.h>
#include <semaphore.h>
#include <signal.h>
#include <pthread.h>
#include <stdatomic.h>

#define SIGUSR_DEPART SIGUSR1
#define SIGUSR_NO_BOARD SIGUSR2

#define MAX_SAMOLOT 10
#define MAX_PASAZER (MAX_SAMOLOT * N) 
#define T1  20 // W sekundach

#define MSG_QUEUE_PATH "msgqueue.key"
#define MSG_QUEUE_PROJ 'M'

typedef enum {
    MSG_SAMOLOT_GOTOWY = 1,
    MSG_SAMOLOT_POWROT,
    MSG_GATE_REQUEST,
    MSG_GATE_ASSIGN,
    MSG_BOARDING_FINISHED 
} rodzaj_wiadomosc;

typedef struct {
    long mtype;
    rodzaj_wiadomosc rodzaj;
    pid_t samolot_pid;
    int gate_id;
} wiadomosc_buf;

// Definicje do obsługi pasażerów

#define M 10 // masa bagażu
#define N 50  //liczba pasażerów w samolocie
#define K  15 // schody
#define VIP_PERCENT 20
#define MAX_SKIP 3

extern volatile sig_atomic_t stopBoarding;

// --------------------------------------------
typedef struct {
    pthread_mutex_t shm_mutex; 
    int total_boarded;
    int total_rejected;
} SharedData;

extern sem_t stairsSemNormal; 
extern sem_t stairsSemVip;

extern pthread_mutex_t mutex;
extern pthread_cond_t samolotCond;

extern int capacityNormal;    
extern int capacityVip;       
extern int licznik_pasazer;   

extern sem_t bagaz_wagaSem;

extern int passengers_on_stairs; 
extern pthread_mutex_t stairsMutex; 
extern pthread_cond_t stairsCond; 

// Zmienne do Dyspozytora
extern int total_passengers_assigned;
extern int planes_returned;

#endif
