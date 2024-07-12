// linux socket explanation: https://www.youtube.com/watch?v=XXfdzwEsxFk

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../hash_table/hash_table.h"

#define PORT_ENGLISH_TO_ITALIAN 8080
#define PORT_ITALIAN_TO_ENGLISH 6969
#define BUFSIZE 1024
#define MAX_LENGTH 1000
#define MAX_USERS_PER_ROOM 2
#define MAX_CLIENTS 50

pthread_mutex_t old_client_mutex = PTHREAD_MUTEX_INITIALIZER;

int waiting_client_socket_list[MAX_CLIENTS] = {0};
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

atomic_int active_english_to_italian_clients = 0;
atomic_int active_italian_to_english_clients = 0;
pthread_mutex_t client_english_to_italian_count_mutex =
    PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t client_italian_to_english_count_mutex =
    PTHREAD_MUTEX_INITIALIZER;

typedef struct {
  ht_hash_table *english_to_italian;
  ht_hash_table *italian_to_english;
} vocab;

int server_fd_english_to_italian, server_fs_italian_to_english;

void add_client(int socket) {
  pthread_mutex_lock(&clients_mutex);
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (waiting_client_socket_list[i] == 0) {
      waiting_client_socket_list[i] = socket;
      break;
    }
  }
  pthread_mutex_unlock(&clients_mutex);
}

void remove_client(int socket) {
  pthread_mutex_lock(&clients_mutex);
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (waiting_client_socket_list[i] == socket) {
      printf("SONO ENTRATO BROOOO!\n");
      waiting_client_socket_list[i] = 0;

      break;
    }
  }
  pthread_mutex_unlock(&clients_mutex);
}

void server_shutdown_handler(int sig) {
  printf("\nShutting down server...\n");

  pthread_mutex_lock(&clients_mutex);
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (waiting_client_socket_list[i] != 0) {
      send(waiting_client_socket_list[i], "SERVER_SHUTDOWN",
           strlen("SERVER_SHUTDOWN"), 0);
      close(waiting_client_socket_list[i]);
      waiting_client_socket_list[i] = 0;
    }
  }
  pthread_mutex_unlock(&clients_mutex);

  close(server_fd_english_to_italian);
  close(server_fs_italian_to_english);

  exit(0);
}

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
  int client_port;
  vocab *vocabulary;
} clientinfo;

// char *get_username(char *message) {
//   char *username_end = strchr(message, ':');
//   if (username_end != NULL) {
//     size_t username_len = username_end - message;
//     char *username = malloc(username_len + 1); // +1 for null terminator
//     strncpy(username, message, username_len);
//     username[username_len] = '\0';
//     return username; // Remember to free this after use
//   } else {
//     return NULL; // No username found
//   }
// }

void broadcast_message(const char *message, int sender_socket) {
  pthread_mutex_lock(&clients_mutex);
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (waiting_client_socket_list[i] != 0) {
      printf("WAITING SOCKET BROADCAST SEND: %d\n",
             waiting_client_socket_list[i]);

      printf("MESSAGE IN BROADCAST: %s\n", message);

      send(waiting_client_socket_list[i], message, strlen(message), 0);
    }
  }
  printf("MANNAGGIA DIOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOo\n");
  pthread_mutex_unlock(&clients_mutex);
}

atomic_int old_client = 0;
void *handle_client_english_to_italian(void *arg) {
  printf("ENTRATO IN THREAD CLIENT\n");
  char buffer[BUFSIZE];
  clientinfo *client_info = (clientinfo *)arg;

  printf("Currently active users %d\n", active_english_to_italian_clients);

  if (active_english_to_italian_clients >= MAX_USERS_PER_ROOM) {
    atomic_store(&old_client, client_info->client_socket);

    add_client(client_info->client_socket);

    printf("ROOM IS LOCKED! Retry again after some time...\n");

    // printf("IN ROOM IS LOCKED ADD_CLIENT\n");
    // pthread_mutex_lock(&clients_mutex);
    // printf("Waiting client socket list: \n");
    // for (int i = 0; i < 8; i++) {
    //   printf("CLIENT SOCKET %d!\n", waiting_client_socket_list[i]);
    // }
    // pthread_mutex_unlock(&clients_mutex);

    memset(buffer, 0, BUFSIZE);
    strcpy(buffer, "LOCKED");

    printf("BUFFER LOCKED: %s\n", buffer);

    // Send the response to the client
    send(client_info->client_socket, buffer, strlen(buffer), 0);

    printf("HELLO\n");

    return NULL;
  }

  int client_to_remove = atomic_exchange(&old_client, 0);
  if (client_to_remove != 0) {
    remove_client(client_to_remove);
  }

  pthread_mutex_lock(&client_english_to_italian_count_mutex);
  active_english_to_italian_clients++;
  pthread_mutex_unlock(&client_english_to_italian_count_mutex);

  while (1) {
    // Receive data from client
    int bytes_received = recv(client_info->client_socket, buffer, BUFSIZE, 0);
    if (bytes_received <= 0) {
      if (bytes_received == 0) {
        printf("Client: %d disconnected\n", client_info->client_port);
      } else {
        perror("recv failed");
      }
      break;
    }

    buffer[bytes_received] = '\0';
    // printf("Received from client %s: %s\n", client_ip, buffer);

    // Remove the user: and read only the message
    char *message_without_user = strchr(buffer, ':');
    if (message_without_user != NULL) {
      message_without_user += 2;
    }

    // Translate the phrase
    char *translated_phrase = translate_phrase(
        client_info->vocabulary->english_to_italian, message_without_user);

    // Reattach the username
    char *final_message = reattach_username(buffer, translated_phrase);

    memset(buffer, 0, BUFSIZE);
    strcpy(buffer, final_message);

    printf("BUFFER FINAL MESSAGE: %s\n", buffer);

    printf("%s\n", final_message);

    // Send the response to the client
    send(client_info->client_socket, buffer, strlen(buffer), 0);
    free(translated_phrase);
    free(final_message);

    if (strcmp(message_without_user, "/ciao") == 0 ||
        strcmp(message_without_user, "/exit") == 0) {
      printf("Client: %d requested to close the connection.\n",
             client_info->client_port);

      printf("IN BROADCAST ADD_CLIENT\n");
      pthread_mutex_lock(&clients_mutex);
      printf("Waiting client socket list: \n");
      for (int i = 0; i < 8; i++) {
        printf("CLIENT SOCKET %d!\n", waiting_client_socket_list[i]);
      }
      pthread_mutex_unlock(&clients_mutex);

      memset(buffer, 0, BUFSIZE);
      strcpy(buffer, "NOT LOCKED");

      broadcast_message(buffer, client_info->client_socket);

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
  char buffer[BUFSIZE];
  clientinfo *client_info = (clientinfo *)arg;

  if (active_italian_to_english_clients == MAX_USERS_PER_ROOM) {
    printf("ROOM IS LOCKED! Retry again after some time...\n");

    // Send the response to the client
    send(client_info->client_socket, "LOCKED", strlen("LOCKED"), 0);
    return NULL;
  }

  add_client(client_info->client_socket);

  pthread_mutex_lock(&client_italian_to_english_count_mutex);
  active_italian_to_english_clients++;
  pthread_mutex_unlock(&client_italian_to_english_count_mutex);

  while (1) {
    // Receive data from client
    int bytes_received = recv(client_info->client_socket, buffer, BUFSIZE, 0);
    if (bytes_received <= 0) {
      if (bytes_received == 0) {
        printf("Client: %d disconnected\n", client_info->client_port);
      } else {
        perror("recv failed");
      }
      break;
    }

    buffer[bytes_received] = '\0';
    // printf("Received from client %s: %s\n", client_ip, buffer);

    // Remove the user: and read only the message
    char *message_without_user = strchr(buffer, ':');
    if (message_without_user != NULL) {
      message_without_user += 2;
    }

    // Translate the phrase
    char *translated_phrase = translate_phrase(
        client_info->vocabulary->italian_to_english, message_without_user);

    // Reattach the username
    char *final_message = reattach_username(buffer, translated_phrase);

    printf("%s\n", final_message);

    // Send the response to the client
    send(client_info->client_socket, final_message, strlen(final_message), 0);

    free(translated_phrase);
    free(final_message);

    if (strcmp(message_without_user, "/ciao") == 0 ||
        strcmp(message_without_user, "/exit") == 0) {
      printf("Client: %d requested to close the connection.\n",
             client_info->client_port);

      // broadcast_message("NOT LOCKED", client_info->client_socket);

      break;
    }
  }

  remove_client(client_info->client_socket);

  close(client_info->client_socket);

  pthread_mutex_lock(&client_italian_to_english_count_mutex);
  active_italian_to_english_clients--;
  pthread_mutex_unlock(&client_italian_to_english_count_mutex);

  return NULL;
}
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++

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
  char buffer[BUFSIZE];

  char *translated_str = NULL;

  while (1) {
    // Accept a new connection
    if ((client_socket = accept(server_fd_english_to_italian,
                                (struct sockaddr *)&client_addr, &addr_len)) <
        0) {
      if (errno == EINTR || errno == ECONNABORTED) {
        continue; // Handle interrupted calls or aborted connections
      } else {
        perror("accept failed");             // Log the error for debugging
        close(server_fd_english_to_italian); // Clean up and exit
        exit(EXIT_FAILURE);
      }
      continue;
    }

    // Print connected client's information
    char client_ip[INET_ADDRSTRLEN];
    int client_port = client_addr.sin_port;

    // Sending the client port back to the client
    char port_str[20];
    snprintf(port_str, sizeof(port_str), "PORT:%d", client_port);

    memset(buffer, 0, BUFSIZE);
    strcpy(buffer, port_str);

    printf("BUFFER PORT: %s\n", buffer);

    send(client_socket, buffer, strlen(buffer), 0);

    inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
    printf("Client connected: IP = %s, Port = %d\n", client_ip, client_port);

    clientinfo *client_info = malloc(sizeof(clientinfo));
    client_info->client_socket = client_socket;
    client_info->vocabulary = v;
    client_info->client_port = client_port;

    pthread_t client_thread;
    if (pthread_create(&client_thread, NULL, handle_client_english_to_italian,
                       (void *)client_info) != 0) {
      perror("Failed to create client thread");
      close(client_socket);
      continue;
    }

    printf("DOPO CREAZIONE THREAD CLIENT\n");

    pthread_detach(client_thread);

    printf("DOPO DETACH THREAD CLIENT\n");
  }
  return NULL;
}

// Italian -> English
void *room_italian_to_english(void *arg) {
  vocab *v = (vocab *)arg;

  ht_hash_table *ht_italian_to_english = v->italian_to_english;

  int client_socket;
  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);
  char buffer[BUFSIZE];

  char *translated_str = NULL;

  while (1) {
    // Accept a new connection
    if ((client_socket = accept(server_fs_italian_to_english,
                                (struct sockaddr *)&client_addr, &addr_len)) <
        0) {
      perror("accept failed");
      continue;
    }

    // Print connected client's information
    char client_ip[INET_ADDRSTRLEN];
    int client_port = client_addr.sin_port;

    inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
    printf("Client connected: IP = %s, Port = %d\n", client_ip, client_port);

    clientinfo *client_info = malloc(sizeof(clientinfo));
    client_info->client_socket = client_socket;
    client_info->vocabulary = v;
    client_info->client_port = client_port;

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

  signal(SIGINT, server_shutdown_handler);

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

  printf("TCP Server is listening on ports %d (English to Italian) and %d "
         "(Italian to English)...\n",
         PORT_ENGLISH_TO_ITALIAN, PORT_ITALIAN_TO_ENGLISH);

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
