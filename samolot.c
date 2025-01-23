// samolot.c

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "global.h"
#include "samolot.h"

int msg_queue_id;
volatile sig_atomic_t keep_running = 1;

void sigint_handler(int sig) {
    printf("\nSamolot %d: SIGINT, konczenie...\n", getpid());
    keep_running = 0;
}

void notify_dyspozytor(rodzaj_wiadomosc msg_type) {
    wiadomosc_buf msg;
    msg.mtype = 1;
    msg.rodzaj = msg_type;
    msg.samolot_pid = getpid();
    msg.gate_id = -1;
    if (msgsnd(msg_queue_id, &msg, sizeof(wiadomosc_buf) - sizeof(long), 0) == -1) {
        perror("Blad msgsnd samolot");
        exit(EXIT_FAILURE);
    }
}

void simulate_flight_cycle() {
    printf("Samolot %d: Start\n", getpid());
    key_t msg_key = ftok(MSG_QUEUE_PATH, MSG_QUEUE_PROJ);
    if (msg_key == -1) {
        perror("Blad ftok samolot");
        exit(EXIT_FAILURE);
    }
    msg_queue_id = msgget(msg_key, 0666);
    if (msg_queue_id == -1) {
        perror("Blad msgget samolot");
        exit(EXIT_FAILURE);
    }
    signal(SIGINT, sigint_handler);
    notify_dyspozytor(MSG_GATE_REQUEST);
    wiadomosc_buf reply_msg;
    printf("Samolot %d: Oczekiwanie na gate...\n", getpid());
    if(msgrcv(msg_queue_id, &reply_msg,
              sizeof(wiadomosc_buf) - sizeof(long),
              getpid(), 0) == -1) {
        perror("Blad msgrcv samolot");
        exit(EXIT_FAILURE);
    }
    if(reply_msg.rodzaj != MSG_GATE_ASSIGN) {
        notify_dyspozytor(MSG_SAMOLOT_POWROT);
        return;
    }
    if(reply_msg.gate_id == -1) {
        notify_dyspozytor(MSG_SAMOLOT_POWROT);
        return;
    }
    printf("Samolot %d: Przydzielono gate %d\n", getpid(), reply_msg.gate_id);
    sleep(5);
    if (!keep_running) {
        notify_dyspozytor(MSG_SAMOLOT_POWROT);
        return;
    }
    printf("Samolot %d: Odlot\n", getpid());
    sleep(2);
    printf("Samolot %d: Powrot\n", getpid());
    notify_dyspozytor(MSG_SAMOLOT_POWROT);
    exit(EXIT_SUCCESS);
}

int main() {
    simulate_flight_cycle();
    return 0;
}
