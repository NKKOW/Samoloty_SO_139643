#ifndef ODPRAWA_H
#define ODPRAWA_H

#include <pthread.h>
#include <stdbool.h>
#include "global.h"
#include "terminal.h"
#include "gate.h"

// Struktura reprezentująca Odprawę
typedef struct {
    int num_samoloty;
    int num_terminale;
    Terminal *terminale;
    pthread_t *terminal_threads;
    pthread_t *gate_threads;
    pthread_mutex_t assign_mutex;
    int msg_queue_id;
    bool running;
} Odprawa;

int odprawa_init(Odprawa *odprawa_ptr, int num_airplanes);
void odprawa_cleanup(Odprawa *odprawa_ptr);
bool assign_samolot(Odprawa *odprawa_ptr, pid_t samolot_pid, Terminal **assigned_terminal, Gate **assigned_gate);
void release_resources(Odprawa *odprawa_ptr, Terminal *terminal, Gate *gate);  // Funkcja zwalniająca terminal i gate po zakończeniu obsługi samolotu
void handle_messages(Odprawa *odprawa_ptr);
void log_event(const char *filename, const char *message);
void odprawa_sigint_handler(int sig);

#endif 
