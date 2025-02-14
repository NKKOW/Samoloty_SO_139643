#ifndef GATE_H
#define GATE_H

#include <sys/types.h>
#include <stdbool.h>

// Struktura bramki – rozszerzona o pole capacity
typedef struct {
    int gate_id;
    bool is_occupied;
    pid_t assigned_samoloty_pid;
    int capacity; // pojemność bramki równa pojemności samolotu
} Gate;

extern Gate* gates;
extern int max_gates;

// Inicjalizacja i obsługa bramek
void gate_init(int num_gates);
int find_free_gate();
void gate_assign(int gate_index, pid_t plane_pid, int capacity);
void gate_release_by_plane(pid_t plane_pid);
void gate_cleanup();

#endif
