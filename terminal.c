#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "terminal.h"
#include "odprawa.h"

int terminal_init(Terminal *terminal, int terminal_id, int num_gates)
{
    terminal -> terminal_id = terminal_id;
    terminal -> current_samoloty = 0;
    terminal -> num_gates = num_gates;

    if(pthread_mutex_init(&terminal -> terminal_mutex, NULL) != 0)
    {
        perror("terminal_init: pthread_mutex_init terminal_mutex");
        return -1;
    }

    if(pthread_cond_init(&terminal->terminal_cond, NULL) != 0) 
    {
        perror("terminal_init: pthread_cond_init terminal_cond");
        return -1;
    }

    return 0;

}

void* terminal_thread_func(void *arg)
{
    Terminal *terminal = (Terminal*) arg;
    char log_msg[256];
    while(1)
    {
        //Możliwość anulowania wątku
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

        pthread_mutex_lock(&terminal -> terminal_mutex);
        while(terminal -> current_samoloty == 0)
        {
            pthread_cond_wait(&terminal -> terminal_cond, &terminal -> terminal_mutex);
        }
        pthread_mutex_unlock(&terminal -> terminal_mutex);

        //Logowanie aktwności terminala
        snprintf(log_msg, sizeof(log_msg), "Terminal %d: Obsługuje %d samolotów.", terminal -> terminal_id, terminal -> current_samoloty);
        log_event("terminal.log", log_msg);

        //sleep(2);

    }

    return NULL;
}