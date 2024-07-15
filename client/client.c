#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "../auth/user_auth.h"

#define SERVER_IP "127.0.0.1"
#define PORT_ENGLISH_TO_ITALIAN 8080
#define PORT_ITALIAN_TO_ENGLISH 6969
#define BUFSIZE 1024
#define GREEN_COLOR "\033[0;32m"
#define RESET_COLOR "\033[0m"
#define MAX_INACTIVE_TIME_IN_SECONDS 5

atomic_bool is_in_room = false;
atomic_bool should_kick_inactive_user = false;
atomic_bool is_server_running = true;

pthread_t inactivity_thread;

time_t last_activity_time;
pthread_mutex_t activity_mutex = PTHREAD_MUTEX_INITIALIZER;

// Inactivity thread to listen and act in case of inactivity
void *inactivity_check_thread(void *arg) {
  while (atomic_load(&is_in_room)) {
    pthread_mutex_lock(&activity_mutex);
    time_t current_time = time(NULL);
    if (difftime(current_time, last_activity_time) >
        MAX_INACTIVE_TIME_IN_SECONDS) {
      system("clear");
      printf("You are now inactive, you are kicked out from the room!!!\n");
      printf("Press a key to continue\n");
      atomic_store(&should_kick_inactive_user, true);
    }
    pthread_mutex_unlock(&activity_mutex);
    sleep(1);
  }

  return NULL;
}

void update_activity_time() {
  pthread_mutex_lock(&activity_mutex);
  last_activity_time = time(NULL);
  pthread_mutex_unlock(&activity_mutex);
}

// Functions to enable menu arrow selection
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
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++

// Room selection
//
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
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

  // Convert string into IP address format
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
  int selected = 1;
  char key;

  enable_raw_mode();

  do {
    system("clear");

    // Display menu
    printf("Choose a room:\n");
    printf("%s%c%s English to Italian\n", selected == 1 ? GREEN_COLOR : "",
           selected == 1 ? '>' : ' ', RESET_COLOR);
    printf("%s%c%s Italian to English\n", selected == 2 ? GREEN_COLOR : "",
           selected == 2 ? '>' : ' ', RESET_COLOR);
    printf("\nUse arrow up/down or j/k to select, Enter to confirm\n");

    // Get user input
    key = getch();

    // If ESC
    if (key == 27) {
      getch();
      key = getch();
    }

    // Update selection based on input
    switch (key) {
    case 65: // up arrow
    case 'k':
      selected = (selected == 1) ? 2 : 1;
      break;
    case 66: // down arrow
    case 'j':
      selected = (selected == 2) ? 1 : 2;
      break;
    case 10: // Enter
      disable_raw_mode();
      return selected;
    }
  } while (1);
}
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++

// Authentication
//
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
void initialize_user_authentication(user *user, const char *username,
                                    const char *password,
                                    const char *language) {
  strncpy(user->username, username, MAX_USERNAME_LENGTH);
  user->username[MAX_USERNAME_LENGTH - 1] = '\0';

  strncpy(user->password, password, MAX_PASSWORD_LENGTH);
  user->password[MAX_PASSWORD_LENGTH - 1] = '\0';

  strncpy(user->language, language, MAX_LANGUAGE_LENGTH);
  user->language[MAX_LANGUAGE_LENGTH - 1] = '\0';
}

user *registration_phase() {
  char username[MAX_USERNAME_LENGTH], password[MAX_PASSWORD_LENGTH],
      language[MAX_LANGUAGE_LENGTH];
  bool is_new_user = 0;
  user *user = NULL;

  printf("--- Hello! Welcome to Chatlingo, register here ---\n");

  do {
    printf("Username: ");
    scanf("%s", username);
    printf("Password: ");
    scanf("%s", password);
    printf("Language: ");
    scanf("%s", language);

    is_new_user = register_user(username, password, language);

    if (is_new_user == 0) {
      printf("Username already exists, try again.\n");
    } else {
      printf("Welcome, you are now registered!\n");
      printf("Redirecting you to room selection...\n");

      user = malloc(sizeof(*user));
      if (user == NULL) {
        fprintf(stderr, "User registration memory allocation failed\n");
        return NULL;
      }

      initialize_user_authentication(user, username, password, language);

      sleep(1);
    }
  } while (is_new_user == 0);

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

      initialize_user_authentication(user, username, password, language);

      sleep(1);
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
      system("clear");

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
        getch();
        key = getch();
      }

      // Update selection based on input
      switch (key) {
      case 65: // up arrow
      case 'k':
        selected = (selected == 1) ? 2 : 1;
        break;
      case 66: // down arrow
      case 'j':
        selected = (selected == 2) ? 1 : 2;
        break;
      case 10: // Enter
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

// Thread that listen for server shutdown message
// void *receive_shutdown_message_thread(void *socket_desc) {
//  int sockfd = *(int *)socket_desc;
//  char server_shutdown_buffer[BUFSIZE];
//
//  while (atomic_load(&is_server_running)) {
//    memset(server_shutdown_buffer, 0, BUFSIZE);
//    int bytes_received_shutdown =
//        recv(sockfd, server_shutdown_buffer, BUFSIZE - 1, 0);
//    if (bytes_received_shutdown > 0) {
//      server_shutdown_buffer[bytes_received_shutdown] = '\0';
//      if (strcmp(server_shutdown_buffer, "SERVER_SHUTDOWN") == 0) {
//        printf("Server is shutting down. Disconnecting...\n");
//        atomic_store(&is_server_running, false);
//        break;
//      }
//    } else if (bytes_received_shutdown == 0 ||
//               (bytes_received_shutdown == -1 && errno != EWOULDBLOCK &&
//                errno != EAGAIN)) {
//      printf("Server disconnected.\n");
//      atomic_store(&is_server_running, false);
//      break;
//    }
//  }
//
//  close(sockfd);
//  return NULL;
//}

int main() {
  int sockfd;
  char server_response_buffer[BUFSIZE];
  char message_buffer[BUFSIZE];
  int room_choice;
  user *user = NULL;

  // pthread_t server_shutdown_thread;

  // if (pthread_create(&server_shutdown_thread, NULL,
  //                    receive_shutdown_message_thread, (void *)&sockfd) < 0) {
  //   perror("Could not create receive thread");
  //   close(sockfd);
  // }

  // while (is_server_running) {
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

    atomic_store(&is_in_room, true);
    last_activity_time = time(NULL);
    if (pthread_create(&inactivity_thread, NULL, inactivity_check_thread,
                       NULL) != 0) {
      perror("Failed to create inactivity check thread");
    }

    // Inside the room
    //
    while (atomic_load(&is_in_room) &&
           !atomic_load(&should_kick_inactive_user)) {
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

      update_activity_time();

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

        atomic_store(&is_in_room, false);
        pthread_join(inactivity_thread, NULL);

        close(sockfd);

        sleep(1);
        break;
      }

      // Receive response from server
      //
      memset(server_response_buffer, 0, BUFSIZE);
      int bytes_received_room_locked =
          recv(sockfd, server_response_buffer, BUFSIZE - 1, 0);
      if (bytes_received_room_locked <= 0) {
        if (bytes_received_room_locked == 0) {
          printf("Server disconnected.\n");
        } else {
          perror("recv failed");
        }
        close(sockfd);
        break;
      }

      server_response_buffer[bytes_received_room_locked] = '\0';

      // printf("Server response: %s\n", server_response_buffer);

      // Waiting queue if the room is full = locked
      if (strcmp(server_response_buffer, "LOCKED") == 0) {
        printf("Can't send the message because the server room is full, try "
               "again after some time.\n");
        printf("If you want to select another room, choose 'q'\n");
        printf("Otherwise choose 'r' to enter in the queue\n");

        atomic_store(&is_in_room, false);
        pthread_join(inactivity_thread, NULL);

        char exit_choice;
        do {
          printf("What you want to do? \n");
          scanf(" %c", &exit_choice);
          getchar();

          // Enter the queue and wait for NOT LOCKED message from the server
          if (exit_choice != 'q') {
            system("clear");
            printf("You are in queue now, wait for your turn...\n");

            memset(server_response_buffer, 0, BUFSIZE);
            int bytes_received_queue =
                recv(sockfd, server_response_buffer, BUFSIZE - 1, 0);
            if (bytes_received_queue <= 0) {
              if (bytes_received_queue == 0) {
                printf("Server disconnected.\n");
              } else {
                perror("recv failed");
              }
              close(sockfd);
              break;
            }

            server_response_buffer[bytes_received_queue] = '\0';

            if (strcmp(server_response_buffer, "LOCKED") == 0) {
              printf("Room is full, still waiting...\n");
              sleep(1);
            } else {
              sockfd = connect_to_server(room_choice);

              atomic_store(&is_in_room, true);
              last_activity_time = time(NULL);
              if (pthread_create(&inactivity_thread, NULL,
                                 inactivity_check_thread, NULL) != 0) {
                perror("Failed to create inactivity check thread");
              }

              break;
            }
          }
        } while (exit_choice != 'q');

        if (strcmp(server_response_buffer, "LOCKED") == 0) {
          break;
        }
      }

      if (atomic_load(&should_kick_inactive_user)) {
        printf("You have been kicked from the room due to inactivity.\n");
        printf("Redirecting you to room selection...\n");

        memset(message_buffer, 0, BUFSIZE);
        strcpy(message_buffer, "KICKED");

        if (send(sockfd, message_buffer, strlen(message_buffer), 0) < 0) {
          perror("send failed");
          break;
        }

        sleep(1);
        break;
      }
    }

    atomic_store(&should_kick_inactive_user, false);
    atomic_store(&is_in_room, false);

    system("clear");

    printf("Do you want to choose another room?\n");
    printf("if not you will exit from the chat!\n");
    printf("(y/n): ");
    char choice;
    scanf(" %c", &choice);
    getchar();

    if (choice != 'y' && choice != 'Y') {
      sleep(1);
      printf("Exiting...\n");
      sleep(1);
      break;
    }

    printf("Redirecting you to room choice...\n");
    sleep(1);
  }

  pthread_join(inactivity_thread, NULL);
  // pthread_join(server_shutdown_thread, NULL);

  return 0;
}
