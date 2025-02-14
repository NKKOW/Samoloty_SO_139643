#define _POSIX_C_SOURCE 200809L
#include "global.h"
#include "dyspozytor.h"
#include "gate.h"
#include "queue.h"
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
#include <sys/shm.h>
#include <sys/ipc.h>

// Deklaracja kolejki komunikatów, zmienne dla samolotów i pasażerów
int msg_queue_id;
static int dispatcher_socket_fd = -1;

pid_t airplane_pids[MAX_SAMOLOT];
int airplane_count = 0;

#define MAX_PASSENGERS 1000
pid_t passenger_pids[MAX_PASSENGERS];
int passenger_count = 0;

pthread_mutex_t airplane_pids_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile sig_atomic_t keep_running = 1;

static PlaneQueue waiting_planes;

static int shm_id = -1;
static SharedData* shm_ptr = NULL;

static PlaneInfo planeInfos[MAX_SAMOLOT];

// Funkcje pomocnicze
void send_signal_to_airplanes(int sig) {
    pthread_mutex_lock(&airplane_pids_mutex);
    for (int i = 0; i < airplane_count; i++) {
        if (airplane_pids[i] > 0) {
            kill(airplane_pids[i], sig);
        }
    }
    pthread_mutex_unlock(&airplane_pids_mutex);
}

void send_signal_to_assigned_airplanes(int sig) {
    for (int i = 0; i < max_gates; i++) {
        if (gates[i].assigned_samoloty_pid > 0) {
            kill(gates[i].assigned_samoloty_pid, sig);
        }
    }
}

void sigusr_handler(int sig) {
    if (sig == SIGUSR1) {
        printf("Dyspozytor: Otrzymano SIGUSR1 - wymuszenie wcześniejszego odlotu.\n");
        send_signal_to_assigned_airplanes(SIGUSR1);
    } else if (sig == SIGUSR2) {
        printf("Dyspozytor: Otrzymano SIGUSR2 - zakaz dalszego boardingu.\n");
        send_signal_to_assigned_airplanes(SIGUSR2);
        stopBoarding = 1;
    }
}

void sigint_handler(int sig) {
    printf("\nDyspozytor: Odebrano SIGINT (%d). Wysyłam SIGKILL do wszystkich samolotów...\n", sig);
    send_signal_to_airplanes(SIGKILL);
    printf("Dyspozytor: Zakończono działanie.\n");
    exit(0);
}

void sigtstp_handler_dispatcher(int sig) {
    printf("Dyspozytor: Otrzymano SIGTSTP (%d). Wysyłam SIGTSTP do wszystkich samolotów.\n", sig);
    send_signal_to_airplanes(SIGTSTP);
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

void sigchld_handler(int sig) {
    (void)sig;
    while (1) {
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid <= 0)
            break;
    }
}

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

void cleanup_message_queue() {
    if (msgctl(msg_queue_id, IPC_RMID, NULL) == -1) {
        perror("Dyspozytor: błąd msgctl");
    } else {
        printf("Dyspozytor: Czyszczenie kolejki komunikatów.\n");
    }
}

int setup_dispatcher_socket() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Dyspozytor: socket()");
        return -1;
    }
    dispatcher_socket_fd = sock;
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

void close_dispatcher_socket(int sockfd) {
    if (sockfd >= 0) {
        close(sockfd);
        printf("Dyspozytor: Zamknięto socket.\n");
    }
}

pid_t create_airplane(int planeIndex, int baggageLimit) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("Dyspozytor: błąd fork - samolot");
        return -1;
    } else if (pid == 0) {
        char argIndex[32], argMd[32], argCapacity[32], argTi[32];
        snprintf(argIndex, sizeof(argIndex), "%d", planeIndex);
        snprintf(argMd, sizeof(argMd), "%d", baggageLimit);
        snprintf(argCapacity, sizeof(argCapacity), "%d", PLANE_CAPACITY);
        int ti = (rand() % 5) + 5;
        snprintf(argTi, sizeof(argTi), "%d", ti);
        execl("./samolot", "samolot", argIndex, argMd, argCapacity, argTi, NULL);
        perror("Dyspozytor: błąd execl samolot");
        exit(EXIT_FAILURE);
    }
    return pid;
}

pid_t create_passenger(int passenger_id, pid_t plane_pid, int planeMd, int planeIndex) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("Dyspozytor: błąd fork - pasażer");
        return -1;
    } else if (pid == 0) {
        char arg_id[32], arg_plane[32], arg_planeMd[32], arg_planeIndex[32];
        snprintf(arg_id, sizeof(arg_id), "%d", passenger_id);
        snprintf(arg_plane, sizeof(arg_plane), "%d", plane_pid);
        snprintf(arg_planeMd, sizeof(arg_planeMd), "%d", planeMd);
        snprintf(arg_planeIndex, sizeof(arg_planeIndex), "%d", planeIndex);
        execl("./pasazer", "pasazer", arg_id, arg_plane, arg_planeMd, arg_planeIndex, NULL);
        perror("Dyspozytor: błąd execl pasazer");
        _exit(1);
    }
    printf("Dyspozytor: Pasażer id=%d, PID=%d\n", passenger_id, pid);
    if (passenger_count < MAX_PASSENGERS) {
        passenger_pids[passenger_count++] = pid;
    }
    return pid;
}

void create_passengers(int count, pid_t plane_pid, int planeMd, int planeIndex) {
    for (int i = 0; i < count; i++) {
        create_passenger(i + 1, plane_pid, planeMd, planeIndex);
    }
}

void clean_all_passengers() {
    for (int i = 0; i < passenger_count; i++) {
        if (passenger_pids[i] > 0) {
            kill(passenger_pids[i], SIGKILL);
            printf("Dyspozytor: Zabito pasażera PID %d\n", passenger_pids[i]);
        }
    }
}

void try_assign_gates_to_waiting_planes() {
    while (!queue_is_empty(&waiting_planes)) {
        int gate_index = find_free_gate();
        if (gate_index == -1)
            break;
        pid_t plane_pid = queue_dequeue(&waiting_planes);
        int capacity = 0;
        for (int i = 0; i < airplane_count; i++) {
            if (airplane_pids[i] == plane_pid) {
                capacity = planeInfos[i].capacityP;
                break;
            }
        }
        gate_assign(gate_index, plane_pid, capacity);

        wiadomosc_buf reply_msg;
        reply_msg.mtype = plane_pid;
        reply_msg.rodzaj = MSG_GATE_ASSIGN;
        reply_msg.samolot_pid = plane_pid;
        reply_msg.gate_id = gates[gate_index].gate_id;
        if (msgsnd(msg_queue_id, &reply_msg, sizeof(wiadomosc_buf) - sizeof(long), 0) == -1) {
            perror("Dyspozytor: błąd msgsnd MSG_GATE_ASSIGN");
        } else {
            printf("Dyspozytor: Przydzielono gate %d samolotowi PID %d.\n",
                   reply_msg.gate_id, plane_pid);
        }
    }
}

void handle_messages() {
    wiadomosc_buf msg;
    while (keep_running) {
        printf("Dyspozytor: Oczekiwanie na komunikat.\n");
        ssize_t recv_result;
        while (1) {
            recv_result = msgrcv(msg_queue_id, &msg, sizeof(wiadomosc_buf) - sizeof(long), 1, 0);
            if (recv_result == -1) {
                if (errno == EINTR) {
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
        if (msg.rodzaj == 999)
            break;
        printf("Dyspozytor: Otrzymano rodzaj %d od PID %d\n", msg.rodzaj, msg.samolot_pid);
        switch (msg.rodzaj) {
            case MSG_GATE_REQUEST:
                queue_enqueue(&waiting_planes, msg.samolot_pid);
                try_assign_gates_to_waiting_planes();
                break;
            case MSG_SAMOLOT_POWROT:
                gate_release_by_plane(msg.samolot_pid);
                try_assign_gates_to_waiting_planes();
                planes_returned++;
                printf("Dyspozytor: Samolot %d wrócił. (planes_returned=%d/%d)\n",
                       msg.samolot_pid, planes_returned, airplane_count);
                pthread_mutex_lock(&shm_ptr->shm_mutex);
                int boarded = shm_ptr->total_boarded;
                int rejected = shm_ptr->total_rejected;
                pthread_mutex_unlock(&shm_ptr->shm_mutex);
                if (planes_returned == airplane_count &&
                    (boarded + rejected) == total_passengers_assigned) {
                    printf("Dyspozytor: Wszystkie samoloty wróciły i wszyscy pasażerowie są rozliczeni (razem %d).\n",
                           boarded + rejected);
                    keep_running = 0;
                    wiadomosc_buf dummy;
                    dummy.mtype = 1;
                    dummy.rodzaj = 999;
                    dummy.samolot_pid = 0;
                    dummy.gate_id = 0;
                    msgsnd(msg_queue_id, &dummy, sizeof(wiadomosc_buf) - sizeof(long), 0);
                }
                break;
            case MSG_BOARDING_FINISHED:
                printf("Dyspozytor: Boarding zakończony dla samolotu PID %d.\n", msg.samolot_pid);
                break;
            default:
                printf("Dyspozytor: Otrzymano nieznany rodzaj komunikatu %d.\n", msg.rodzaj);
                break;
        }
    }
}

void* signal_sender_thread(void* arg) {
    (void)arg;
    while (keep_running) {
        sleep(30);
        int choice = rand() % 2;
        if (choice == 0) {
            send_signal_to_assigned_airplanes(SIGUSR_DEPART);
        } else {
            send_signal_to_assigned_airplanes(SIGUSR_NO_BOARD);
        }
    }
    return NULL;
}

static void init_shared_memory_dyspozytor() {
    key_t key = ftok(MSG_QUEUE_PATH, 'S');
    if (key == -1) {
        perror("Dyspozytor: ftok (shm)");
        exit(EXIT_FAILURE);
    }
    shm_id = shmget(key, sizeof(SharedData), 0666 | IPC_CREAT);
    if (shm_id < 0) {
        perror("Dyspozytor: shmget");
        exit(EXIT_FAILURE);
    }
    shm_ptr = (SharedData*) shmat(shm_id, NULL, 0);
    if (shm_ptr == (void*)-1) {
        perror("Dyspozytor: shmat");
        exit(EXIT_FAILURE);
    }
    pthread_mutex_lock(&shm_ptr->shm_mutex);
    shm_ptr->total_boarded = 0;
    shm_ptr->total_rejected = 0;
    shm_ptr->licznik_pasazer = 0;
    for (int i = 0; i < MAX_SAMOLOT; i++) {
        shm_ptr->gate_open[i] = 0;
        shm_ptr->boarded[i] = 0;
        shm_ptr->expected[i] = 0;
        sem_init(&shm_ptr->boarding_sem[i], 1, 0);
    }
    pthread_mutex_unlock(&shm_ptr->shm_mutex);
}

static void detach_shared_memory_dyspozytor() {
    if (shm_ptr) {
        shmdt(shm_ptr);
        shm_ptr = NULL;
    }
}

int main() {
    printf("Dyspozytor: Start\n");
    srand(time(NULL));

    struct sigaction sa_int, sa_tstp, sa_chld, sa_usr1, sa_usr2;
    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGINT, &sa_int, NULL);
    sa_tstp.sa_handler = sigtstp_handler_dispatcher;
    sigemptyset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = 0;
    sigaction(SIGTSTP, &sa_tstp, NULL);
    sa_chld.sa_handler = sigchld_handler;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa_chld, NULL);
    sa_usr1.sa_handler = sigusr_handler;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = 0;
    sigaction(SIGUSR1, &sa_usr1, NULL);
    sa_usr2.sa_handler = sigusr_handler;
    sigemptyset(&sa_usr2.sa_mask);
    sa_usr2.sa_flags = 0;
    sigaction(SIGUSR2, &sa_usr2, NULL);

    remove_existing_message_queue();
    initialize_message_queue();
    queue_init(&waiting_planes);
    gate_init(max_gates);
    init_shared_memory_dyspozytor();
    int sockfd = setup_dispatcher_socket();

    pthread_t signal_thread;
    if (pthread_create(&signal_thread, NULL, signal_sender_thread, NULL) != 0) {
        perror("Dyspozytor: nie udało się utworzyć wątku signal_sender_thread");
        exit(EXIT_FAILURE);
    }

    int num_airplanes_to_create = MAX_SAMOLOT;
    for (int i = 0; i < num_airplanes_to_create; i++) {
        planeInfos[i].planeIndex = i;
        planeInfos[i].baggageLimitMd = (rand() % 5) + 8;
        planeInfos[i].capacityP = PLANE_CAPACITY;
        planeInfos[i].returnTimeTi = (rand() % 5) + 5;
    }
    airplane_count = num_airplanes_to_create;

    // Najpierw tworzymy samoloty
    for (int i = 0; i < num_airplanes_to_create; i++) {
        pid_t pid = create_airplane(i, planeInfos[i].baggageLimitMd);
        if (pid > 0) {
            airplane_pids[i] = pid;
            printf("Dyspozytor: Utworzono samolot PID %d (planeIndex=%d, Md=%d)\n",
                   pid, i, planeInfos[i].baggageLimitMd);
        } else {
            fprintf(stderr, "Dyspozytor: Nie udało się utworzyć samolotu.\n");
        }
    }

    // Krótkie opóźnienie, aby samoloty się zainicjowały
    sleep(2);

    // Następnie tworzymy pasażerów – przekazujemy już właściwy PID samolotu
    int total_passengers_to_create = 100;
    for (int i = 0; i < total_passengers_to_create; i++) {
        int randomIndex = rand() % airplane_count;
        int chosenPlaneMd = planeInfos[randomIndex].baggageLimitMd;
        pthread_mutex_lock(&shm_ptr->shm_mutex);
        shm_ptr->expected[randomIndex] += 1;
        pthread_mutex_unlock(&shm_ptr->shm_mutex);
        create_passenger(i + 1, airplane_pids[randomIndex], chosenPlaneMd, randomIndex);
        total_passengers_assigned++;
    }

    handle_messages();

    for (int i = 0; i < passenger_count; i++) {
        if (passenger_pids[i] > 0) {
            kill(passenger_pids[i], SIGKILL);
            printf("Dyspozytor: Zabito pasażera PID %d\n", passenger_pids[i]);
        }
    }

    printf("Dyspozytor: Kończymy pracę.\n");
    send_signal_to_airplanes(SIGKILL);

    for (int i = 0; i < airplane_count; i++) {
        if (airplane_pids[i] > 0) {
            waitpid(airplane_pids[i], NULL, 0);
        }
    }

    pthread_cancel(signal_thread);
    pthread_join(signal_thread, NULL);

    gate_cleanup();
    cleanup_message_queue();
    close_dispatcher_socket(sockfd);
    detach_shared_memory_dyspozytor();

    printf("Dyspozytor: Koniec.\n");
    return 0;
}
