#ifndef DYSPOZYTOR_H
#define DYSPOZYTOR_H

void initialize_message_queue();  //inicjacja kolejki wiadomości
void cleanup_message_queue(); //czyszczenie kolejki wiadomości
void handle_messages(); // odbieranie wiadomości
void create_airplanes(int num_samoloty);  //tworzenie samolotów

#endif 