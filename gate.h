#ifndef GATE_H
#define GATE_H

#include <pthread.h>
#include "global.h"

// Struktura reprezentująca Gate
typedef struct {
    int gate_id;
    pid_t samolot_pid;
    pthread_mutex_t gate_mutex;
} Gate;

int gate_init(Gate *gate, int gate_id);
void* gate_thread_func(void *arg);

#endif 