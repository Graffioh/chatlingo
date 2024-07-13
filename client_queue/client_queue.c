#include "./client_queue.h"

#include <stdio.h>
#include <stdlib.h>

client_queue *create_client_q() {
  client_queue *queue = (client_queue *)malloc(sizeof(client_queue));
  queue->front = 0;
  queue->rear = -1;
  queue->size = 0;
  return queue;
}

int is_client_q_empty(client_queue *queue) { return queue->size == 0; }

int is_client_q_full(client_queue *queue) {
  return queue->size == MAX_QUEUE_SIZE;
}

void client_enqueue(client_queue *queue, int client_socket) {
  if (is_client_q_full(queue)) {
    printf("Queue is full, cannot add more clients.\n");
    return;
  }
  queue->rear = (queue->rear + 1) % MAX_QUEUE_SIZE;
  queue->clients[queue->rear] = client_socket;
  queue->size++;
}

int client_dequeue(client_queue *queue) {
  if (is_client_q_empty(queue)) {
    printf("Queue is empty, no clients to remove.\n");
    return -1;
  }
  int client = queue->clients[queue->front];
  queue->front = (queue->front + 1) % MAX_QUEUE_SIZE;
  queue->size--;
  return client;
}
