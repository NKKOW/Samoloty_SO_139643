#ifndef GLOBAL_H
#define GLOBAL_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdbool.h>

// --------------------------------------------
// Definicje związane z samolotami i komunikatami
// --------------------------------------------

#define MAX_SAMOLOT 5

#define MSG_QUEUE_PATH "msgqueue.key"
#define MSG_QUEUE_PROJ 'M'

typedef enum {
    MSG_SAMOLOT_GOTOWY = 1,
    MSG_SAMOLOT_POWROT,
    MSG_GATE_REQUEST,
    MSG_GATE_ASSIGN
} rodzaj_wiadomosc;

typedef struct {
    long mtype;
    rodzaj_wiadomosc rodzaj;
    pid_t samolot_pid;
    int gate_id;
} wiadomosc_buf;

// --------------------------------------------
// Definicje do obsługi pasażerów
// --------------------------------------------

// Maksymalna waga bagażu
#define M 10
// Liczba wszystkich pasażerów
#define N 150
// Pojemność schodów pasażerskich
#define K 10

// Dodatkowy limit, ile razy można kogoś przepuścić
#define MAX_SKIP 3

// Ile % pasażerów jest VIP (przykładowo 20%).
#define VIP_PERCENT 20

// --------------------------------------------
// Segment pamięci dzielonej – np. do liczenia pasażerów
// --------------------------------------------
typedef struct {
    int total_boarded;        // liczba faktycznie wsiedzionych
    int total_rejected;       // liczba odrzuconych
} SharedData;

#endif
