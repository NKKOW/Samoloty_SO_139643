#ifndef DYSPOZYTOR_H
#define DYSPOZYTOR_H

#include <sys/types.h>

// Tworzenie pasażerów
pid_t create_passenger(int passenger_id, pid_t plane_pid, int planeMd);
void create_passengers(int count, pid_t plane_pid, int planeMd);

// Zarządzanie kolejką komunikatów
void remove_existing_message_queue();
void initialize_message_queue();
void cleanup_message_queue();
void handle_messages();
void create_airplanes(int num_samoloty);

// Zarządzanie gniazdem
int setup_dispatcher_socket();
void close_dispatcher_socket(int sockfd);

// Wątek do wysyłania sygnałów
void* signal_sender_thread(void* arg);

#endif
