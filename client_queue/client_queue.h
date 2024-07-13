#ifndef CLIENTQUEUE_H
#define CLIENTQUEUE_H

#define MAX_QUEUE_SIZE 50

typedef struct {
  int clients[MAX_QUEUE_SIZE];
  int front;
  int rear;
  int size;
} client_queue;

client_queue *create_client_q();

int is_client_q_empty(client_queue *queue);

int is_client_q_full(client_queue *queue);

void client_enqueue(client_queue *queue, int client_socket);

int client_dequeue(client_queue *queue);

#endif // CLIENTQUEUE_H
