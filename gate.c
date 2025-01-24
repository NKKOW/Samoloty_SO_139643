#include <stdlib.h>
#include <stdio.h>
#include "gate.h"
#include "global.h"

Gate* gates = NULL;
int max_gates = (MAX_SAMOLOT * 2 + 2) / 3;

void gate_init(int num_gates) {
    gates = (Gate*)malloc(sizeof(Gate) * num_gates);
    if (!gates) {
        perror("Blad alokacji dla gates");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < num_gates; i++) {
        gates[i].gate_id = i + 1;
        gates[i].is_occupied = false;
        gates[i].assigned_samoloty_pid = -1;
    }
    max_gates = num_gates;
    printf("Gate: Zainicjalizowano %d gate'ow.\n", max_gates);
}

int find_free_gate() {
    for (int i = 0; i < max_gates; i++) {
        if (!gates[i].is_occupied) {
            return i;
        }
    }
    return -1;
}

void gate_assign(int gate_index, pid_t plane_pid) {
    gates[gate_index].is_occupied = true;
    gates[gate_index].assigned_samoloty_pid = plane_pid;
    printf("Gate: Samolot %d przypisany do gate'a %d.\n",
           plane_pid, gates[gate_index].gate_id);
}

void gate_release_by_plane(pid_t plane_pid) {
    for (int i = 0; i < max_gates; i++) {
        if (gates[i].assigned_samoloty_pid == plane_pid) {
            gates[i].is_occupied = false;
            gates[i].assigned_samoloty_pid = -1;
            printf("Gate %d: Zwolniony po powrocie samolotu %d.\n",
                   gates[i].gate_id, plane_pid);
            return;
        }
    }
    printf("Gate: Nie znaleziono gate dla samolotu %d.\n", plane_pid);
}

void gate_cleanup() {
    if (gates) {
        free(gates);
        gates = NULL;
    }
    printf("Gate: Zasoby gate zwolnione.\n");
}
