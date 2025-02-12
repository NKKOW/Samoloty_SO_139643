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

#define SIGUSR_DEPART   SIGUSR1
#define SIGUSR_NO_BOARD SIGUSR2

// Zmiana nazwy na MAX_PLANES dla czytelności
#define MAX_SAMOLOT 10

// Każdy samolot będzie miał faktyczną pojemność P, np.:
#define PLANE_CAPACITY 25

// Stała globalna T1 to czas (w sekundach) po którym samolot może odlecieć
#define T1  20  

#define MSG_QUEUE_PATH "msgqueue.key"
#define MSG_QUEUE_PROJ 'M'

// Rodzaje komunikatów
typedef enum {
    MSG_SAMOLOT_GOTOWY = 1,
    MSG_SAMOLOT_POWROT,
    MSG_GATE_REQUEST,
    MSG_GATE_ASSIGN,
    MSG_BOARDING_FINISHED
} rodzaj_wiadomosc;

// Struktura komunikatu w kolejce
typedef struct {
    long mtype;
    rodzaj_wiadomosc rodzaj;
    pid_t samolot_pid;
    int gate_id;
} wiadomosc_buf;

// Definicje do obsługi pasażerów
// Uwaga: Każdy samolot może mieć inny limit bagażu, stąd M w starym sensie
// będzie jedynie stałą domyślną / nieużywaną. Realny limit – dynamicznie.
#define DEFAULT_M 10

// Pojemność schodów
#define K  15

// Procent VIP-ów
#define VIP_PERCENT 20

// Ile razy pasażer może przepuścić innych
#define MAX_SKIP 3

// Flaga globalna do zatrzymania boardingu (sygnał2)
extern volatile sig_atomic_t stopBoarding;

// Struktura pamięci współdzielonej
typedef struct {
    pthread_mutex_t shm_mutex;
    int total_boarded;
    int total_rejected;
} SharedData;

// Semafory i inne zmienne globalne
extern sem_t stairsSemNormal;
extern sem_t stairsSemVip;

extern pthread_mutex_t mutex;
extern pthread_cond_t samolotCond;

// Te wartości dot. schodów podzielonych na część normalną i VIP
extern int capacityNormal;
extern int capacityVip;

// Licznik wszystkich pasażerów (dla uproszczenia; w oryginale można rozdzielić)
extern int licznik_pasazer;

// Semafor do wagi bagażu (jeśli potrzebny)
extern sem_t bagaz_wagaSem;

// Pasażerowie na schodach
extern int passengers_on_stairs;
extern pthread_mutex_t stairsMutex;
extern pthread_cond_t stairsCond;

// Zmienne do Dyspozytora
extern int total_passengers_assigned;
extern int planes_returned;

// --------------------------
// Struktura opisująca samolot
// --------------------------
typedef struct {
    int planeIndex;         // Indeks samolotu (0..MAX_PLANES-1)
    int baggageLimitMd;     // Limit bagażu podręcznego w tym samolocie
    int capacityP;          // Rzeczywista pojemność samolotu
    int returnTimeTi;       // Czas "powrotu" samolotu (symulacja)
} PlaneInfo;

#endif
