// linux socket explanation: https://www.youtube.com/watch?v=XXfdzwEsxFk

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SERVER_IP "127.0.0.1"
#define PORT_ENGLISH_TO_ITALIAN 8080
#define PORT_ITALIAN_TO_ENGLISH 6969
#define BUFSIZE 1024

int main() {
  int sockfd;
  struct sockaddr_in server_addr;
  char server_response_buffer[BUFSIZE];
  char message_buffer[BUFSIZE];
  int room_choice;

  // Ask user to choose a room
  do {
    printf("Choose a room:\n");
    printf("1. English to Italian\n");
    printf("2. Italian to English\n");
    printf("Enter your choice (1 or 2): ");
    scanf("%d", &room_choice);
    getchar();
  } while (room_choice != 1 && room_choice != 2);

  // Create socket
  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }

  // Set server address
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(room_choice == 1 ? PORT_ENGLISH_TO_ITALIAN
                                                : PORT_ITALIAN_TO_ENGLISH);
  if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
    perror("inet_pton failed");
    exit(EXIT_FAILURE);
  }

  // Connect to the server
  if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("connect failed");
    exit(EXIT_FAILURE);
  }

  printf("Connected to %s room\n",
         room_choice == 1 ? "English to Italian" : "Italian to English");

  while (1) {
    // Get user input
    printf("Enter message: ");
    fgets(message_buffer, BUFSIZE, stdin);

    // Remove newline character
    message_buffer[strcspn(message_buffer, "\n")] = 0;

    // Send message to server
    send(sockfd, message_buffer, strlen(message_buffer), 0);

    // When /ciao is sent, exit
    if (strcmp(message_buffer, "/ciao") == 0) {
      printf("Exiting...\n");
      break;
    }

    // Receive response from server
    int num_bytes_received = recv(sockfd, server_response_buffer, BUFSIZE, 0);
    if (num_bytes_received < 0) {
      perror("recv failed");
      continue;
    }

    server_response_buffer[num_bytes_received] = '\0';
    printf("Server response: %s\n", server_response_buffer);
  }

  close(sockfd);
  return 0;
}
