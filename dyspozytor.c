#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "global.h"
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
    msg_queue_id = msgget(MSG_QUEUE_KEY, IPC_CREAT | 0666);
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

        if (msg.mtype == MSG_SAMOLOT_GOTOWY)
        {
            printf("Dyspozytor: Samolot o PID %d jest gotowy do startu.\n", msg.samolot_pid);
        }
        else if (msg.mtype == MSG_SAMOLOT_POWROT)
        {
            printf("Dyspozytor: Samolot o PID %d powrócił na lotnisko.\n", msg.samolot_pid);
            powrot_count ++;
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

    initialize_message_queue();

    create_airplanes(MAX_SAMOLOT);

    handle_messages();

    cleanup_message_queue();

    printf("Dyspozytor: Zakończył działanie.\n");
    return 0;
}