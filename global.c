#include "global.h"

// Flaga globalna do zatrzymania boardingu
volatile sig_atomic_t stopBoarding = 0;

// Semafory i inne
sem_t stairsSemNormal; 
sem_t stairsSemVip;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t samolotCond = PTHREAD_COND_INITIALIZER;

// capacityNormal i capacityVip odnoszą się do schodów (K = 5)
int capacityNormal = (int)((80*K)/100);  // 80% schodów – część "Normal"
int capacityVip    = (int)((20*K)/100);  // 20% schodów – część "VIP"

int licznik_pasazer = 0;  // prosty licznik – ilu pasażerów już się wpasowało lub odpadło (globalnie)

// Semafor do wagi bagażu
sem_t bagaz_wagaSem;

// Pasażerowie na schodach
int passengers_on_stairs = 0;
pthread_mutex_t stairsMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t stairsCond = PTHREAD_COND_INITIALIZER;

// Zmienne do Dyspozytora
int total_passengers_assigned = 0;
int planes_returned = 0;
