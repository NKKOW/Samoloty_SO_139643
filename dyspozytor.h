#ifndef DYSPOZYTOR_H
#define DYSPOZYTOR_H

void remove_existing_message_queue();
void initialize_message_queue();
void cleanup_message_queue();
void handle_messages();
void create_airplanes(int num_samoloty);

// Funkcje przykładowo do gniazd (socket) – tu jedynie pokazanie użycia
int setup_dispatcher_socket();
void close_dispatcher_socket(int sockfd);

// Deklaracja funkcji wątku do wysyłania sygnałów
void* signal_sender_thread(void* arg);

#endif