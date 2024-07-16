// linux socket explanation: https://www.youtube.com/watch?v=XXfdzwEsxFk

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../client_queue/client_queue.h"
#include "../hash_table/hash_table.h"

#define PORT_ENGLISH_TO_ITALIAN 8080
#define PORT_ITALIAN_TO_ENGLISH 6969
#define BUFSIZE 1024
#define MAX_LENGTH 1000
#define MAX_USERS_PER_ROOM 2
#define MAX_CLIENTS 50

typedef struct {
  ht_hash_table *english_to_italian;
  ht_hash_table *italian_to_english;
} vocab;

int active_client_socket_list[MAX_CLIENTS] = {0};
pthread_mutex_t active_clients_mutex = PTHREAD_MUTEX_INITIALIZER;

atomic_int active_english_to_italian_clients = 0;
atomic_int active_italian_to_english_clients = 0;
pthread_mutex_t client_english_to_italian_count_mutex =
    PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t client_italian_to_english_count_mutex =
    PTHREAD_MUTEX_INITIALIZER;

int server_fd_english_to_italian, server_fs_italian_to_english;

client_queue *waiting_client_queue_english_to_italian;
client_queue *waiting_client_queue_italian_to_english;

pthread_mutex_t waiting_clients_english_to_italian_mutex =
    PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t waiting_clients_italian_to_english_mutex =
    PTHREAD_MUTEX_INITIALIZER;

// Translation handling
//
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// Create vocabulary hash tables for text file
vocab *vocab_setup_from_txt() {
  FILE *file = fopen("./server/vocab.txt", "r");
  if (file == NULL) {
    perror("Error opening file: %s");
    return NULL;
  }

  char line[MAX_LENGTH];
  char *first_word, *second_word;

  vocab *v = malloc(sizeof(vocab));
  v->english_to_italian = ht_new();
  v->italian_to_english = ht_new();

  while (fgets(line, MAX_LENGTH, file) != NULL) {
    line[strcspn(line, "\n")] = 0;

    first_word = strtok(line, ",");
    if (first_word != NULL) {
      second_word = strtok(NULL, ",");
      if (second_word != NULL) {
        ht_insert(v->english_to_italian, first_word, second_word);
        ht_insert(v->italian_to_english, second_word, first_word);
      }
    }
  }

  fclose(file);

  return v;
}

void first_letter_uppercase(char *str) { str[0] = toupper(str[0]); }

char *reattach_username(char *original_message, char *translated_message) {
  char *username_end = strchr(original_message, ':');
  if (username_end != NULL) {
    // Calculate length of username
    size_t username_len = username_end - original_message;

    // Allocate space for the final message, including the colon and space
    // between user and message
    char *final_message = malloc(username_len + strlen(translated_message) +
                                 3); // +3 for ": " and null terminator

    // Copy username
    memcpy(final_message, original_message, username_len);

    // Add colon and space
    final_message[username_len] = ':';
    final_message[username_len + 1] = ' ';

    // Append translated message
    strcpy(final_message + username_len + 2, translated_message);

    return final_message;
  } else {
    // No username found, return the translated message as-is
    return translated_message;
  }
}

char *translate_phrase(ht_hash_table *dictionary, char *phrase) {
  char *result = malloc(BUFSIZE);
  result[0] = '\0';
  char *word = strtok(phrase, " ");

  while (word != NULL) {
    first_letter_uppercase(word);

    char *translated_word = ht_search(dictionary, word);
    if (translated_word == NULL) {
      translated_word = word;
    }

    strcat(result, translated_word);
    strcat(result, " ");

    word = strtok(NULL, " ");
  }

  int len = strlen(result);
  result[len - 1] = '\0';

  return result;
}
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++

// Multiple clients handling
//
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
typedef struct {
  int client_socket;
  vocab *vocabulary;
} clientinfo;

void broadcast_message_english_to_italian(const char *message,
                                          int sender_socket) {
  pthread_mutex_lock(&waiting_clients_english_to_italian_mutex);
  if (!is_client_q_empty(waiting_client_queue_english_to_italian)) {
    int next_client =
        waiting_client_queue_english_to_italian
            ->clients[waiting_client_queue_english_to_italian->front];
    send(next_client, message, strlen(message), 0);
  }
  pthread_mutex_unlock(&waiting_clients_english_to_italian_mutex);
}

void broadcast_message_italian_to_english(const char *message,
                                          int sender_socket) {
  pthread_mutex_lock(&waiting_clients_italian_to_english_mutex);
  if (!is_client_q_empty(waiting_client_queue_italian_to_english)) {
    int next_client =
        waiting_client_queue_italian_to_english
            ->clients[waiting_client_queue_italian_to_english->front];
    send(next_client, message, strlen(message), 0);
  }
  pthread_mutex_unlock(&waiting_clients_italian_to_english_mutex);
}

void *handle_client_english_to_italian(void *arg) {
  char client_english_to_italian_buffer[BUFSIZE];
  clientinfo *client_info = (clientinfo *)arg;

  if (active_english_to_italian_clients >= MAX_USERS_PER_ROOM) {
    pthread_mutex_lock(&waiting_clients_english_to_italian_mutex);
    client_enqueue(waiting_client_queue_english_to_italian,
                   client_info->client_socket);
    pthread_mutex_unlock(&waiting_clients_english_to_italian_mutex);

    printf("\033[33m"
           "A user tried to enter the room, but it's full...\n"
           "\033[0m");

    memset(client_english_to_italian_buffer, 0, BUFSIZE);
    strcpy(client_english_to_italian_buffer, "LOCKED");

    // Send the response to the client
    send(client_info->client_socket, client_english_to_italian_buffer,
         strlen(client_english_to_italian_buffer), 0);

    return NULL;
  }

  int client_to_remove = -1;
  pthread_mutex_lock(&waiting_clients_english_to_italian_mutex);
  if (!is_client_q_empty(waiting_client_queue_english_to_italian)) {
    client_to_remove = client_dequeue(waiting_client_queue_english_to_italian);
  }
  pthread_mutex_unlock(&waiting_clients_english_to_italian_mutex);

  pthread_mutex_lock(&client_english_to_italian_count_mutex);
  active_english_to_italian_clients++;
  pthread_mutex_unlock(&client_english_to_italian_count_mutex);

  while (1) {
    memset(client_english_to_italian_buffer, 0, BUFSIZE);

    // Receive message from client
    int bytes_received_message =
        recv(client_info->client_socket, client_english_to_italian_buffer,
             BUFSIZE, 0);

    client_english_to_italian_buffer[bytes_received_message] = '\0';
    // printf("Received from client %s: %s\n", client_ip, buffer);

    if (strcmp(client_english_to_italian_buffer, "KICKED") == 0) {
      memset(client_english_to_italian_buffer, 0, BUFSIZE);
      strcpy(client_english_to_italian_buffer, "NOT LOCKED");

      broadcast_message_english_to_italian(client_english_to_italian_buffer,
                                           client_info->client_socket);

      break;
    }

    // Remove the user: and read only the message
    char *message_without_user = strchr(client_english_to_italian_buffer, ':');
    if (message_without_user != NULL) {
      message_without_user += 2;
    }

    // Translate the phrase
    char *translated_phrase = translate_phrase(
        client_info->vocabulary->english_to_italian, message_without_user);

    // Reattach the username
    char *final_message =
        reattach_username(client_english_to_italian_buffer, translated_phrase);

    printf("%s\n", final_message);

    memset(client_english_to_italian_buffer, 0, BUFSIZE);
    strcpy(client_english_to_italian_buffer, final_message);

    // Send the response to the client
    send(client_info->client_socket, client_english_to_italian_buffer,
         strlen(client_english_to_italian_buffer), 0);
    free(translated_phrase);
    free(final_message);

    if (strcmp(message_without_user, "/ciao") == 0 ||
        strcmp(message_without_user, "/exit") == 0) {
      memset(client_english_to_italian_buffer, 0, BUFSIZE);
      strcpy(client_english_to_italian_buffer, "NOT LOCKED");

      broadcast_message_english_to_italian(client_english_to_italian_buffer,
                                           client_info->client_socket);

      break;
    }
  }

  close(client_info->client_socket);

  pthread_mutex_lock(&client_english_to_italian_count_mutex);
  active_english_to_italian_clients--;
  pthread_mutex_unlock(&client_english_to_italian_count_mutex);

  return NULL;
}

void *handle_client_italian_to_english(void *arg) {
  char client_italian_to_english_buffer[BUFSIZE];
  clientinfo *client_info = (clientinfo *)arg;

  if (active_italian_to_english_clients == MAX_USERS_PER_ROOM) {
    printf("\033[33m"
           "A user tried to enter the room, but it's full...\n"
           "\033[0m");

    pthread_mutex_lock(&waiting_clients_italian_to_english_mutex);
    client_enqueue(waiting_client_queue_italian_to_english,
                   client_info->client_socket);
    pthread_mutex_unlock(&waiting_clients_italian_to_english_mutex);

    memset(client_italian_to_english_buffer, 0, BUFSIZE);
    strcpy(client_italian_to_english_buffer, "LOCKED");

    // Send the response to the client
    send(client_info->client_socket, client_italian_to_english_buffer,
         strlen(client_italian_to_english_buffer), 0);

    return NULL;
  }

  int client_to_remove = -1;
  pthread_mutex_lock(&waiting_clients_italian_to_english_mutex);
  if (!is_client_q_empty(waiting_client_queue_italian_to_english)) {
    client_to_remove = client_dequeue(waiting_client_queue_italian_to_english);
  }
  pthread_mutex_unlock(&waiting_clients_italian_to_english_mutex);

  pthread_mutex_lock(&client_italian_to_english_count_mutex);
  active_italian_to_english_clients++;
  pthread_mutex_unlock(&client_italian_to_english_count_mutex);

  while (1) {
    memset(client_italian_to_english_buffer, 0, BUFSIZE);

    // Receive data from client
    int bytes_received_message =
        recv(client_info->client_socket, client_italian_to_english_buffer,
             BUFSIZE, 0);

    client_italian_to_english_buffer[bytes_received_message] = '\0';
    // printf("Received from client %s: %s\n", client_ip, buffer);

    if (strcmp(client_italian_to_english_buffer, "KICKED") == 0) {
      memset(client_italian_to_english_buffer, 0, BUFSIZE);
      strcpy(client_italian_to_english_buffer, "NOT LOCKED");

      broadcast_message_english_to_italian(client_italian_to_english_buffer,
                                           client_info->client_socket);

      break;
    }

    // Remove the user: and read only the message
    char *message_without_user = strchr(client_italian_to_english_buffer, ':');
    if (message_without_user != NULL) {
      message_without_user += 2;
    }

    // Translate the phrase
    char *translated_phrase = translate_phrase(
        client_info->vocabulary->italian_to_english, message_without_user);

    // Reattach the username
    char *final_message =
        reattach_username(client_italian_to_english_buffer, translated_phrase);

    printf("%s\n", final_message);

    memset(client_italian_to_english_buffer, 0, BUFSIZE);
    strcpy(client_italian_to_english_buffer, final_message);

    // Send the response to the client
    send(client_info->client_socket, final_message, strlen(final_message), 0);
    free(translated_phrase);
    free(final_message);

    if (strcmp(message_without_user, "/ciao") == 0 ||
        strcmp(message_without_user, "/exit") == 0) {
      memset(client_italian_to_english_buffer, 0, BUFSIZE);
      strcpy(client_italian_to_english_buffer, "NOT LOCKED");

      broadcast_message_italian_to_english(client_italian_to_english_buffer,
                                           client_info->client_socket);

      break;
    }
  }

  close(client_info->client_socket);

  pthread_mutex_lock(&client_italian_to_english_count_mutex);
  active_italian_to_english_clients--;
  pthread_mutex_unlock(&client_italian_to_english_count_mutex);

  return NULL;
}
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++

void print_welcome_message(int client_socket) {
  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);
  getpeername(client_socket, (struct sockaddr *)&client_addr, &addr_len);
  char timestamp[20];
  time_t now = time(NULL);
  strftime(timestamp, 20, "%Y-%m-%d %H:%M:%S", localtime(&now));

  printf("[%s] New client connected (English to Italian): %s:%d\n", timestamp,
         inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
}

// Rooms
//
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// English -> Italian
void *room_english_to_italian(void *arg) {
  vocab *v = (vocab *)arg;

  int client_socket;
  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);
  char room_english_to_italian_buffer[BUFSIZE];

  char *translated_str = NULL;

  while (1) {
    // Accept a new connection
    if ((client_socket = accept(server_fd_english_to_italian,
                                (struct sockaddr *)&client_addr, &addr_len)) <
        0) {
      if (errno == EINTR || errno == ECONNABORTED) {
        continue;
      } else {
        perror("accept failed");
        close(server_fd_english_to_italian);
        exit(EXIT_FAILURE);
      }
      continue;
    }

    print_welcome_message(client_socket);

    clientinfo *client_info = malloc(sizeof(clientinfo));
    client_info->client_socket = client_socket;
    client_info->vocabulary = v;

    pthread_t client_thread;
    if (pthread_create(&client_thread, NULL, handle_client_english_to_italian,
                       (void *)client_info) != 0) {
      perror("Failed to create client thread");
      close(client_socket);
      continue;
    }

    pthread_detach(client_thread);
  }
  return NULL;
}

// Italian -> English
void *room_italian_to_english(void *arg) {
  vocab *v = (vocab *)arg;

  int client_socket;
  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);
  char room_italian_to_english_buffer[BUFSIZE];

  char *translated_str = NULL;

  while (1) {
    // Accept a new connection
    if ((client_socket = accept(server_fs_italian_to_english,
                                (struct sockaddr *)&client_addr, &addr_len)) <
        0) {
      if (errno == EINTR || errno == ECONNABORTED) {
        continue;
      } else {
        perror("accept failed");
        close(server_fd_english_to_italian);
        exit(EXIT_FAILURE);
      }
      continue;
    }

    print_welcome_message(client_socket);

    clientinfo *client_info = malloc(sizeof(clientinfo));
    client_info->client_socket = client_socket;
    client_info->vocabulary = v;

    pthread_t client_thread;
    if (pthread_create(&client_thread, NULL, handle_client_italian_to_english,
                       (void *)client_info) != 0) {
      perror("Failed to create client thread");
      close(client_socket);
      continue;
    }

    pthread_detach(client_thread);
  }

  return NULL;
}
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++

// Create thread for each room
void room_creation(vocab *vocabulary) {
  pthread_t th;
  pthread_t th2;

  if (pthread_create(&th, NULL, room_english_to_italian, (void *)vocabulary) !=
      0) {
    perror("Failed to create thread room english to italian");
    exit(EXIT_FAILURE);
  }

  if (pthread_create(&th2, NULL, room_italian_to_english, (void *)vocabulary) !=
      0) {
    perror("Failed to create thread room italian to english");
    exit(EXIT_FAILURE);
  }

  if (pthread_join(th, NULL) != 0) {
    perror("Failed to join thread room english to italian");
    exit(EXIT_FAILURE);
  }

  if (pthread_join(th2, NULL) != 0) {
    perror("Failed to join thread room italian to english");
    exit(EXIT_FAILURE);
  }
}

int main() {
  waiting_client_queue_english_to_italian = create_client_q();
  waiting_client_queue_italian_to_english = create_client_q();

  struct sockaddr_in server_addr_english, server_addr_italian;

  // Create socket
  if ((server_fd_english_to_italian = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket failed for English to Italian");
    exit(EXIT_FAILURE);
  }
  if ((server_fs_italian_to_english = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket failed for Italian to English");
    exit(EXIT_FAILURE);
  }

  // Things for server shutdown
  int opt = 1;
  if (setsockopt(server_fd_english_to_italian, SOL_SOCKET, SO_REUSEADDR, &opt,
                 sizeof(opt)) < 0) {
    perror("setsockopt failed for English to Italian");
    exit(EXIT_FAILURE);
  }
  if (setsockopt(server_fs_italian_to_english, SOL_SOCKET, SO_REUSEADDR, &opt,
                 sizeof(opt)) < 0) {
    perror("setsockopt failed for Italian to English");
    exit(EXIT_FAILURE);
  }

  // Set server address
  server_addr_english.sin_family = AF_INET;
  server_addr_english.sin_addr.s_addr = INADDR_ANY;
  server_addr_english.sin_port = htons(PORT_ENGLISH_TO_ITALIAN);

  server_addr_italian.sin_family = AF_INET;
  server_addr_italian.sin_addr.s_addr = INADDR_ANY;
  server_addr_italian.sin_port = htons(PORT_ITALIAN_TO_ENGLISH);

  // Bind the socket to the address
  if (bind(server_fd_english_to_italian,
           (struct sockaddr *)&server_addr_english,
           sizeof(server_addr_english)) < 0) {
    perror("bind failed for English to Italian");
    exit(EXIT_FAILURE);
  }
  if (bind(server_fs_italian_to_english,
           (struct sockaddr *)&server_addr_italian,
           sizeof(server_addr_italian)) < 0) {
    perror("bind failed for Italian to English");
    exit(EXIT_FAILURE);
  }

  // Listen for incoming connections
  if (listen(server_fd_english_to_italian, 3) < 0) {
    perror("listen failed for English to Italian");
    exit(EXIT_FAILURE);
  }
  if (listen(server_fs_italian_to_english, 3) < 0) {
    perror("listen failed for Italian to English");
    exit(EXIT_FAILURE);
  }

  printf("---------------------------------------------------------------------"
         "----------------------------------\n");
  printf("TCP Chat Server is listening on ports %d (English to Italian Room) "
         "and %d "
         "(Italian to English Room)\n",
         PORT_ENGLISH_TO_ITALIAN, PORT_ITALIAN_TO_ENGLISH);
  printf("---------------------------------------------------------------------"
         "----------------------------------\n");

  vocab *vocabulary = malloc(sizeof(vocab));
  vocabulary = vocab_setup_from_txt();

  room_creation(vocabulary);

  ht_del_hash_table(vocabulary->english_to_italian);
  ht_del_hash_table(vocabulary->italian_to_english);
  free(vocabulary);

  close(server_fd_english_to_italian);
  close(server_fs_italian_to_english);
  return 0;
}
