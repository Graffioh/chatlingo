// linux socket explanation: https://www.youtube.com/watch?v=XXfdzwEsxFk

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../hash_table/hash_table.h"

#define PORT_ENGLISH_TO_ITALIAN 8080
#define PORT_ITALIAN_TO_ENGLISH 6969
#define BUFSIZE 1024
#define MAX_LENGTH 1000

typedef struct {
  ht_hash_table *english_to_italian;
  ht_hash_table *italian_to_english;
} vocab;

int server_fd_english_to_italian, server_fs_italian_to_english;

void server_shutdown_handler(int sig) {
  printf("\nShutting down server...\n");
  close(server_fd_english_to_italian);
  close(server_fs_italian_to_english);
  exit(0);
}

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

void *room_english_to_italian(void *arg) {
  vocab *v = (vocab *)arg;

  ht_hash_table *ht_english_to_italian = v->english_to_italian;

  int new_socket;
  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);
  char buffer[BUFSIZE];

  char *translated_str = NULL;

  while (1) {
    // Accept a new connection
    if ((new_socket = accept(server_fd_english_to_italian,
                             (struct sockaddr *)&client_addr, &addr_len)) < 0) {
      perror("accept failed");
      continue;
    }

    // Print connected client's information
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
    printf("Client connected: IP = %s, Port = %d\n", client_ip,
           ntohs(client_addr.sin_port));

    while (1) {
      // Receive data from client
      int bytes_received = recv(new_socket, buffer, BUFSIZE, 0);
      if (bytes_received <= 0) {
        if (bytes_received == 0) {
          printf("Client: %s disconnected\n", client_ip);
        } else {
          perror("recv failed");
        }
        break;
      }

      buffer[bytes_received] = '\0';
      printf("Received from client %s: %s\n", client_ip, buffer);

      // Translate the string or give the old string if no translation is found
      if ((translated_str = ht_search(ht_english_to_italian, buffer))) {
        printf("Translation english -> italian: %s\n", translated_str);
      } else {
        translated_str = buffer;
      }

      // Send a response back to the client
      const char *response = translated_str;
      send(new_socket, response, strlen(response), 0);

      if (strcmp(buffer, "/ciao") == 0) {
        printf("Client: %s requested to close the connection.\n", client_ip);
        break;
      }
    }

    close(new_socket);
  }

  return NULL;
}

void *room_italian_to_english(void *arg) {
  vocab *v = (vocab *)arg;

  ht_hash_table *ht_italian_to_english = v->italian_to_english;

  int new_socket;
  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);
  char buffer[BUFSIZE];

  char *translated_str = NULL;

  while (1) {
    // Accept a new connection
    if ((new_socket = accept(server_fs_italian_to_english,
                             (struct sockaddr *)&client_addr, &addr_len)) < 0) {
      perror("accept failed");
      continue;
    }

    // Print connected client's information
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
    printf("Client connected: IP = %s, Port = %d\n", client_ip,
           ntohs(client_addr.sin_port));

    while (1) {
      // Receive data from client
      int bytes_received = recv(new_socket, buffer, BUFSIZE, 0);
      if (bytes_received <= 0) {
        if (bytes_received == 0) {
          printf("Client: %s disconnected\n", client_ip);
        } else {
          perror("recv failed");
        }
        break;
      }

      buffer[bytes_received] = '\0';
      printf("Received from client %s: %s\n", client_ip, buffer);

      // Translate the string or give the old string if no translation is found
      if ((translated_str = ht_search(ht_italian_to_english, buffer))) {
        printf("Translation italian -> english: %s\n", translated_str);
      } else {
        translated_str = buffer;
      }

      // Send a response back to the client
      const char *response = translated_str;
      send(new_socket, response, strlen(response), 0);

      if (strcmp(buffer, "/ciao") == 0) {
        printf("Client: %s requested to close the connection.\n", client_ip);
        break;
      }
    }

    close(new_socket);
  }

  return NULL;
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

  // creazione thread room english to italian
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

  ht_del_hash_table(vocabulary->english_to_italian);
  ht_del_hash_table(vocabulary->italian_to_english);
  free(vocabulary);

  close(server_fd_english_to_italian);
  close(server_fs_italian_to_english);
  return 0;
}
