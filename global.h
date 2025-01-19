#ifndef GLOBAL_H
#define GLOBAL_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdbool.h>

//Stałe
#define MAX_SAMOLOT 5  //Ogranicznik maksymalnej liczby samolotów
#define MAX_PASAZER 100  //Ogranicznik liczby pasażerów
#define MSG_QUEUE_KEY 1234 //klucz do kolejki komunikatów

//Rodzaj wiadomości
typedef enum {
    MSG_SAMOLOT_GOTOWY = 1, //Gotowość samolotu do odlotu
    MSG_SAMOLOT_POWROT  //Samolot powrócił na lotnisko
} rodzaj_wiadomosc;

//Bufor wiadomości
typedef struct {
    long mtype; //typ wiadomości
    pid_t samolot_pid; //PID samolot
} wiadomosc_buf;


#endif 
