// dyspozytor.h

#ifndef DYSPOZYTOR_H
#define DYSPOZYTOR_H

void remove_existing_message_queue();
void initialize_message_queue();
void cleanup_message_queue();
void handle_messages();
void create_airplanes(int num_samoloty);

#endif
