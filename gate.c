#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "gate.h"
#include "odprawa.h"

int gate_init(Gate *gate, int gate_id) 
{
    gate->gate_id = gate_id;
    gate->samolot_pid = -1;
    if(pthread_mutex_init(&gate->gate_mutex, NULL) != 0) 
    {
        perror("gate_init: pthread_mutex_init gate_mutex");
        return -1;
    }

    return 0;
}

void* gate_thread_func(void *arg) 
{
    Gate *gate = (Gate*) arg;
    char log_msg[256];
    while(1) 
    {
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

        pthread_mutex_lock(&gate->gate_mutex);
        if(gate->samolot_pid != -1) 
        {
            snprintf(log_msg, sizeof(log_msg), "Gate %d: Obsługuje samolot PID %d.", gate->gate_id, gate->samolot_pid);
            log_event("gate.log", log_msg);

            pthread_mutex_unlock(&gate->gate_mutex);

            //sleep(5); // Czas obsługi samolotu

            // Zwolnienie zasobów
            pthread_mutex_lock(&gate->gate_mutex);
            gate->samolot_pid = -1;
            pthread_mutex_unlock(&gate->gate_mutex);

            snprintf(log_msg, sizeof(log_msg), "Gate %d: Samolot PID %d zakończył obsługę.", gate->gate_id, gate->samolot_pid);
            log_event("gate.log", log_msg);
        } 
        else 
        {
            pthread_mutex_unlock(&gate->gate_mutex);
            //sleep(1); // Gate jest wolny
        }
    }
    
    return NULL;
}