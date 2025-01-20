#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "global.h"
#include "shared_memory.h"
#include "samolot.h"

int msg_queue_id;

volatile sig_atomic_t keep_running = 1;

// Funkcja obsługi sygnału SIGINT
void sigint_handler(int sig) 
{
    printf("\nSamolot %d: Otrzymano SIGINT, zakończanie lotu...\n", getpid());
    keep_running = 0;
}

void notify_dyspozytor(long msg_type)
{
    wiadomosc_buf msg;
    msg.mtype = msg_type;
    msg.samolot_pid = getpid();

    if(msgsnd(msg_queue_id, &msg, sizeof(pid_t), 0) == -1)
    {
        perror("Blad msgsnd samolot\n");
        exit(EXIT_FAILURE);
    }

    if (msg_type == MSG_SAMOLOT_GOTOWY)
    {
        printf("Samolot PID = %d powiadamia dyspozytora o gotowosci.\n", getpid());
    }
    else if(msg_type == MSG_SAMOLOT_POWROT)
    {
        printf("Samolot PID = %d powiadamia dyspozytora o powrocie na lotnisko.\n", getpid());
    }
}

void simulate_flight_cycle()
{
    printf("Samolot %d: Startuje..!\n", getpid());

    shm_ptr = shared_memory_att();
    if (shm_ptr == NULL)
    {
        fprintf(stderr, "Samolot %d: Nie mozna dołączyć do pamieci wspoldzielonej\n", getpid());
        exit(EXIT_FAILURE);
    }

    if (semaphore_att() == -1)
    {
        fprintf(stderr, "Samolot %d: Nie mozna dołączyć do semafora\n", getpid());
        shared_memory_detach(shm_ptr);
        exit(EXIT_FAILURE);
    }

    key_t msg_key = ftok(MSG_QUEUE_PATH, MSG_QUEUE_PROJ);
    if (msg_key == -1)
    {
        perror("Blad ftok samolot\n");
        shared_memory_detach(shm_ptr);
        exit(EXIT_FAILURE);
    }

    msg_queue_id = msgget(msg_key, 0666);
    if (msg_queue_id == -1)
    {
        perror("Blad msgget samolot\n");
        shared_memory_detach(shm_ptr);
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, sigint_handler);

    notify_dyspozytor(MSG_SAMOLOT_GOTOWY);

    if (semaphore_lock() == -1)
    {
        perror("Blad semaphore_lock() | samolot wysyła MSG_SAMOLOT GOTOWY\n");
        printf("error samolot %d\n", getpid());
        notify_dyspozytor(MSG_SAMOLOT_POWROT);
        shared_memory_detach(shm_ptr);
        exit(EXIT_FAILURE);
    }

    sleep(5);

    if (!keep_running) 
    {
        printf("Samolot %d: Lot przerwany przed odlotem.\n", getpid());

        if (semaphore_lock() == -1)
        {
            perror("Blad semaphore_lock() | keep_running gotowy");
        }
        else
        {
            shm_ptr -> akt_samoloty -= 1;
            printf("Samolot %d: Liczba aktywnych samolotów: %d\n", getpid(), shm_ptr->akt_samoloty);
            semaphore_unlock();
            printf("Samolot odblokowuje semafor.\n");

        }

        shared_memory_detach(shm_ptr);

        return;
    }

    printf("Samolot %d: Odleciał.\n", getpid());
    sleep(10);

    if (!keep_running)
    {
        printf("Samolot %d: Lot przerwany. Powrót.\n", getpid());

        if (semaphore_lock() == -1)
        {
            perror("Blad semaphore_lock() | keep_running odleciał");
        }
        else
        {
            shm_ptr -> akt_samoloty -= 1;
            printf("Samolot %d: Liczba aktywnych samolotów: %d\n", getpid(), shm_ptr->akt_samoloty);
            semaphore_unlock();
            printf("Samolot odblokowuje semafor.\n");
        }

        notify_dyspozytor(MSG_SAMOLOT_POWROT);

        shared_memory_detach(shm_ptr);
        return;
    }

    printf("Samolot %d: Wraca na lotnisko.\n", getpid());

    if (semaphore_lock() == -1)
    {
        perror("Blad semaphore_lock() | wraca");
    }
    else
    {
        shm_ptr -> akt_samoloty -= 1;
        printf("Samolot %d: Liczba aktywnych samolotów: %d\n", getpid(), shm_ptr->akt_samoloty);
        semaphore_unlock();
        printf("Samolot odblokowuje semafor.\n");
    }

    notify_dyspozytor(MSG_SAMOLOT_POWROT);
    printf("Samolot %d: Koniec lotu.\n", getpid());

    shared_memory_detach(shm_ptr);

}

int main() 
{
    simulate_flight_cycle();
    return 0;
}