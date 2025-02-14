#include <stdlib.h>
#include <stdio.h>
#include "gate.h"
#include "global.h"

// Tablica bramek
Gate* gates = NULL;
int max_gates = (MAX_SAMOLOT * 2 + 2) / 3;

// Inicjalizacja bramek
void gate_init(int num_gates) {
    gates = (Gate*)malloc(sizeof(Gate) * num_gates);
    if (!gates) {
        perror("Gate: alokacja");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < num_gates; i++) {
        gates[i].gate_id = i + 1;
        gates[i].is_occupied = false;
        gates[i].assigned_samoloty_pid = -1;
        gates[i].capacity = 0;
    }
    max_gates = num_gates;
    printf("Gate: Zainicjalizowano %d.\n", max_gates);
}

// Szuka wolnej bramki
int find_free_gate() {
    for (int i = 0; i < max_gates; i++) {
        if (!gates[i].is_occupied) {
            return i;
        }
    }
    return -1;
}

// Przydziela bramkę do samolotu – ustawia też capacity
void gate_assign(int gate_index, pid_t plane_pid, int capacity) {
    gates[gate_index].is_occupied = true;
    gates[gate_index].assigned_samoloty_pid = plane_pid;
    gates[gate_index].capacity = capacity;
    printf("Gate: Samolot %d -> gate %d z pojemnością %d.\n", plane_pid, gates[gate_index].gate_id, capacity);
}

// Zwalnia bramkę na podstawie PID samolotu
void gate_release_by_plane(pid_t plane_pid) {
    for (int i = 0; i < max_gates; i++) {
        if (gates[i].assigned_samoloty_pid == plane_pid) {
            gates[i].is_occupied = false;
            gates[i].assigned_samoloty_pid = -1;
            gates[i].capacity = 0;
            printf("Gate %d: Zwolniony (samolot %d).\n", gates[i].gate_id, plane_pid);
            return;
        }
    }
    printf("Gate: Nie znaleziono samolotu %d.\n", plane_pid);
}

// Sprzątanie zasobów bramek
void gate_cleanup() {
    if (gates) {
        free(gates);
        gates = NULL;
    }
    printf("Gate: Cleanup.\n");
}
