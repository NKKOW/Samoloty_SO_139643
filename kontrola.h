#ifndef KONTROLA_H
#define KONTROLA_H

#include <pthread.h>
#include <stdbool.h>

// Stała definiująca maksymalną liczbę przepuszczeń
#define MAX_SKIP 3

// Struktura stanowiska kontroli bezpieczeństwa
typedef struct {
    int occupied;    // Ile osób aktualnie (0,1,2)
    char plec;       // 'K' lub 'M' lub '\0'
} StanowiskoBezp;

// Struktura pasażera w kolejce
typedef struct PassengerNode {
    long pasazer_id;
    char plec;
    int skip_count;
    bool has_been_skipped; // Nowy atrybut
    pthread_cond_t cond; // Warunek do sygnalizacji pasażera
    struct PassengerNode* next;
} PassengerNode;

// Struktura kolejki FIFO
typedef struct {
    PassengerNode* front;
    PassengerNode* rear;
    pthread_mutex_t mutex;
} PassengerQueue;

// Deklaracje zmiennych globalnych
extern StanowiskoBezp stanowiska[3];
extern pthread_mutex_t stanowiskoMutex;
extern pthread_cond_t  stanowiskoCond;

extern StanowiskoBezp vipStanowiska[3];
extern pthread_mutex_t vipStanowiskoMutex;
extern pthread_cond_t  vipStanowiskoCond;

// Deklaracje funkcji kontroli bezpieczeństwa
void kontrola_bezpieczenstwa(long pasazer_id, char plec);
void kontrola_bezpieczenstwa_vip(long pasazer_id, char plec);

// Funkcje pomocnicze do zarządzania kolejką
void enqueue_passenger(PassengerQueue* queue, PassengerNode* node);
PassengerNode* dequeue_passenger(PassengerQueue* queue);
PassengerNode* find_and_remove_passenger(PassengerQueue* queue, char plec);
void init_queue(PassengerQueue* queue);
int is_queue_empty(PassengerQueue* queue);

#endif
