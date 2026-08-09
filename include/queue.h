#ifndef QUEUE_H
#define QUEUE_H

#include "process.h"

// Node of the linked list used internally by the queue
typedef struct QueueNode {
    Process process;
    struct QueueNode *next;
} QueueNode;

// FIFO queue of ready processes
typedef struct {
    QueueNode *front;   // points to the first process in line
    QueueNode *rear;    // points to the last process in line
    int size;           // number of processes currently in the queue
} Queue;

// Initializes an empty queue
void queue_init(Queue *q);

// Adds a process to the end of the queue
void queue_enqueue(Queue *q, Process p);

// Removes and returns the process at the front of the queue
Process queue_dequeue(Queue *q);

// Returns 1 if the queue is empty, 0 otherwise
int queue_is_empty(Queue *q);

// Frees any remaining nodes (e.g. at program shutdown)
void queue_destroy(Queue *q);

#endif
