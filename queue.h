#ifndef QUEUE_H
#define QUEUE_H

#include <sys/types.h>
#include <stdbool.h>


//Struktury kolejki do bramek
typedef struct PlaneNode {
    pid_t plane_pid;
    struct PlaneNode* next;
} PlaneNode;

typedef struct {
    PlaneNode* front;
    PlaneNode* rear;
} PlaneQueue;

//Funckje obsługujące bramki
void queue_init(PlaneQueue* q);
bool queue_is_empty(PlaneQueue* q);
void queue_enqueue(PlaneQueue* q, pid_t plane_pid);
pid_t queue_dequeue(PlaneQueue* q);

#endif
