#ifndef SAMOLOT_H
#define SAMOLOT_H

void* samolot_func(void* arg);

extern pthread_mutex_t mutex; 
extern int capacity; // pojemność samolotu obecnie  
extern int P;  // pojemność samolotu ogólnie

#endif