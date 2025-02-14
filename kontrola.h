#ifndef KONTROLA_H
#define KONTROLA_H

#include <pthread.h>
#include <stdbool.h>

// Maksymalna liczba przepuściń
#define MAX_SKIP 3

// Stanowisko kontroli
typedef struct {
    int occupied;
    char plec;
} StanowiskoBezp;

// Węzeł pasażera w kolejce
typedef struct PassengerNode {
    long pasazer_id;
    char plec;
    int skip_count;
    bool has_been_skipped;
    pthread_cond_t cond;
    struct PassengerNode* next;
} PassengerNode;

// Kolejka pasażerów
typedef struct {
    PassengerNode* front;
    PassengerNode* rear;
    pthread_mutex_t mutex;
} PassengerQueue;

// Deklaracje stanowisk
extern StanowiskoBezp stanowiska[3];
extern pthread_mutex_t stanowiskoMutex;
extern pthread_cond_t  stanowiskoCond;

extern StanowiskoBezp vipStanowiska[3];
extern pthread_mutex_t vipStanowiskoMutex;
extern pthread_cond_t  vipStanowiskoCond;

// Funkcje kontroli – zmienione na zwracanie int
int kontrola_bezpieczenstwa(long pasazer_id, char plec);
int kontrola_bezpieczenstwa_vip(long pasazer_id, char plec);

// Zarządzanie kolejkami
void enqueue_passenger(PassengerQueue* queue, PassengerNode* node);
PassengerNode* dequeue_passenger(PassengerQueue* queue);
PassengerNode* find_and_remove_passenger(PassengerQueue* queue, char plec);
void init_queue(PassengerQueue* queue);
int is_queue_empty(PassengerQueue* queue);

#endif
