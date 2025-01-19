#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "global.h"
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

    msg_queue_id = msgget(MSG_QUEUE_KEY, 0666);
    if (msg_queue_id == -1)
    {
        perror("Blad msgget samolot\n");
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, sigint_handler);

    notify_dyspozytor(MSG_SAMOLOT_GOTOWY);
    sleep(5);

    if (!keep_running) 
    {
        printf("Samolot %d: Lot przerwany przed odlotem.\n", getpid());
        return;
    }

    printf("Samolot %d: Odleciał.\n", getpid());
    sleep(10);

    if (!keep_running)
    {
        printf("Samolot %d: Lot przerwany przed odlotem.\n", getpid());
        return;
    }

    printf("Samolot %d: Wraca na lotnisko.\n", getpid());

    notify_dyspozytor(MSG_SAMOLOT_POWROT);
    printf("Samolot %d: Koniec lotu.\n", getpid());
}

int main() 
{
    simulate_flight_cycle();
    return 0;
}