// linux socket explanation: https://www.youtube.com/watch?v=XXfdzwEsxFk

#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../hash_table/hash_table.h"

#define PORT 8080
#define BUFSIZE 1024
#define MAX_LENGTH 1000

int server_fd;

void server_shutdown_handler(int sig) {
  printf("\nShutting down server...\n");
  close(server_fd);
  exit(0);
}

// Converts a txt file into an hash table
ht_hash_table *ht_setup_from_txt(FILE *txt) {
  char line[MAX_LENGTH];
  char *first_word, *second_word;
  ht_hash_table *ht = ht_new();

  while (fgets(line, MAX_LENGTH, txt) != NULL) {
    line[strcspn(line, "\n")] = 0;

    first_word = strtok(line, ",");
    if (first_word != NULL) {
      second_word = strtok(NULL, ",");
      if (second_word != NULL) {
        ht_insert(ht, first_word, second_word);
      }
    }
  }

  fclose(txt);

  return ht;
}

int main() {
  int new_socket;
  struct sockaddr_in server_addr, client_addr;
  socklen_t addr_len = sizeof(client_addr);
  char buffer[BUFSIZE];

  // Create socket
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  // Set server address
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(PORT);

  signal(SIGINT, server_shutdown_handler);

  // Bind the socket to the address
  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  // Listen for incoming connections
  if (listen(server_fd, 3) < 0) {
    perror("listen failed");
    exit(EXIT_FAILURE);
  }

  printf("TCP Server is listening on port %d...\n", PORT);

  // Vocab setup from txt to hash table for O(1) search
  //
  FILE *file = fopen("./server/vocab.txt", "r");
  if (file == NULL) {
    perror("Error opening file: %s");
    return -1;
  }
  ht_hash_table *ht = ht_setup_from_txt(file);

  char *translated_str = {0};
  while (1) {
    // Accept a new connection
    if ((new_socket = accept(server_fd, (struct sockaddr *)&client_addr,
                             &addr_len)) < 0) {
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
      printf("Received from client: %s\n", buffer);

      if ((translated_str = ht_search(ht, buffer))) {
        printf("Translation: %s\n", translated_str);
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

  close(server_fd);
  return 0;
}
