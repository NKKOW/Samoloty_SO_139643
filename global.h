// global.h

#ifndef GLOBAL_H
#define GLOBAL_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdbool.h>

#define MAX_SAMOLOT 500
#define MIEJSCA_SAMOLOT 10
#define MAX_PASAZER 300

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

#endif
