#include <stdlib.h>
#include "queue.h"

// Sets the queue to an empty, ready-to-use state
void queue_init(Queue *q) {
    q->front = NULL;
    q->rear = NULL;
    q->size = 0;
}

// Inserts a new process at the rear of the queue (classic FIFO insertion)
void queue_enqueue(Queue *q, Process p) {
    QueueNode *new_node = malloc(sizeof(QueueNode));
    new_node->process = p;
    new_node->next = NULL;

    if (q->rear == NULL) {
        // Queue was empty, so the new node is both front and rear
        q->front = new_node;
    } else {
        q->rear->next = new_node;
    }
    q->rear = new_node;
    q->size++;
}

// Removes the process at the front of the queue and returns it
// Caller is responsible for checking queue_is_empty() first
Process queue_dequeue(Queue *q) {
    QueueNode *node = q->front;
    Process p = node->process;

    q->front = node->next;
    if (q->front == NULL) {
        // Queue became empty, so rear must be reset too
        q->rear = NULL;
    }

    free(node);
    q->size--;
    return p;
}

// Simple empty check based on the size counter
int queue_is_empty(Queue *q) {
    return q->size == 0;
}

// Drains the queue, freeing every remaining node
void queue_destroy(Queue *q) {
    while (!queue_is_empty(q)) {
        queue_dequeue(q);
    }
}