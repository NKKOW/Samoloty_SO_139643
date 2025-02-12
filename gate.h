#ifndef GATE_H
#define GATE_H

#include <sys/types.h>
#include <stdbool.h>

// Struktura bramki
typedef struct {
    int gate_id;
    bool is_occupied;
    pid_t assigned_samoloty_pid;
} Gate;

extern Gate* gates;
extern int max_gates;

// Inicjalizacja i obsługa bramek
void gate_init(int num_gates);
int find_free_gate();
void gate_assign(int gate_index, pid_t plane_pid);
void gate_release_by_plane(pid_t plane_pid);
void gate_cleanup();

#endif
