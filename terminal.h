#ifndef TERMINAL_H
#define TERMINAL_H

#include <pthread.h>
#include "global.h"
#include "gate.h"

#define MAX_SAMOLOT_PER_TERMINAL  5

// Struktura reprezentująca Terminal
typedef struct {
    int terminal_id;
    int current_samoloty;
    int num_gates;
    Gate *gates;
    pthread_mutex_t terminal_mutex;
    pthread_cond_t terminal_cond;
} Terminal;

int terminal_init(Terminal *terminal, int terminal_id, int num_gates);
void* terminal_thread_func(void *arg);

#endif 