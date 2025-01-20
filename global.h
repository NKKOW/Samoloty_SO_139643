#ifndef GLOBAL_H
#define GLOBAL_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdbool.h>

//Stałe
#define MAX_SAMOLOT 15  //Ogranicznik maksymalnej liczby samolotów
#define MIEJSCA_SAMOLOT 10 //ogranicznik liczby miejsc w samolocie
#define MAX_PASAZER 300  //Ogranicznik liczby pasażerów
//#define MSG_QUEUE_KEY 1234 //klucz do kolejki komunikatów

//Pliki ftok()
#define MSG_QUEUE_PATH "lotnisko.c"  //ścieżka do koleki komunikatów
#define SHM_PATH "shared_memory.c"  //ścieżka dla pamięci wspóldzielonej
#define SEM_PATH "shared_memory.c"  //Ścieżka dla semafora

//Identyfikatory projektu dla ftok()
#define MSG_QUEUE_PROJ 'M'  //identyfikator projektu dla kolejki komunikatów
#define SHM_PROJ 'S'  //Identyfikator dla pamięci wspóldzielonej
#define SEM_PROJ 'E'  //Identyfikator projektu dla semafora

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
