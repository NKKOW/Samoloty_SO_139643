#ifndef DYSPOZYTOR_H
#define DYSPOZYTOR_H

pid_t create_passenger(int passenger_id, pid_t plane_pid);
void create_passengers(int count, pid_t plane_pid);

void remove_existing_message_queue();
void initialize_message_queue();
void cleanup_message_queue();
void handle_messages();
void create_airplanes(int num_samoloty);

int setup_dispatcher_socket();
void close_dispatcher_socket(int sockfd);

void* signal_sender_thread(void* arg);

#endif
