// dyspozytor.c

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include "global.h"
#include "dyspozytor.h"
#include "gate.h"
#include "queue.h"

int msg_queue_id;
extern int max_gates;
volatile sig_atomic_t keep_running = 1;
static PlaneQueue waiting_planes;

void remove_existing_message_queue() {
    key_t msg_key = ftok(MSG_QUEUE_PATH, MSG_QUEUE_PROJ);
    if (msg_key == -1) {
        perror("Blad ftok");
        exit(EXIT_FAILURE);
    }
    int existing_msgqid = msgget(msg_key, 0666);
    if (existing_msgqid != -1) {
        if (msgctl(existing_msgqid, IPC_RMID, NULL) == -1) {
            perror("Blad usuwania kolejki");
            exit(EXIT_FAILURE);
        } else {
            printf("Usunieto istniejaca kolejke.\n");
        }
    }
}

void sigint_handler(int sig) {
    printf("\nDyspozytor: SIGINT, konczenie...\n");
    keep_running = 0;
}

void sigchld_handler(int sig) {
    while (1) {
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid <= 0) {
            break;
        }
        printf("Dyspozytor: Proces potomny %d zakonczyl sie.\n", pid);
    }
}

void initialize_message_queue() {
    key_t msg_key = ftok(MSG_QUEUE_PATH, MSG_QUEUE_PROJ);
    if (msg_key == -1) {
        perror("Blad ftok dyspozytor");
        exit(EXIT_FAILURE);
    }
    msg_queue_id = msgget(msg_key, IPC_CREAT | 0666);
    if (msg_queue_id == -1) {
        perror("Blad msgget dyspozytor");
        exit(EXIT_FAILURE);
    }
}

void cleanup_message_queue() {
    if (msgctl(msg_queue_id, IPC_RMID, NULL) == -1) {
        perror("Blad msgctl dyspozytor");
    }
}

void create_airplanes(int num_samoloty) {
    for (int i = 0; i < num_samoloty; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("Blad fork dyspozytor");
            cleanup_message_queue();
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            execl("./samolot", "samolot", NULL);
            perror("Blad execl dyspozytor");
            exit(EXIT_FAILURE);
        }
        printf("Dyspozytor: Utworzono samolot PID %d\n", pid);
    }
}

void try_assign_gates_to_waiting_planes() {
    while (!queue_is_empty(&waiting_planes)) {
        int gate_index = find_free_gate();
        if (gate_index == -1) {
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
            perror("Blad msgsnd MSG_GATE_ASSIGN");
        } else {
            printf("Dyspozytor: Przydzielono gate %d samolotowi %d.\n",
                   reply_msg.gate_id, plane_pid);
        }
    }
}

void handle_messages()
{
    wiadomosc_buf msg;
    int powrot_count = 0;

    while (keep_running && powrot_count < MAX_SAMOLOT)
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
                perror("Blad msgrcv dyspozytor");
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
                powrot_count++;
                gate_release_by_plane(msg.samolot_pid);
                try_assign_gates_to_waiting_planes();
                if (powrot_count >= MAX_SAMOLOT)
                {
                    printf("Dyspozytor: Wszystkie samoloty powrocily.\n");
                    keep_running = 0;
                }
                break;

            default:
                break;
        }
    }
}


int main() {
    printf("Dyspozytor: Start\n");
    struct sigaction sa_int, sa_chld;
    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    if (sigaction(SIGINT, &sa_int, NULL) == -1) {
        perror("Blad sigaction SIGINT");
        exit(EXIT_FAILURE);
    }
    sa_chld.sa_handler = sigchld_handler;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa_chld, NULL) == -1) {
        perror("Blad sigaction SIGCHLD");
        exit(EXIT_FAILURE);
    }
    remove_existing_message_queue();
    initialize_message_queue();
    queue_init(&waiting_planes);
    gate_init(max_gates);
    create_airplanes(MAX_SAMOLOT);
    handle_messages();
    gate_cleanup();
    cleanup_message_queue();
    printf("Dyspozytor: Koniec.\n");
    return 0;
}
