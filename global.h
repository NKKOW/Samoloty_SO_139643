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

// Sygnały kontrolne
#define SIGUSR_DEPART   SIGUSR1
#define SIGUSR_NO_BOARD SIGUSR2

// Maksymalna liczba samolotów
#define MAX_SAMOLOT 10

// Pojemność jednego samolotu
#define PLANE_CAPACITY 25

// Czas oczekiwania T1 (sekundy)
#define T1  20

// Informacje o kolejce komunikatów
#define MSG_QUEUE_PATH "msgqueue.key"
#define MSG_QUEUE_PROJ 'M'

// Typy komunikatów
typedef enum {
    MSG_SAMOLOT_GOTOWY = 1,
    MSG_SAMOLOT_POWROT,
    MSG_GATE_REQUEST,
    MSG_GATE_ASSIGN,
    MSG_BOARDING_FINISHED
} rodzaj_wiadomosc;

// Struktura wiadomości
typedef struct {
    long mtype;
    rodzaj_wiadomosc rodzaj;
    pid_t samolot_pid;
    int gate_id;
} wiadomosc_buf;

// Domyślny limit bagażu (niewykorzystywany)
#define DEFAULT_M 10

// Pojemność schodów
#define K  15

// Procent pasażerów VIP
#define VIP_PERCENT 20

// Maksymalna liczba przepuszczeń w kolejce
#define MAX_SKIP 3

// Flaga zatrzymania boardingu
extern volatile sig_atomic_t stopBoarding;

// Struktura danych współdzielonych – teraz zawiera też licznik pasażer
typedef struct {
    pthread_mutex_t shm_mutex;
    int total_boarded;
    int total_rejected;
    int licznik_pasazer;  // Licznik pasażerów, wspólny dla wszystkich procesów
} SharedData;

// Deklaracje semaforów i mutexów
extern sem_t stairsSemNormal;
extern sem_t stairsSemVip;
extern pthread_mutex_t mutex;
extern pthread_cond_t samolotCond;

// Pojemności dwóch części schodów
extern int capacityNormal;
extern int capacityVip;

// Semafor dla wagi bagażu
extern sem_t bagaz_wagaSem;

// Liczba pasażerów na schodach
extern int passengers_on_stairs;
extern pthread_mutex_t stairsMutex;
extern pthread_cond_t stairsCond;

// Zmienne używane w Dyspozytorze
extern int total_passengers_assigned;
extern int planes_returned;

// Struktura opisująca samolot
typedef struct {
    int planeIndex;
    int baggageLimitMd;
    int capacityP;
    int returnTimeTi;
} PlaneInfo;

#endif
