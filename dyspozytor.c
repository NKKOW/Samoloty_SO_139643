#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "global.h"
#include "shared_memory.h"
#include "dyspozytor.h"

int msg_queue_id;

volatile sig_atomic_t keep_running = 1;

// Funkcja obsługi sygnału SIGINT
void sigint_handler(int sig) 
{
    printf("\nDyspozytor: Otrzymano SIGINT, czyszczenie zasobów...\n");
    keep_running = 0;
}

void initialize_message_queue() 
{
    key_t msg_key = ftok(MSG_QUEUE_PATH, MSG_QUEUE_PROJ);
    if (msg_key == -1)
    {
        perror("Blad ftok dyspozytor\n");
        exit(EXIT_FAILURE);
    }

    msg_queue_id = msgget(msg_key, IPC_CREAT | 0666);
    if (msg_queue_id == -1)
    {
        perror("Blad msgget dyspozytor\n");
        exit(EXIT_FAILURE);
    }
}

void cleanup_message_queue()
{
    if (msgctl(msg_queue_id, IPC_RMID, NULL) == -1)
    {
        perror("Blad msgctl dyspozytor\n");
    }
}

void create_airplanes(int num_samoloty)
{
    for (int i = 0; i < num_samoloty; i++)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            perror("Blad fork dyspozytor\n");
            cleanup_message_queue();
            shared_memory_cleanup();
            semaphore_cleanup();
            exit(EXIT_FAILURE);
        }
        else if ( pid == 0)
        {
            execl("./samolot", "samolot", NULL);
            perror("Blad execl dyspozytor\n");
        }

        printf("Dyspozytor: Utworzono samolot o PID %d\n", pid);
    }
}

void handle_messages()
{
    wiadomosc_buf msg;
    int powrot_count = 0;

    while(keep_running && powrot_count < MAX_SAMOLOT)
    {
        if (msgrcv(msg_queue_id, &msg, sizeof(pid_t), 0, 0) == -1)
        {
            perror("Blad msgrcv dyspozytor\n");
            continue;
        }

        printf("Dyspozytor: Otrzymano wiadomość typu %ld od samolotu PID %d\n", msg.mtype, msg.samolot_pid);

        if (msg.mtype == MSG_SAMOLOT_GOTOWY)
        {
            printf("Dyspozytor: Samolot o PID %d jest gotowy do startu.\n", msg.samolot_pid);
            if (semaphore_lock() == -1)
            {
                perror("Dyspozytor: Blad semaphore_lock | MSG_SAMOLOT_GOTOWY");
                continue;
            }
            shm_ptr -> akt_samoloty += 1;
            printf("Dyspozytor: Liczba aktywnych samolotów: %d\n", shm_ptr->akt_samoloty);
            semaphore_unlock();
            printf("Dyspozytor: Semafor odblokowany.\n");
        }
        else if (msg.mtype == MSG_SAMOLOT_POWROT)
        {
            printf("Dyspozytor: Samolot o PID %d powrócił na lotnisko.\n", msg.samolot_pid);
            powrot_count ++;

            if (semaphore_lock() == -1)
            {
                perror("Dyspozytor: Blad semaphore_lock | MSG_SAMOLOT_POWROT");
                continue;
            }
            shm_ptr -> akt_samoloty -= 1;
            printf("Dyspozytor: Liczba aktywnych samolotów: %d\n", shm_ptr->akt_samoloty);
            semaphore_unlock();
            printf("Dyspozytor: Semafor odblokowany.\n");
        }

        if (powrot_count >= MAX_SAMOLOT) 
        {
            printf("Dyspozytor: Wszystkie samoloty powróciły. Zakończenie działania.\n");
        }
    }
}

int main() {
    printf("Dyspozytor: Startuje...\n");

    signal(SIGINT, sigint_handler);

    if (shared_memory_init() == -1)
    {
        fprintf(stderr, "Dyspozytor: Nie mozna zainicjalizowac pamieci wspoldzielonej.\n");
        exit(EXIT_FAILURE);
    }

    if (semaphore_init() == -1)
    {
        fprintf(stderr, "Dyspozytor: Nie mozna zainicjalizowac semafora.\n");
        shared_memory_cleanup();
        exit(EXIT_FAILURE);
    }

    initialize_message_queue();

    create_airplanes(MAX_SAMOLOT);

    handle_messages();

    cleanup_message_queue();
    shared_memory_cleanup();
    semaphore_cleanup();

    printf("Dyspozytor: Zakończył działanie.\n");
    return 0;
}