#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "../auth/user_auth.h"

#define SERVER_IP "127.0.0.1"
#define PORT_ENGLISH_TO_ITALIAN 8080
#define PORT_ITALIAN_TO_ENGLISH 6969
#define BUFSIZE 1024
#define GREEN_COLOR "\033[0;32m"
#define RESET_COLOR "\033[0m"

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

// Room selection
//
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// Configure terminal for single character input
void enable_raw_mode() {
  struct termios term;
  tcgetattr(0, &term);
  term.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(0, TCSANOW, &term);
}

// Restore terminal to normal mode
void disable_raw_mode() {
  struct termios term;
  tcgetattr(0, &term);
  term.c_lflag |= ICANON | ECHO;
  tcsetattr(0, TCSANOW, &term);
}

// Read a single character
char getch() {
  char buf = 0;
  if (read(0, &buf, 1) < 0)
    return -1;
  return buf;
}

int choose_room() {
  int selected = 1;
  char key;

  enable_raw_mode();

  do {
    // Clear screen (ANSI escape code)
    printf("\033[2J\033[H");

    // Display menu
    printf("Choose a room:\n");
    printf("%s%c%s English to Italian\n", selected == 1 ? GREEN_COLOR : "",
           selected == 1 ? '>' : ' ', RESET_COLOR);
    printf("%s%c%s Italian to English\n", selected == 2 ? GREEN_COLOR : "",
           selected == 2 ? '>' : ' ', RESET_COLOR);
    printf("\nUse arrow up/down or j/k to select, Enter to confirm\n");

    // Get user input
    key = getch();

    if (key == 27) { // ESC character
      getch();       // Skip the [
      key = getch();
    }

    // Update selection based on input
    switch (key) {
    case 65:
    case 'k': // Up arrow or 'k'
      selected = (selected == 1) ? 2 : 1;
      break;
    case 66:
    case 'j': // Down arrow or 'j'
      selected = (selected == 2) ? 1 : 2;
      break;
    case 10: // Enter key
      disable_raw_mode();
      return selected;
    }
  } while (1);
}
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++

void initialize_user(user *user, const char *username, const char *password,
                     const char *language) {
  strncpy(user->username, username, MAX_USERNAME_LENGTH);
  user->username[MAX_USERNAME_LENGTH - 1] = '\0';

  strncpy(user->password, password, MAX_PASSWORD_LENGTH);
  user->password[MAX_PASSWORD_LENGTH - 1] = '\0';

  strncpy(user->language, language, MAX_LANGUAGE_LENGTH);
  user->language[MAX_LANGUAGE_LENGTH - 1] = '\0';
}

// Authentication
//
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
user *registration_phase() {
  char username[MAX_USERNAME_LENGTH], password[MAX_PASSWORD_LENGTH],
      language[MAX_LANGUAGE_LENGTH];
  bool registration_check = 0;
  user *user = NULL;

  printf("--- Hello! Welcome to Chatlingo, register here ---\n");

  do {
    printf("Username: ");
    scanf("%s", username);
    printf("Password: ");
    scanf("%s", password);
    printf("Language: ");
    scanf("%s", language);

    registration_check = register_user(username, password, language);

    if (registration_check == 0) {
      printf("Username already exists, try again.\n");
    } else {
      printf("Welcome, you are now registered!\n");
      printf("Redirecting you to room selection...\n");

      user = malloc(sizeof(*user));
      if (user == NULL) {
        fprintf(stderr, "User registration memory allocation failed\n");
        return NULL;
      }

      initialize_user(user, username, password, language);

      sleep(1);
      system("clear");
    }
  } while (registration_check == 0);

  return user;
}

user *login_phase() {
  char username[MAX_USERNAME_LENGTH], password[MAX_PASSWORD_LENGTH],
      language[MAX_LANGUAGE_LENGTH];
  int login_choice = 0;
  user *user = NULL;

  printf("--- LOGIN ---\n");

  do {
    printf("Username: ");
    scanf("%s", username);
    printf("Password: ");
    scanf("%s", password);

    user = login(username, password);

    if (user == NULL) {
      printf("User not present in the database, you need to register.\n");
      printf("Do you want try again (1) or you want to register (2)? ");
      scanf("%d", &login_choice);

      switch (login_choice) {
      case 1:
        break;

      case 2:
        user = registration_phase();
        break;

      default:
        printf("Wrong choice!\n");
        login_choice = 1;
        break;
      }

    } else {
      printf("Welcome, you are now logged in!\n");
      printf("Redirecting you to room selection...\n");

      user = malloc(sizeof(*user));
      if (user == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
      }

      initialize_user(user, username, password, language);

      sleep(1);
      system("clear");
    }
  } while (login_choice == 1 && user == NULL);

  return user;
}

void login_or_registration_selection(user **user) {
  if (*user == NULL) {
    int selected = 1;
    char key;

    enable_raw_mode();

    do {
      // Clear screen (ANSI escape code)
      printf("\033[2J\033[H");

      // Display menu
      printf("--- Welcome to Chatlingo ---\n\n");
      printf("Choose an option:\n");
      printf("%s%c%s Login\n", selected == 1 ? GREEN_COLOR : "",
             selected == 1 ? '>' : ' ', RESET_COLOR);
      printf("%s%c%s Register\n", selected == 2 ? GREEN_COLOR : "",
             selected == 2 ? '>' : ' ', RESET_COLOR);
      printf("\nUse arrow up/down or j/k to select, Enter to confirm\n");

      // Get user input
      key = getch();

      // Handle arrow keys (they send a sequence of characters)
      if (key == 27) { // ESC character
        getch();       // Skip the [
        key = getch();
      }

      // Update selection based on input
      switch (key) {
      case 65:
      case 'k': // Up arrow or 'k'
        selected = (selected == 1) ? 2 : 1;
        break;
      case 66:
      case 'j': // Down arrow or 'j'
        selected = (selected == 2) ? 1 : 2;
        break;
      case 10: // Enter key
        disable_raw_mode();
        switch (selected) {
        case 1:
          *user = login_phase();
          break;
        case 2:
          *user = registration_phase();
          break;
        }
        return;
      }
    } while (1);
  }
}
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++

int main() {
  int sockfd;
  char server_response_buffer[BUFSIZE];
  char message_buffer[BUFSIZE];
  int room_choice;
  user *user = NULL;

  while (1) {
    // Authentication
    //
    login_or_registration_selection(&user);

    // Room choice
    //
    room_choice = choose_room();
    fflush(stdin);
    sockfd = connect_to_server(room_choice);

    if (sockfd < 0) {
      printf("Failed to connect. Try again.\n");
      continue;
    }

    // Inside the room
    //
    while (1) {
      // Get user input
      int username_length = strlen(user->username);
      char message_with_user[BUFSIZE + username_length + 4];

      // loop till the message is empty or only contains whitespace
      do {
        printf("Enter message: ");
        if (fgets(message_buffer, BUFSIZE, stdin) == NULL) {
          perror("Error reading input");
          return -1;
        }

        message_buffer[strcspn(message_buffer, "\n")] = '\0';

      } while (strspn(message_buffer, " \t\n\r") == strlen(message_buffer));

      snprintf(message_with_user, sizeof(message_with_user), "%s: %s",
               user->username, message_buffer);

      message_with_user[strcspn(message_with_user, "\n")] = '\0';

      // Send message to server
      if (send(sockfd, message_with_user, strlen(message_with_user), 0) < 0) {
        perror("send failed");
        break;
      }

      // When /ciao is sent, close current connection and go back to room
      // selection
      if (strcmp(message_buffer, "/ciao") == 0 ||
          strcmp(message_buffer, "/exit") == 0) {
        printf("Disconnecting from current room. Sending you back to room "
               "selection...\n");
        close(sockfd);

        sleep(1);
        system("clear");
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
      // printf("Server response: %s\n", server_response_buffer);
    }

    printf("Do you want to choose another room? (y/n): ");
    char choice;
    scanf(" %c", &choice);
    getchar();

    if (choice != 'y' && choice != 'Y') {
      printf("Exiting...\n");

      sleep(1);
      system("clear");
      break;
    }
  }

  return 0;
}
