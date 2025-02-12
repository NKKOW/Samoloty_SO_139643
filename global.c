#include "global.h"
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdatomic.h>

// Flaga zatrzymania
volatile sig_atomic_t stopBoarding = 0;

// Semafory schodów
sem_t stairsSemNormal;
sem_t stairsSemVip;

// Mutex i warunek samolotu
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t samolotCond = PTHREAD_COND_INITIALIZER;

// Część normalna i VIP schodów
int capacityNormal = (int)((80 * K) / 100);
int capacityVip = (int)((20 * K) / 100);

// Semafor wagi bagażu
sem_t bagaz_wagaSem;

// Stan schodów
int passengers_on_stairs = 0;
pthread_mutex_t stairsMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t stairsCond = PTHREAD_COND_INITIALIZER;

// Dane statystyczne dla Dyspozytora
int total_passengers_assigned = 0;
int planes_returned = 0;
