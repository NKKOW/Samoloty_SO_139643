#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include "odprawa.h"
#include "terminal.h"
#include "gate.h"


void log_event(const char *filename, const char *message) 
{
    int fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if(fd == -1) 
    {
        perror("log_event: open");
        return;
    }

    time_t now = time(NULL);
    char time_str[26];
    ctime_r(&now, time_str);
    time_str[strcspn(time_str, "\n")] = '\0';

    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "[%s] %s\n", time_str, message);

    if(write(fd, log_msg, strlen(log_msg)) == -1) 
    {
        perror("log_event: write");
    }

    close(fd);
}

void odprawa_sigint_handler(int sig) 
{
    printf("\nOdprawa: Otrzymano SIGINT, czyszczenie zasobów...\n");
    exit(0);
}


int odprawa_init(Odprawa *odprawa_ptr, int num_samoloty) 
{
    odprawa_ptr -> num_samoloty = num_samoloty;
    odprawa_ptr -> num_samoloty = (num_samoloty + MAX_SAMOLOT_PER_TERMINAL - 1) / MAX_SAMOLOT_PER_TERMINAL;

    // Alokacja pamięci dla terminali
    odprawa_ptr -> terminale = malloc(sizeof(Terminal) * odprawa_ptr -> num_terminale);
    if(!odprawa_ptr -> terminale) 
    {
        perror("odprawa_init: malloc terminals");
        return -1;
    }

    // Inicjalizacja terminali
    int remaining_samoloty = num_samoloty;
    for(int i = 0; i < odprawa_ptr -> num_samoloty; i++) 
    {
        int gates_in_terminal = (remaining_samoloty >= MAX_SAMOLOT_PER_TERMINAL) ? MAX_SAMOLOT_PER_TERMINAL : remaining_samoloty;
        if(terminal_init(&odprawa_ptr -> terminale[i], i + 1, gates_in_terminal) != 0) 
        {
            fprintf(stderr, "odprawa_init: Nie można zainicjalizować terminala %d.\n", i + 1);
            return -1;
        }
        remaining_samoloty -= gates_in_terminal;
    }

    // Inicjalizacja mutexu przydziału
    if(pthread_mutex_init(&odprawa_ptr -> assign_mutex, NULL) != 0) 
    {
        perror("odprawa_init: pthread_mutex_init assign_mutex");
        return -1;
    }

    // Alokacja pamięci dla wątków terminali
    odprawa_ptr -> terminal_threads = malloc(sizeof(pthread_t) * odprawa_ptr -> num_terminale);
    if(!odprawa_ptr -> terminal_threads) 
    {
        perror("odprawa_init: malloc terminal_threads");
        return -1;
    }

    // Tworzenie wątków terminali
    for(int i = 0; i < odprawa_ptr -> num_terminale; i++) 
    {
        if(pthread_create(&odprawa_ptr -> terminal_threads[i], NULL, terminal_thread_func, &odprawa_ptr -> terminale[i]) != 0) 
        {
            perror("odprawa_init: pthread_create terminal");
            return -1;
        }
    }

    // Alokacja pamięci dla wątków gate'ów
    int total_gates = 0;
    for(int i = 0; i < odprawa_ptr -> num_terminale; i++) 
    {
        total_gates += odprawa_ptr -> terminale[i].num_gates;
    }

    odprawa_ptr -> gate_threads = malloc(sizeof(pthread_t) * total_gates);
    if(!odprawa_ptr -> gate_threads) 
    {
        perror("odprawa_init: malloc gate_threads");
        return -1;
    }

    // Tworzenie wątków gate'ów
    int gate_index = 0;
    for(int i = 0; i < odprawa_ptr -> num_terminale; i++) 
    {
        for(int j = 0; j < odprawa_ptr -> terminale[i].num_gates; j++) 
        {
            if(pthread_create(&odprawa_ptr -> gate_threads[gate_index], NULL, gate_thread_func, &odprawa_ptr -> terminale[i].gates[j]) != 0) 
            {
                perror("odprawa_init: pthread_create gate");
                return -1;
            }
            gate_index++;
        }
    }

    // Inicjalizacja kolejki komunikatów
    key_t msg_key = ftok(MSG_QUEUE_PATH, 'O'); 
    if(msg_key == -1) 
    {
        perror("odprawa_init: ftok");
        return -1;
    }

    odprawa_ptr -> msg_queue_id = msgget(msg_key, IPC_CREAT | 0666);
    if(odprawa_ptr -> msg_queue_id == -1) 
    {
        perror("odprawa_init: msgget");
        return -1;
    }

    odprawa_ptr->running = true;

    // Rejestracja obsługi sygnału SIGINT
    struct sigaction sa;
    sa.sa_handler = odprawa_sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if(sigaction(SIGINT, &sa, NULL) == -1) 
    {
        perror("odprawa_init: sigaction");
        return -1;
    }

    // Tworzenie wątku obsługującego wiadomości
    pthread_t msg_thread;
    if(pthread_create(&msg_thread, NULL, (void*(*)(void*)) &handle_messages, odprawa_ptr) != 0) 
    {
        perror("odprawa_init: pthread_create msg_thread");
        return -1;
    }

    pthread_detach(msg_thread);

    printf("Odprawa: Inicjalizacja zakończona. Terminale i gate'y są gotowe.\n");
    log_event("odprawa.log", "Odprawa zainicjalizowana.");

    return 0;
}

void odprawa_cleanup(Odprawa *odprawa_ptr) 
{
    // Zatrzymywanie wątków terminali
    for(int i = 0; i < odprawa_ptr -> num_terminale; i++) 
    {
        pthread_cancel(odprawa_ptr -> terminal_threads[i]);
        pthread_join(odprawa_ptr -> terminal_threads[i], NULL);
        pthread_mutex_destroy(&odprawa_ptr -> terminale[i].terminal_mutex);
        pthread_cond_destroy(&odprawa_ptr -> terminale[i].terminal_cond);

        // Zwalnianie mutexów gate'ów w terminalu
        for(int j = 0; j < odprawa_ptr -> terminale[i].num_gates; j++) 
        {
            pthread_mutex_destroy(&odprawa_ptr -> terminale[i].gates[j].gate_mutex);
        }

        free(odprawa_ptr -> terminale[i].gates);
    }

    // Zatrzymywanie wątków gate'ów
    int total_gates = 0;
    for(int i = 0; i < odprawa_ptr -> num_terminale; i++) 
    {
        total_gates += odprawa_ptr -> terminale[i].num_gates;
    }

    for(int i = 0; i < total_gates; i++) 
    {
        pthread_cancel(odprawa_ptr -> gate_threads[i]);
        pthread_join(odprawa_ptr-> gate_threads[i], NULL);
    }

    // Zwalnianie mutexu przydziału
    pthread_mutex_destroy(&odprawa_ptr -> assign_mutex);

    // Zwolnienie pamięci dla terminali i wątków
    free(odprawa_ptr -> terminale);
    free(odprawa_ptr -> terminal_threads);
    free(odprawa_ptr -> gate_threads);

    // Usunięcie kolejki komunikatów
    if(msgctl(odprawa_ptr -> msg_queue_id, IPC_RMID, NULL) == -1) 
    {
        perror("odprawa_cleanup: msgctl");
    }

    log_event("odprawa.log", "Odprawa została wyczyszczona.");
    printf("Odprawa: Zasoby zostały wyczyszczone.\n");
}

// Funkcja przydzielająca samolot do terminala i gate'a
bool assign_samolot(Odprawa *odprawa_ptr, pid_t samolot_pid, Terminal **assigned_terminal, Gate **assigned_gate) 
{
    pthread_mutex_lock(&odprawa_ptr -> assign_mutex);

    // Szukanie dostępnego terminala
    for(int i = 0; i < odprawa_ptr -> num_terminale; i++) 
    {
        pthread_mutex_lock(&odprawa_ptr->terminale[i].terminal_mutex);
        if(odprawa_ptr->terminale[i].current_samoloty < MAX_SAMOLOT_PER_TERMINAL) 
        {
            odprawa_ptr->terminale[i].current_samoloty += 1;
            *assigned_terminal = &odprawa_ptr->terminale[i];
            pthread_mutex_unlock(&odprawa_ptr->terminale[i].terminal_mutex);
            break;
        }
        pthread_mutex_unlock(&odprawa_ptr->terminale[i].terminal_mutex);
    }

    // Sprawdzenie czy znaleziono terminal
    if(*assigned_terminal == NULL) 
    {
        pthread_mutex_unlock(&odprawa_ptr->assign_mutex);
        return false;
    }

    // Szukanie wolnego gate'a w przydzielonym terminalu
    for(int i = 0; i < (*assigned_terminal)->num_gates; i++) 
    {
        Gate *gate = &(*assigned_terminal)->gates[i];
        if(pthread_mutex_trylock(&gate->gate_mutex) == 0) 
        {
            if(gate->samolot_pid == -1) 
            {
                gate->samolot_pid = samolot_pid;
                *assigned_gate = gate;
                pthread_mutex_unlock(&gate->gate_mutex);
                pthread_mutex_unlock(&odprawa_ptr->assign_mutex);
                return true;
            }
            pthread_mutex_unlock(&gate->gate_mutex);
        }
    }

    // Jeśli nie ma wolnych gate'ów, zwracamy fałsz
    pthread_mutex_lock(&(*assigned_terminal)->terminal_mutex);
    (*assigned_terminal) -> current_samoloty -= 1;
    pthread_mutex_unlock(&(*assigned_terminal)->terminal_mutex);
    pthread_mutex_unlock(&odprawa_ptr->assign_mutex);
    return false;
}

// Funkcja zwalniająca terminal i gate po zakończeniu obsługi samolotu
void release_resources(Odprawa *odprawa_ptr, Terminal *terminal, Gate *gate) 
{
    // Zwalnianie gate'a
    pthread_mutex_lock(&gate -> gate_mutex);
    gate -> samolot_pid = -1;
    pthread_mutex_unlock(&gate -> gate_mutex);

    // Zwalnianie terminala
    pthread_mutex_lock(&terminal -> terminal_mutex);
    terminal -> current_samoloty -= 1;
    pthread_cond_signal(&terminal -> terminal_cond);
    pthread_mutex_unlock(&terminal -> terminal_mutex);

    // Logowanie zwolnienia zasobów
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Zwolniono Terminal %d i Gate %d.", terminal->terminal_id, gate->gate_id);
    log_event("odprawa.log", log_msg);
}

void handle_messages(Odprawa *odprawa_ptr) 
{
    while(odprawa_ptr->running) 
    {
        wiadomosc_buf msg;
        ssize_t received = msgrcv(odprawa_ptr->msg_queue_id, &msg, sizeof(wiadomosc_buf) - sizeof(long), 0, 0);
        if(received == -1) 
        {
            if(errno == EINTR) 
            {
                continue;
            }
            perror("odprawa: msgrcv");
            continue;
        }

        printf("Odprawa: Otrzymano wiadomość typu %ld od samolotu PID %d\n", msg.mtype, msg.samolot_pid);
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Otrzymano wiadomość typu %ld od samolotu PID %d.", msg.mtype, msg.samolot_pid);
        log_event("odprawa.log", log_msg);

        if(msg.mtype == MSG_SAMOLOT_GOTOWY) 
        {
            printf("Odprawa: Samolot PID %d jest gotowy do odprawy.\n", msg.samolot_pid);
            log_event("odprawa.log", "Samolot gotowy do odprawy.");

            Terminal *assigned_terminal = NULL;
            Gate *assigned_gate = NULL;

            if(assign_samolot(odprawa_ptr, msg.samolot_pid, &assigned_terminal, &assigned_gate)) 
            {
                printf("Odprawa: Przydzielono Terminal %d i Gate %d dla samolotu PID %d\n", 
                        assigned_terminal->terminal_id, assigned_gate->gate_id, msg.samolot_pid);

                snprintf(log_msg, sizeof(log_msg), "Przydzielono Terminal %d i Gate %d dla samolotu PID %d.", 
                         assigned_terminal->terminal_id, assigned_gate->gate_id, msg.samolot_pid);

                log_event("odprawa.log", log_msg);
            } 
            else 
            {
                printf("Odprawa: Brak dostępnych zasobów dla samolotu PID %d\n", msg.samolot_pid);
                log_event("odprawa.log", "Brak dostępnych zasobów dla samolotu.");
            }
        }
        else if(msg.mtype == MSG_SAMOLOT_POWROT) 
        {
            printf("Odprawa: Samolot PID %d powrócił na lotnisko.\n", msg.samolot_pid);
            log_event("odprawa.log", "Samolot powrócił na lotnisko.");
        }
    }
}

// Główna funkcja odprawy
int main(int argc, char *argv[]) 
{
    if(argc != 1) 
    {
        fprintf(stderr, "Użycie: %s\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Inicjalizacja odprawy
    Odprawa odprawa_instance;
    if(odprawa_init(&odprawa_instance, 0) == -1) 
    {
        fprintf(stderr, "Odprawa: Nie można zainicjalizować odprawy.\n");
        exit(EXIT_FAILURE);
    }

    // Główna pętla odprawy obsługująca wiadomości
    handle_messages(&odprawa_instance);

    // Czyszczenie zasobów
    odprawa_cleanup(&odprawa_instance);

    return EXIT_SUCCESS;
}
   
