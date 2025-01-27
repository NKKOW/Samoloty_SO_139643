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

// Definicje związane z samolotami i komunikatami

#define SIGUSR_DEPART     SIGUSR1  // Sygnał 1: wymuszenie wcześniejszego odlotu
#define SIGUSR_NO_BOARD   SIGUSR2  // Sygnał 2: zakaz dalszego boardingu

#define MAX_SAMOLOT 5
#define T1  10 // W sekundach

#define MSG_QUEUE_PATH "msgqueue.key"
#define MSG_QUEUE_PROJ 'M'

typedef enum {
    MSG_SAMOLOT_GOTOWY = 1,
    MSG_SAMOLOT_POWROT,
    MSG_GATE_REQUEST,
    MSG_GATE_ASSIGN
} rodzaj_wiadomosc;

typedef struct {
    long mtype;
    rodzaj_wiadomosc rodzaj;
    pid_t samolot_pid;
    int gate_id;
} wiadomosc_buf;

// Definicje do obsługi pasażerów

#define M 10  //Miejsca
#define N 10  //ilość pasażerów
#define K 10 //pojemność schodów

// Dodatkowy limit, ile razy można kogoś przepuścić (dot. nie-VIP).
#define MAX_SKIP 3

// Ile % pasażerów jest VIP (przykładowo 20%).
#define VIP_PERCENT 20

extern volatile sig_atomic_t stopBoarding; // Blokuje boarding pasażerów

// --------------------------------------------
// Segment pamięci dzielonej – np. do liczenia pasażerów
// --------------------------------------------
typedef struct {
    pthread_mutex_t shm_mutex; // Mutex do synchronizacji
    int total_boarded;        // Liczba faktycznie wsiedzionych
    int total_rejected;       // Liczba odrzuconych
} SharedData;

// --------------------------------------------
// Semafory do obsługi schodów
// --------------------------------------------
extern sem_t stairsSemNormal; 
extern sem_t stairsSemVip;

// --------------------------------------------
// Mutex i warunki dla pasażerów
// --------------------------------------------
extern pthread_mutex_t mutex;
extern pthread_cond_t samolotCond;

// --------------------------------------------
// Liczniki pasażerów
// --------------------------------------------
extern int capacityNormal;    // Liczba wolnych miejsc na schodach zwykłych
extern int capacityVip;       // Liczba wolnych miejsc na schodach VIP
extern int licznik_pasazer;   // Licznik pasażerów (tych co wsiedli lub odpadli)

// --------------------------------------------
// Semafor do wagi bagażowej
// --------------------------------------------
extern sem_t bagaz_wagaSem;

#endif 
