#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SERVER_IP "127.0.0.1"
#define PORT_ENGLISH_TO_ITALIAN 8080
#define PORT_ITALIAN_TO_ENGLISH 6969
#define BUFSIZE 1024

int connect_to_server(int room_choice) {
  int sockfd;
  struct sockaddr_in server_addr;

  // Create socket
  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket creation failed");
    return -1;
  }

  // Set server address
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(room_choice == 1 ? PORT_ENGLISH_TO_ITALIAN
                                                : PORT_ITALIAN_TO_ENGLISH);

  if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
    perror("inet_pton failed");
    close(sockfd);
    return -1;
  }

  // Connect to the server
  if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("connect failed");
    close(sockfd);
    return -1;
  }

  printf("Connected to %s room\n",
         room_choice == 1 ? "English to Italian" : "Italian to English");
  return sockfd;
}

int choose_room() {
  int room_choice;
  do {
    printf("Choose a room:\n");
    printf("1. English to Italian\n");
    printf("2. Italian to English\n");
    printf("Enter your choice (1 or 2): ");
    scanf("%d", &room_choice);
    getchar(); // Consume newline
  } while (room_choice != 1 && room_choice != 2);
  return room_choice;
}

int main() {
  int sockfd;
  char server_response_buffer[BUFSIZE];
  char message_buffer[BUFSIZE];
  int room_choice;

  while (1) {
    room_choice = choose_room();
    sockfd = connect_to_server(room_choice);

    if (sockfd < 0) {
      printf("Failed to connect. Try again.\n");
      continue;
    }

    while (1) {
      // Get user input
      printf("Enter message: ");
      fgets(message_buffer, BUFSIZE, stdin);
      message_buffer[strcspn(message_buffer, "\n")] = 0; // Remove newline

      // Send message to server
      if (send(sockfd, message_buffer, strlen(message_buffer), 0) < 0) {
        perror("send failed");
        break;
      }

      // When /ciao is sent, close current connection and go back to room
      // selection
      if (strcmp(message_buffer, "/ciao") == 0 ||
          strcmp(message_buffer, "/exit") == 0) {
        printf("Disconnected from current room.\n");
        close(sockfd);
        break;
      }

      // Receive response from server
      int num_bytes_received = recv(sockfd, server_response_buffer, BUFSIZE, 0);
      if (num_bytes_received <= 0) {
        if (num_bytes_received == 0) {
          printf("Server disconnected.\n");
        } else {
          perror("recv failed");
        }
        close(sockfd);
        break;
      }

      server_response_buffer[num_bytes_received] = '\0';
      printf("Server response: %s\n", server_response_buffer);
    }

    printf("Do you want to choose another room? (y/n): ");
    char choice;
    scanf(" %c", &choice);
    getchar(); // Consume newline
    if (choice != 'y' && choice != 'Y') {
      printf("Exiting...\n");
      break;
    }
  }

  return 0;
}
