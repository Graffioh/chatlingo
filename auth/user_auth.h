#ifndef USER_AUTH_H
#define USER_AUTH_H

#include <stdbool.h>

#define MAX_USERNAME_LENGTH 50
#define MAX_PASSWORD_LENGTH 50
#define MAX_LANGUAGE_LENGTH 10
#define USERS_FILE "../auth/users.txt"

typedef struct {
  char username[MAX_USERNAME_LENGTH];
  char password[MAX_PASSWORD_LENGTH];
  char language[MAX_LANGUAGE_LENGTH];
  char user_port[50];
} user;

bool register_user(const char *username, const char *password,
                   const char *language);
user *login(const char *username, const char *password);

#endif // USER_AUTH_H
