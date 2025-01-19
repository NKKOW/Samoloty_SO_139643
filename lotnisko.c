#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include "dyspozytor.h"
#include "global.h"

// Funkcja obsługi sygnału SIGINT
void sigint_handler(int sig) 
{
    printf("\nLotnisko: Otrzymano SIGINT, czyszczenie zasobów...\n");
    exit(0);
}

int main() {
    printf("Lotnisko: Startuje...\n");

    // Rejestracja obsługi sygnału SIGINT
    signal(SIGINT, sigint_handler);

      // Tworzenie dyspozytora jako osobny proces
    pid_t dyspozytor_pid = fork();
    if (dyspozytor_pid < 0) 
    {
        perror("Blad fork dla dyspozytora");
        exit(EXIT_FAILURE);
    } 
    else if (dyspozytor_pid == 0) 
    {
        execl("./dyspozytor", "dyspozytor", NULL);
        perror("Blad execl dla dyspozytora");
        exit(EXIT_FAILURE);
    } 
    else 
    {
        // Proces lotniska kontynuuje
        printf("Lotnisko: Uruchomiono dyspozytora o PID %d\n", dyspozytor_pid);
    }

     // Oczekiwanie na zakończenie dyspozytora
    pid_t pid = waitpid(dyspozytor_pid, NULL, 0);
    if (pid > 0) 
    {
        printf("Lotnisko: Dyspozytor o PID %d zakończył działanie.\n", pid);
    }

    printf("Lotnisko: Zakończył działanie.\n");
    return 0;
}