#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>   
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#include "global.h"
#include "dyspozytor.h"
#include "gate.h"
#include "queue.h"

// Globalne zmienne
int msg_queue_id;
extern int max_gates;
volatile sig_atomic_t keep_running = 1;
static PlaneQueue waiting_planes;

// Zmienna globalna do socketu (przykład użycia)
static int dispatcher_socket_fd = -1;

// Tablica do śledzenia PID samolotów
pid_t airplane_pids[MAX_SAMOLOT];
int airplane_count = 0;

// Mutex do synchronizacji dostępu do airplane_pids
pthread_mutex_t airplane_pids_mutex = PTHREAD_MUTEX_INITIALIZER;

// Funkcja do wysyłania sygnału do wszystkich samolotów
void send_signal_to_airplanes(int sig) {
    pthread_mutex_lock(&airplane_pids_mutex);
    printf("Dyspozytor: Wysyłam sygnał %d do wszystkich samolotów.\n", sig);
    for (int i = 0; i < airplane_count; i++) {
        if (airplane_pids[i] > 0) {
            printf("Dyspozytor: Przygotowuję się do wysłania sygnału %d do samolotu PID %d.\n", sig, airplane_pids[i]);
            if (kill(airplane_pids[i], sig) == -1) {
                perror("Dyspozytor: błąd wysyłania sygnału do samolotu");
            } else {
                printf("Dyspozytor: Wysłano sygnał %d do samolotu PID %d.\n", sig, airplane_pids[i]);
            }
        }
    }
    pthread_mutex_unlock(&airplane_pids_mutex);
} 


// Funkcja do wysyłania sygnału do samolotów przypisanych do bramek
void send_signal_to_assigned_airplanes(int sig) {
    printf("Dyspozytor: Wysyłam sygnał %d do samolotów przypisanych do bramek.\n", sig);

    for (int i = 0; i < max_gates; i++) {
        if (gates[i].assigned_samoloty_pid > 0) {
            pid_t plane_pid = gates[i].assigned_samoloty_pid;

            printf("Dyspozytor: Przygotowuję się do wysłania sygnału %d do samolotu PID %d (bramka %d).\n",
                   sig, plane_pid, gates[i].gate_id);

            if (kill(plane_pid, sig) == -1) {
                perror("Dyspozytor: błąd wysyłania sygnału do samolotu");
            } else {
                printf("Dyspozytor: Wysłano sygnał %d do samolotu PID %d (bramka %d).\n",
                       sig, plane_pid, gates[i].gate_id);
            }
        }
    }
}


// Handler dla SIGUSR1 i SIGUSR2
void sigusr_handler(int sig) {
    if (sig == SIGUSR1) {
        printf("Dyspozytor: Otrzymano sygnał SIGUSR1 - wymuszenie wcześniejszego odlotu.\n");
        send_signal_to_assigned_airplanes(SIGUSR1);
    } else if (sig == SIGUSR2) {
        printf("Dyspozytor: Otrzymano sygnał SIGUSR2 - zakaz dalszego boardingu.\n");
        send_signal_to_assigned_airplanes(SIGUSR2);
        stopBoarding = 1; // Ustawienie flagi zatrzymującej boarding
    }
}

// Handler dla SIGINT
void sigint_handler(int sig) {
    printf("\nDyspozytor: Odebrano SIGINT (%d). Wysyłam SIGKILL do wszystkich samolotów...\n", sig);
    send_signal_to_airplanes(SIGKILL);
    printf("Dyspozytor: Zakończono działanie.\n");
    exit(0);
}

// Handler dla SIGTSTP w dyspozytorze
void sigtstp_handler_dispatcher(int sig) {
    printf("Dyspozytor: Otrzymano SIGTSTP (%d). Wysyłam SIGTSTP do wszystkich samolotów.\n", sig);
    send_signal_to_airplanes(SIGTSTP);
    printf("Dyspozytor: STOP.\n");

    // Przywracanie domyślnego handlera dla SIGTSTP
    struct sigaction sa_default;
    sa_default.sa_handler = SIG_DFL;
    sigemptyset(&sa_default.sa_mask);
    sa_default.sa_flags = 0;
    if (sigaction(SIGTSTP, &sa_default, NULL) == -1) {
        perror("Dyspozytor: błąd sigaction SIGTSTP");
        exit(EXIT_FAILURE);
    }

    raise(SIGTSTP);
}

// Handler dla SIGCHLD
void sigchld_handler(int sig) {
    printf("Dyspozytor: Odebrano SIGCHLD (%d)\n", sig);
    while (1) {
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid <= 0) {
            break;
        }
        printf("Dyspozytor: Proces potomny PID %d zakończył się.\n", pid);
        // Usuwanie PID z tablicy
        for (int i = 0; i < airplane_count; i++) {
            if (airplane_pids[i] == pid) {
                // Przesuwamy pozostałe PIDy
                for (int j = i; j < airplane_count -1; j++) {
                    airplane_pids[j] = airplane_pids[j+1];
                }
                airplane_pids[airplane_count -1] = 0;
                airplane_count--;
                break;
            }
        }
    }
}

// Funkcja do usuwania istniejącej kolejki komunikatów
void remove_existing_message_queue() {
    key_t msg_key = ftok(MSG_QUEUE_PATH, MSG_QUEUE_PROJ);
    if (msg_key == -1) {
        perror("Dyspozytor: błąd ftok");
        exit(EXIT_FAILURE);
    }
    int existing_msgqid = msgget(msg_key, 0666);
    if (existing_msgqid != -1) {
        if (msgctl(existing_msgqid, IPC_RMID, NULL) == -1) {
            perror("Dyspozytor: błąd usuwania kolejki");
            exit(EXIT_FAILURE);
        } else {
            printf("Dyspozytor: Usunięto istniejącą kolejkę.\n");
        }
    }
}

// Funkcja do inicjalizacji kolejki komunikatów
void initialize_message_queue() {
    key_t msg_key = ftok(MSG_QUEUE_PATH, MSG_QUEUE_PROJ);
    if (msg_key == -1) {
        perror("Dyspozytor: błąd ftok");
        exit(EXIT_FAILURE);
    }
    msg_queue_id = msgget(msg_key, IPC_CREAT | 0666);
    if (msg_queue_id == -1) {
        perror("Dyspozytor: błąd msgget");
        exit(EXIT_FAILURE);
    }
}

// Funkcja do czyszczenia kolejki komunikatów
void cleanup_message_queue() {
    if (msgctl(msg_queue_id, IPC_RMID, NULL) == -1) {
        perror("Dyspozytor: błąd msgctl");
    } else {
        printf("Dyspozytor: Czyszczenie kolejki komunikatów.\n");
    }
}

// Funkcja do ustawienia socketu dyspozytora (opcjonalnie)
int setup_dispatcher_socket() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Dyspozytor: socket()");
        return -1;
    }
    dispatcher_socket_fd = sock;

    // Bindowanie do dowolnego portu
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(12345);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Dyspozytor: bind()");
        close(sock);
        return -1;
    }
    if (listen(sock, 5) < 0) {
        perror("Dyspozytor: listen()");
        close(sock);
        return -1;
    }
    printf("Dyspozytor: Uruchomiono socket nasłuchujący na porcie 12345.\n");
    return sock;
}

// Funkcja do zamknięcia socketu dyspozytora
void close_dispatcher_socket(int sockfd) {
    close(sockfd);
    printf("Dyspozytor: Zamknięto socket.\n");
}

// Funkcja do tworzenia samolotu
pid_t create_airplane() {
    pid_t pid = fork();
    if (pid < 0) {
        perror("Dyspozytor: błąd fork");
        return -1;
    } else if (pid == 0) {
        if (setpgid(0, getppid()) == -1) {
            perror("Dyspozytor: błąd setpgid w samolocie");
            exit(EXIT_FAILURE);
        }
        execl("./samolot", "samolot", NULL);
        perror("Dyspozytor: błąd execl");
        exit(EXIT_FAILURE);
    }
    printf("Dyspozytor: Utworzono samolot PID %d\n", pid);
    return pid;
}

// Funkcja do tworzenia wielu samolotów
void create_airplanes(int num_samoloty) {
    for (int i = 0; i < num_samoloty; i++) {
        if (airplane_count < MAX_SAMOLOT) {
            pid_t pid = create_airplane();
            if (pid > 0) {
                airplane_pids[airplane_count++] = pid;
            } else {
                fprintf(stderr, "Dyspozytor: Nie udało się utworzyć samolotu.\n");
            }
        } else {
            fprintf(stderr, "Dyspozytor: Przekroczono maksymalną liczbę samolotów (%d).\n", MAX_SAMOLOT);
            break;
        }
    }
}

// Funkcja do przypisywania bramek do oczekujących samolotów
void try_assign_gates_to_waiting_planes() {
    while (!queue_is_empty(&waiting_planes)) {
        int gate_index = find_free_gate();
        if (gate_index == -1) {
            // Brak wolnych bramek
            break;
        }
        pid_t plane_pid = queue_dequeue(&waiting_planes);
        gate_assign(gate_index, plane_pid);
        wiadomosc_buf reply_msg;
        reply_msg.mtype = plane_pid;
        reply_msg.rodzaj = MSG_GATE_ASSIGN;
        reply_msg.samolot_pid = 0;
        reply_msg.gate_id = gates[gate_index].gate_id;
        if (msgsnd(msg_queue_id, &reply_msg,
                   sizeof(wiadomosc_buf) - sizeof(long), 0) == -1) {
            perror("Dyspozytor: błąd msgsnd MSG_GATE_ASSIGN");
        } else {
            printf("Dyspozytor: Przydzielono gate %d samolotowi PID %d.\n",
                   reply_msg.gate_id, plane_pid);
        }
    }
}

// Funkcja do obsługi wiadomości z samolotów
void handle_messages()
{
    wiadomosc_buf msg;

    while (keep_running)
    {
        printf("Dyspozytor: Oczekiwanie na komunikat.\n");

        ssize_t recv_result;
        while (1)
        {
            recv_result = msgrcv(msg_queue_id, &msg,
                                 sizeof(wiadomosc_buf) - sizeof(long),
                                 0, 0);
            if (recv_result == -1)
            {
                if (errno == EINTR)
                {
                    if (!keep_running)
                        break;
                    continue;
                }
                perror("Dyspozytor: błąd msgrcv");
                continue;
            }
            break;
        }

        if (!keep_running || recv_result == -1)
            break;

        printf("Dyspozytor: Otrzymano rodzaj %d od PID %d\n",
               msg.rodzaj, msg.samolot_pid);

        switch (msg.rodzaj)
        {
            case MSG_GATE_REQUEST:
                queue_enqueue(&waiting_planes, msg.samolot_pid);
                try_assign_gates_to_waiting_planes();
                break;

            case MSG_SAMOLOT_POWROT:
                gate_release_by_plane(msg.samolot_pid);
                try_assign_gates_to_waiting_planes();
                break;

            case MSG_BOARDING_FINISHED: // Nowy typ komunikatu
                printf("Dyspozytor: Boarding zakończony dla samolotu PID %d.\n", msg.samolot_pid);
                // Możesz dodać dodatkową logikę tutaj
                break;

            default:
                printf("Dyspozytor: Otrzymano nieznany rodzaj komunikatu %d.\n", msg.rodzaj);
                break;
        }
    }
}

// Funkcja wątku do wysyłania sygnałów
void* signal_sender_thread(void* arg) {
    (void)arg; 
    while (keep_running) {
        sleep(30); // Czas pomiędzy wysyłaniem sygnałów, np. co 30 sekund

        // Losowo zdecyduj, który sygnał wysłać
        int choice = rand() % 2;
        if (choice == 0) {
            // Wysyłanie SIGUSR1
            send_signal_to_assigned_airplanes(SIGUSR_DEPART);
        } else {
            // Wysyłanie SIGUSR2
            send_signal_to_assigned_airplanes(SIGUSR_NO_BOARD);
        }
    }
    return NULL;
}

int main() {
    printf("Dyspozytor: Start\n");
    struct sigaction sa_int, sa_tstp, sa_chld, sa_usr1, sa_usr2;

    // Ustawienie dispatcher jako lider grupy procesów
    pid_t pid = getpid();
    if (setpgid(pid, pid) == -1) {
        perror("Dyspozytor: setpgid failed");
        exit(EXIT_FAILURE);
    }

    // Ustawienie grupy procesów jako foreground dla terminala
    if (tcsetpgrp(STDIN_FILENO, getpgid(pid)) == -1) {
        perror("Dyspozytor: tcsetpgrp failed");
        // Kontynuacja nawet jeśli ustawienie nie powiedzie się
    }

    // Inicjalizacja random seed
    srand(time(NULL));

    // Inicjalizacja airplane_pids
    memset(airplane_pids, 0, sizeof(airplane_pids));

    // Handler dla SIGINT
    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    if (sigaction(SIGINT, &sa_int, NULL) == -1) {
        perror("Dyspozytor: błąd sigaction SIGINT");
        exit(EXIT_FAILURE);
    }

    // Handler dla SIGTSTP
    sa_tstp.sa_handler = sigtstp_handler_dispatcher;
    sigemptyset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = 0;
    if (sigaction(SIGTSTP, &sa_tstp, NULL) == -1) {
        perror("Dyspozytor: błąd sigaction SIGTSTP");
        exit(EXIT_FAILURE);
    }

    // Handler dla SIGCHLD
    sa_chld.sa_handler = sigchld_handler;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa_chld, NULL) == -1) {
        perror("Dyspozytor: błąd sigaction SIGCHLD");
        exit(EXIT_FAILURE);
    }

    // Handlery dla SIGUSR1 i SIGUSR2
    sa_usr1.sa_handler = sigusr_handler;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = 0;
    if (sigaction(SIGUSR1, &sa_usr1, NULL) == -1) {
        perror("Dyspozytor: błąd sigaction SIGUSR1");
        exit(EXIT_FAILURE);
    }

    sa_usr2.sa_handler = sigusr_handler;
    sigemptyset(&sa_usr2.sa_mask);
    sa_usr2.sa_flags = 0;
    if (sigaction(SIGUSR2, &sa_usr2, NULL) == -1) {
        perror("Dyspozytor: błąd sigaction SIGUSR2");
        exit(EXIT_FAILURE);
    }

    remove_existing_message_queue();
    initialize_message_queue();
    queue_init(&waiting_planes);
    gate_init(max_gates);

    //Socket
    int sockfd = setup_dispatcher_socket();
    if (sockfd < 0) {
        fprintf(stderr, "Dyspozytor: Nie udało się utworzyć socketu. Kontynuacja bez socketu.\n");
    }

    // Tworzenie wątku do wysyłania sygnałów
    pthread_t signal_thread;
    if (pthread_create(&signal_thread, NULL, signal_sender_thread, NULL) != 0) {
        perror("Dyspozytor: nie udało się utworzyć wątku signal_sender_thread");
        exit(EXIT_FAILURE);
    }

    // Tworzymy samoloty
    create_airplanes(MAX_SAMOLOT); // MAX_SAMOLOT powinien być zdefiniowany w global.h

    // Obsługa wiadomości
    handle_messages();

    // Po zakończeniu obsługi wiadomości, wysyłamy SIGKILL do wszystkich samolotów
    send_signal_to_airplanes(SIGKILL);

    // Oczekujemy na zakończenie wszystkich procesów potomnych
    for (int i = 0; i < airplane_count; i++) {
        if (airplane_pids[i] > 0) {
            printf("Dyspozytor: Oczekiwanie na zakończenie samolotu PID %d.\n", airplane_pids[i]);
            waitpid(airplane_pids[i], NULL, 0);
        }
    }

    // Zatrzymanie wątku sygnałów
    pthread_cancel(signal_thread);
    pthread_join(signal_thread, NULL);

    gate_cleanup();
    cleanup_message_queue();

    // Zamykamy ewentualnie nasz socket:
    if (sockfd >= 0) {
        close_dispatcher_socket(sockfd);
    }

    printf("Dyspozytor: Koniec.\n");
    return 0;
}
