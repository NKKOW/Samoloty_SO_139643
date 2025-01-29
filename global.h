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

#define SIGUSR_DEPART SIGUSR1  // Sygnał 1: wymuszenie wcześniejszego odlotu
#define SIGUSR_NO_BOARD SIGUSR2  // Sygnał 2: zakaz dalszego boardingu

#define MAX_SAMOLOT 10
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

#define M 10 // Masa bagażu
#define N 50  // Ilość pasażerów
#define K  15 // Pojemność schodów
//Procent VIP
#define VIP_PERCENT 20
// Dodatkowy limit, ile razy można kogoś przepuścić (dot. nie-VIP).
#define MAX_SKIP 3

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
extern int capacityNormal;    
extern int capacityVip;       
extern int licznik_pasazer;   // Licznik pasażerów (tych co wsiedli lub odpadli)

// --------------------------------------------
// Semafor do wagi bagażowej
// --------------------------------------------
extern sem_t bagaz_wagaSem;

// --------------------------------------------
// Nowe zmienne do śledzenia pasażerów na schodach
// --------------------------------------------
extern int passengers_on_stairs; // Liczba pasażerów na schodach
extern pthread_mutex_t stairsMutex; // Mutex do ochrony zmiennej passengers_on_stairs
extern pthread_cond_t stairsCond; // Warunek do sygnalizacji zmiany stanu pasażerów na schodach

#endif  
