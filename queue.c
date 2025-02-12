#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

// Inicjalizacja kolejki
void queue_init(PlaneQueue* q) {
    q->front = NULL;
    q->rear = NULL;
}

// Sprawdzenie, czy kolejka jest pusta
bool queue_is_empty(PlaneQueue* q) {
    return (q->front == NULL);
}

// Wstawienie elementu do kolejki
void queue_enqueue(PlaneQueue* q, pid_t plane_pid) {
    PlaneNode* newNode = (PlaneNode*)malloc(sizeof(PlaneNode));
    if (!newNode) {
        perror("Kolejka: alokacja");
        exit(EXIT_FAILURE);
    }
    newNode->plane_pid = plane_pid;
    newNode->next = NULL;
    if (queue_is_empty(q)) {
        q->front = newNode;
        q->rear = newNode;
    } else {
        q->rear->next = newNode;
        q->rear = newNode;
    }
}

// Usunięcie elementu z kolejki
pid_t queue_dequeue(PlaneQueue* q) {
    if (queue_is_empty(q)) {
        return -1;
    }
    PlaneNode* temp = q->front;
    pid_t pid = temp->plane_pid;
    q->front = q->front->next;
    if (q->front == NULL) {
        q->rear = NULL;
    }
    free(temp);
    return pid;
}
