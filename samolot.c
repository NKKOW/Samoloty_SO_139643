#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/shm.h>        // segmenty pamięci dzielonej
#include <sys/ipc.h>
#include <sys/stat.h>       // do open, etc.
#include <fcntl.h>          // do open
#include <string.h>

#include "global.h"
#include "samolot.h"
#include "pasazer.h"

int msg_queue_id;
volatile sig_atomic_t keep_running = 1;

// Zmienne do pamięci dzielonej
static int shm_id = -1;
static SharedData* shm_ptr = NULL;

void sigint_handler(int sig) {
    printf("\nSamolot %d: Otrzymano sygnał %d (SIGINT), kończenie...\n", getpid(), sig);
    keep_running = 0;
}

// Tworzenie / przyłączanie pamięci dzielonej
static void init_shared_memory() {
    // Używamy np. klucza z pliku:
    key_t key = ftok("msgqueue.key", 'S');
    if (key == -1) {
        perror("Samolot ftok for shm");
        exit(EXIT_FAILURE);
    }

    // tworzymy / uzyskujemy segment
    shm_id = shmget(key, sizeof(SharedData), 0666 | IPC_CREAT);
    if (shm_id < 0) {
        perror("Samolot shmget");
        exit(EXIT_FAILURE);
    }

    // przyłączamy
    shm_ptr = (SharedData*) shmat(shm_id, NULL, 0);
    if (shm_ptr == (void*) -1) {
        perror("Samolot shmat");
        exit(EXIT_FAILURE);
    }
}

// Odłączanie i ew. usuwanie pamięci (na koniec)
static void detach_shared_memory() {
    if (shm_ptr) {
        shmdt(shm_ptr);
        shm_ptr = NULL;
    }
    // Nie usuwamy segmentu, bo może jeszcze być potrzebny
    // do innych samolotów w projekcie
}

void notify_dyspozytor(rodzaj_wiadomosc msg_type) {
    wiadomosc_buf msg;
    msg.mtype = 1;
    msg.rodzaj = msg_type;
    msg.samolot_pid = getpid();
    msg.gate_id = -1;
    if (msgsnd(msg_queue_id, &msg, sizeof(wiadomosc_buf) - sizeof(long), 0) == -1) {
        perror("Blad msgsnd samolot");
        exit(EXIT_FAILURE);
    }
}

void simulate_flight_cycle() {
    printf("Samolot %d: Start\n", getpid());
    key_t msg_key = ftok(MSG_QUEUE_PATH, MSG_QUEUE_PROJ);
    if (msg_key == -1) {
        perror("Blad ftok samolot");
        exit(EXIT_FAILURE);
    }
    msg_queue_id = msgget(msg_key, 0666);
    if (msg_queue_id == -1) {
        perror("Blad msgget samolot");
        exit(EXIT_FAILURE);
    }
    signal(SIGINT, sigint_handler);

    // Inicjujemy pamięć dzieloną
    init_shared_memory();

    // Najpierw prosimy dyspozytora o gate:
    notify_dyspozytor(MSG_GATE_REQUEST);

    wiadomosc_buf reply_msg;
    printf("Samolot %d: Oczekiwanie na gate...\n", getpid());
    if(msgrcv(msg_queue_id, &reply_msg,
              sizeof(wiadomosc_buf) - sizeof(long),
              getpid(), 0) == -1) {
        perror("Blad msgrcv samolot");
        detach_shared_memory();
        exit(EXIT_FAILURE);
    }
    if(reply_msg.rodzaj != MSG_GATE_ASSIGN) {
        notify_dyspozytor(MSG_SAMOLOT_POWROT);
        detach_shared_memory();
        return;
    }
    if(reply_msg.gate_id == -1) {
        notify_dyspozytor(MSG_SAMOLOT_POWROT);
        detach_shared_memory();
        return;
    }

    printf("Samolot %d: Przydzielono gate %d\n", getpid(), reply_msg.gate_id);

    // -----------------------------
    // TU uruchamiamy wątki pasażerów:
    // -----------------------------
    pthread_t threads[N];
    sem_init(&bagaz_wagaSem, 0, 1); // tylko 1 waga bagażowa
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&samolotCond, NULL);

    // Zerujemy globalne zmienne
    capacity = K;          // K miejsc na 'schodach'
    licznik_pasazer = 0;   // ilu pasażerów wsiadło lub odpadło

    // Inicjujemy stanowiska kontroli i ich mutex:
    pthread_mutex_init(&stanowsikoMutex, NULL);
    pthread_cond_init(&stanowiskoCond, NULL);
    for(int i=0; i<3; i++){
        stanowiska[i].occupied = 0;
        stanowiska[i].plec = '\0';
    }

    // Tworzymy wątki pasażerów
    for (long i = 0; i < N; i++) {
        pthread_create(&threads[i], NULL, pasazer_func, (void*) i);
    }

    // Czekamy aż wszyscy pasażerowie przejdą proces (albo się skończy capacity)
    pthread_mutex_lock(&mutex);
    while (licznik_pasazer < N && capacity > 0) {
        pthread_cond_wait(&samolotCond, &mutex);
    }
    pthread_mutex_unlock(&mutex);

    // Dołączenie wątków
    for (int i = 0; i < N; i++) {
        pthread_join(threads[i], NULL);
    }

    // Zbieramy dane do pamięci dzielonej
    pthread_mutex_lock(&mutex);
    int boarded = K - capacity; // tylu realnie weszło
    shm_ptr->total_boarded += boarded;
    shm_ptr->total_rejected += (N - boarded);
    pthread_mutex_unlock(&mutex);

    printf("Samolot %d: Wszyscy pasażerowie zakończyli procedury, wsiadło = %d.\n",
           getpid(), boarded);

    // Czyszczenie semaforów/mutexów
    sem_destroy(&bagaz_wagaSem);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&samolotCond);
    pthread_mutex_destroy(&stanowsikoMutex);
    pthread_cond_destroy(&stanowiskoCond);

    // Symulacja przygotowania do odlotu
    sleep(2);
    if (!keep_running) {
        // kończymy, wysyłamy powrót
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
    exit(EXIT_SUCCESS);
}

int main() {
    simulate_flight_cycle();
    return 0;
}
