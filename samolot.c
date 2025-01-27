// samolot.c

#include "samolot.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/shm.h>        // Segmenty pamięci dzielonej
#include <sys/ipc.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#include "global.h"
#include "pasazer.h"

// Zmienne globalne
int msg_queue_id;
volatile sig_atomic_t keep_running = 1;
volatile sig_atomic_t early_depart = 0; // Flaga do wczesnego odlotu

// Zmienne do pamięci dzielonej
static int shm_id = -1;
static SharedData* shm_ptr = NULL;

// Handler dla SIGALRM
void sigalrm_handler(int sig) {
    (void)sig; // Zapobiega ostrzeżeniu o nieużywanym parametrze
    printf("Samolot %d: Otrzymano SIGALRM - czas T1 upłynął, przygotowanie do odlotu.\n", getpid());
    early_depart = 0; // Normalny odlot
    keep_running = 0;
}

// Handler dla SIGTSTP
void sigtstp_handler(int sig) {
    printf("Samolot %d: Otrzymano sygnał %d (SIGTSTP), zatrzymywanie procesu.\n", getpid(), sig);
    // Przywracamy domyślne zachowanie dla SIGTSTP
    struct sigaction sa_default;
    sa_default.sa_handler = SIG_DFL;
    sigemptyset(&sa_default.sa_mask);
    sa_default.sa_flags = 0;
    if (sigaction(SIGTSTP, &sa_default, NULL) == -1) {
        perror("Samolot: blad sigaction SIGTSTP");
        exit(EXIT_FAILURE);
    }
    // Ponowne wysłanie SIGTSTP, aby zatrzymać proces
    raise(SIGTSTP);
}

// Handler dla SIGUSR1 i SIGUSR2
void sigusr_handler(int sig) {
    if (sig == SIGUSR1) {
        printf("Samolot %d: Otrzymano sygnał SIGUSR1 - wymuszenie wczesniejszego odlotu.\n", getpid());
        early_depart = 1;
        keep_running = 0;
    } else if (sig == SIGUSR2) {
        printf("Samolot %d: Otrzymano sygnał SIGUSR2 - zakaz dalszego boardingu.\n", getpid());
        keep_running = 0;
    }
}

// Funkcja do ignorowania SIGINT w samolocie
void ignore_sigint() {
    struct sigaction sa_ignore;
    sa_ignore.sa_handler = SIG_IGN;
    sigemptyset(&sa_ignore.sa_mask);
    sa_ignore.sa_flags = 0;
    if (sigaction(SIGINT, &sa_ignore, NULL) == -1) {
        perror("Samolot: błąd sigaction SIGINT");
        exit(EXIT_FAILURE);
    }
}

// Tworzenie / przyłączanie pamięci dzielonej
static void init_shared_memory() {
    // Używamy klucza z pliku:
    key_t key = ftok(MSG_QUEUE_PATH, 'S');
    if (key == -1) {
        perror("Samolot: blad ftok for shm");
        exit(EXIT_FAILURE);
    }

    // Tworzymy / uzyskujemy segment
    shm_id = shmget(key, sizeof(SharedData), 0666 | IPC_CREAT);
    if (shm_id < 0) {
        perror("Samolot: blad shmget");
        exit(EXIT_FAILURE);
    }

    // Przyłączamy
    shm_ptr = (SharedData*) shmat(shm_id, NULL, 0);
    if (shm_ptr == (void*) -1) {
        perror("Samolot: blad shmat");
        exit(EXIT_FAILURE);
    }

    // Inicjalizacja mutexa w pamięci dzielonej (tylko jeśli jest to pierwsze połączenie)
    // Zakładamy, że inny proces inicjalizuje mutex
}

// Odłączanie pamięci (na koniec)
static void detach_shared_memory() {
    if (shm_ptr) {
        // Nie niszczymy mutexa, ponieważ może być używany przez inne procesy
        shmdt(shm_ptr);
        shm_ptr = NULL;
    }
    // Nie usuwamy segmentu, bo może jeszcze być potrzebny
    // do innych samolotów w projekcie
}

// Funkcja wysyłająca komunikaty do dyspozytora
void notify_dyspozytor(rodzaj_wiadomosc msg_type) {
    wiadomosc_buf msg;
    msg.mtype = 1;
    msg.rodzaj = msg_type;
    msg.samolot_pid = getpid();
    msg.gate_id = -1;
    if (msgsnd(msg_queue_id, &msg, sizeof(wiadomosc_buf) - sizeof(long), 0) == -1) {
        perror("Samolot: blad msgsnd");
        exit(EXIT_FAILURE);
    }
}

void simulate_flight_cycle() {
    printf("Samolot %d: Start cyklu lotu\n", getpid());

    // Usunięto ustawianie nowej grupy procesów dla samolotu
    // if (setpgid(0, 0) == -1) {
    //     perror("Samolot: blad setpgid");
    //     exit(EXIT_FAILURE);
    // }

    // Ignorowanie SIGINT
    ignore_sigint();

    // Setup signal handlers
    struct sigaction sa_usr, sa_alarm, sa_tstp;

    // Obsługa SIGUSR1 i SIGUSR2
    sa_usr.sa_handler = sigusr_handler;
    sigemptyset(&sa_usr.sa_mask);
    sa_usr.sa_flags = 0;
    if (sigaction(SIGUSR1, &sa_usr, NULL) == -1) {
        perror("Samolot: blad sigaction SIGUSR1");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGUSR2, &sa_usr, NULL) == -1) {
        perror("Samolot: blad sigaction SIGUSR2");
        exit(EXIT_FAILURE);
    }

    // Obsługa SIGALRM
    sa_alarm.sa_handler = sigalrm_handler;
    sigemptyset(&sa_alarm.sa_mask);
    sa_alarm.sa_flags = 0;
    if (sigaction(SIGALRM, &sa_alarm, NULL) == -1) {
        perror("Samolot: blad sigaction SIGALRM");
        exit(EXIT_FAILURE);
    }

    // Obsługa SIGTSTP
    sa_tstp.sa_handler = sigtstp_handler;
    sigemptyset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = 0;
    if (sigaction(SIGTSTP, &sa_tstp, NULL) == -1) {
        perror("Samolot: blad sigaction SIGTSTP");
        exit(EXIT_FAILURE);
    }

    // Inicjalizacja pamięci dzielonej
    init_shared_memory();

    // Inicjalizacja semaforów do schodów
    if (sem_init(&stairsSemNormal, 0, 1) == -1) { // Shared między wątkami w tym samym procesie
        perror("Samolot: blad inicjalizacji semafora stairsSemNormal");
        detach_shared_memory();
        exit(EXIT_FAILURE);
    }
    if (sem_init(&stairsSemVip, 0, 1) == -1) { // Shared między wątkami w tym samym procesie
        perror("Samolot: blad inicjalizacji semafora stairsSemVip");
        sem_destroy(&stairsSemNormal);
        detach_shared_memory();
        exit(EXIT_FAILURE);
    }

    // Inicjalizacja semafora do wagi bagażowej
    if (sem_init(&bagaz_wagaSem, 0, 1) == -1) { // Shared między wątkami w tym samym procesie
        perror("Samolot: blad inicjalizacji semafora bagaz_wagaSem");
        sem_destroy(&stairsSemNormal);
        sem_destroy(&stairsSemVip);
        detach_shared_memory();
        exit(EXIT_FAILURE);
    }

    // Inicjalizacja mutexa i warunku
    if (pthread_mutex_init(&mutex, NULL) != 0) {
        perror("Samolot: blad inicjalizacji mutex");
        sem_destroy(&bagaz_wagaSem);
        sem_destroy(&stairsSemNormal);
        sem_destroy(&stairsSemVip);
        detach_shared_memory();
        exit(EXIT_FAILURE);
    }
    if (pthread_cond_init(&samolotCond, NULL) != 0) {
        perror("Samolot: blad inicjalizacji warunku");
        pthread_mutex_destroy(&mutex);
        sem_destroy(&bagaz_wagaSem);
        sem_destroy(&stairsSemNormal);
        sem_destroy(&stairsSemVip);
        detach_shared_memory();
        exit(EXIT_FAILURE);
    }

    // Inicjalizacja pojemności
    capacityNormal = K;
    capacityVip    = K;
    licznik_pasazer = 0;

    // Inicjalizacja kolejki komunikatów
    key_t msg_key = ftok(MSG_QUEUE_PATH, MSG_QUEUE_PROJ);
    if (msg_key == -1) {
        perror("Samolot: blad ftok");
        pthread_cond_destroy(&samolotCond);
        pthread_mutex_destroy(&mutex);
        sem_destroy(&bagaz_wagaSem);
        sem_destroy(&stairsSemNormal);
        sem_destroy(&stairsSemVip);
        detach_shared_memory();
        exit(EXIT_FAILURE);
    }
    msg_queue_id = msgget(msg_key, 0666);
    if (msg_queue_id == -1) {
        perror("Samolot: blad msgget");
        pthread_cond_destroy(&samolotCond);
        pthread_mutex_destroy(&mutex);
        sem_destroy(&bagaz_wagaSem);
        sem_destroy(&stairsSemNormal);
        sem_destroy(&stairsSemVip);
        detach_shared_memory();
        exit(EXIT_FAILURE);
    }

    // Prośba do dyspozytora o gate
    notify_dyspozytor(MSG_GATE_REQUEST);

    wiadomosc_buf reply_msg;
    printf("Samolot %d: Oczekiwanie na gate...\n", getpid());
    if(msgrcv(msg_queue_id, &reply_msg,
              sizeof(wiadomosc_buf) - sizeof(long),
              getpid(), 0) == -1) {
        perror("Samolot: blad msgrcv");
        sem_destroy(&bagaz_wagaSem);
        sem_destroy(&stairsSemNormal);
        sem_destroy(&stairsSemVip);
        pthread_cond_destroy(&samolotCond);
        pthread_mutex_destroy(&mutex);
        detach_shared_memory();
        return;
    }
    if(reply_msg.rodzaj != MSG_GATE_ASSIGN || reply_msg.gate_id == -1) {
        notify_dyspozytor(MSG_SAMOLOT_POWROT);
        sem_destroy(&bagaz_wagaSem);
        sem_destroy(&stairsSemNormal);
        sem_destroy(&stairsSemVip);
        pthread_cond_destroy(&samolotCond);
        pthread_mutex_destroy(&mutex);
        detach_shared_memory();
        return;
    }

    printf("Samolot %d: Przydzielono gate %d\n", getpid(), reply_msg.gate_id);

    // -----------------------------
    // Uruchamiamy wątki pasażerów
    // -----------------------------
    pthread_t threads[N];

    // Tworzenie wątków pasażerów
    for (long i = 0; i < N; i++) {
        if (pthread_create(&threads[i], NULL, pasazer_func, (void*) i) != 0) {
            perror("Samolot: blad tworzenia wątku pasażera");
            // Kontynuujemy tworzenie pozostałych pasażerów
        }
    }

    // Rozpocznij timer dla odlotu po T1 sekund
    alarm(T1); // Używanie alarmu dla prostoty

    // Czekamy, aż wszyscy pasażerowie przejdą procedury (lub wyczerpią się miejsca).
    pthread_mutex_lock(&mutex);
    while (licznik_pasazer < N && (capacityNormal > 0 || capacityVip > 0) && keep_running) {
        pthread_cond_wait(&samolotCond, &mutex);
    }
    pthread_mutex_unlock(&mutex);

    // Jeśli early departure zostało wyzwolone, pomiń oczekiwanie
    if (early_depart) {
        printf("Samolot %d: Wczesny odlot.\n", getpid());
    } else {
        // Normalny odlot
        printf("Samolot %d: Czas T1 upłynął, odlot.\n", getpid());
    }

    // Ustaw keep_running na 0, by zasygnalizować pasażerom zatrzymanie boardingu
    keep_running = 0;

    // Dołączamy wątki
    for (int i = 0; i < N; i++) {
        pthread_join(threads[i], NULL);
    }

    // Zbieramy dane do pamięci dzielonej
    pthread_mutex_lock(&shm_ptr->shm_mutex);
    int boardedNormal = K - capacityNormal;
    int boardedVip    = K - capacityVip;
    int boarded       = boardedNormal + boardedVip;
    shm_ptr->total_boarded += boarded;
    shm_ptr->total_rejected += (N - boarded);
    pthread_mutex_unlock(&shm_ptr->shm_mutex);

    printf("Samolot %d: Pasażerowie zakończyli procedury. Wsiadło razem=%d [normal=%d, vip=%d].\n",
           getpid(), boarded, boardedNormal, boardedVip);

    // Czyszczenie semaforów i mutexów
    sem_destroy(&bagaz_wagaSem);
    sem_destroy(&stairsSemNormal);
    sem_destroy(&stairsSemVip);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&samolotCond);

    // Symulacja przygotowania do odlotu
    sleep(2);
    if (!keep_running) {
        // Kończymy, wysyłamy powrót
        notify_dyspozytor(MSG_SAMOLOT_POWROT);
        detach_shared_memory();
        return;
    }

    printf("Samolot %d: Odlot\n", getpid());
    sleep(1);
    printf("Samolot %d: Powrot\n", getpid());
    notify_dyspozytor(MSG_SAMOLOT_POWROT);

    // Odpinamy pamięć dzieloną
    detach_shared_memory();
}

int main() {
    while (1) { // Pętla ciągła
        simulate_flight_cycle();
        // Opcjonalnie, dodaj krótki sen, aby uniknąć zbyt szybkiego pętlenia
        sleep(1);
    }
    return 0;
}
