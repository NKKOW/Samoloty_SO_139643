#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "global.h"
#include "shared_memory.h"

int shm_id;
shared_data *shm_ptr;
int sem_id;


//Pamięć wspóldzielona
int shared_memory_init()
{
    key_t shm_key = ftok(SHM_PATH, SHM_PROJ);
    if(shm_key == -1)
    {
        perror("Blad ftok | shared_memory.c | init");
        return -1;
    }

    shm_id = shmget(shm_key, sizeof(shared_data), IPC_CREAT | 0666);
    if (shm_id == -1)
    {
        perror("Blad shmget | shared_memory.c | init");
        return -1;
    }

    shm_ptr = (shared_data*) shmat(shm_id, NULL, 0);
    if (shm_ptr == (void*) -1)
    {
        perror("Blad shmat | shared_memory.c | init");
        return -1;
    }

    shm_ptr -> akt_samoloty = 0;
    //Inne pola

    return 0;

}

shared_data* shared_memory_att() 
{
    key_t shm_key = ftok(SHM_PATH, SHM_PROJ);
    if(shm_key == -1)
    {
        perror("Blad ftok | shared_memory.c | attach");
        return NULL;
    }

    shm_id = shmget(shm_key, sizeof(shared_data), 0666);
    if (shm_id == -1)
    {
        perror("Blad shmget | shared_memory.c | attach");
        return NULL;
    }

    shm_ptr = (shared_data*) shmat(shm_id, NULL, 0);
    if (shm_ptr == (void*) -1)
    {
        perror("Blad shmat | shared_memory.c | attach");
        return NULL;
    }

    return shm_ptr;
}

void shared_memory_detach(shared_data *shm_ptr_local)
{
    if (shmdt(shm_ptr) == -1)
    {
        perror("Blad shmdt | shared_memory.c | detach");
    }
}

void shared_memory_cleanup()
{
    if (shmdt(shm_ptr) == -1)
    {
        perror("Blad shmdt | shared_memory.c | cleanup");
    }

    if (shmctl(shm_id, IPC_RMID, NULL))
    {
        perror("Blad shmctl | shared_memory.c | cleanup");
    }
}

//Semafory
int semaphore_init()
{
    key_t sem_key = ftok(SEM_PATH, SEM_PROJ);
    if(sem_key == -1)
    {
        perror("Blad ftok | shared_memory.c | semaphore_init");
        return -1;
    }

    sem_id = semget(sem_key, 1 , IPC_CREAT | 0666);
    if (sem_id == -1)
    {
        perror("Blad semget | shared_memory.c | init");
        return -1;
    }

    if (semctl(sem_id, 0, SETVAL, 1) == -1)
    {
        perror("Blad semctl SETVAL | shared_memory.c | init");
        return -1;
    }

    return 0;
}

int semaphore_att() 
{
    key_t sem_key = ftok(SEM_PATH, SEM_PROJ);
    if (sem_key == -1) 
    {
        perror("Blad ftok | shared_memory.c | semaphore_attach");
        return -1;
    }

    sem_id = semget(sem_key, 1, 0666);
    if (sem_id == -1) 
    {
        perror("Blad semget | shared_memory.c | semaphore_attach");
        return -1;
    }

    return 0;
}

int semaphore_lock() {
    struct sembuf sb = {0, -1, 0};
    return semop(sem_id, &sb, 1);
}


int semaphore_unlock() {
    struct sembuf sb = {0, 1, 0};
    return semop(sem_id, &sb, 1);
}


void semaphore_cleanup() {
    if (semctl(sem_id, 0, IPC_RMID) == -1) 
    {
        perror("Blad semctl IPC_RMID");
    }
}
