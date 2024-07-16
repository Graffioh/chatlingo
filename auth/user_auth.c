#include "user_auth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

user *register_user(user *user, const char *username, const char *password,
                    const char *language) {
  FILE *file = fopen(USERS_FILE, "a+");
  if (file == NULL) {
    perror("Error opening file");
    return NULL;
  }

  char
      line[MAX_USERNAME_LENGTH + MAX_PASSWORD_LENGTH + MAX_LANGUAGE_LENGTH + 3];

  // Check if username already exists
  rewind(file);
  while (fgets(line, sizeof(line), file)) {
    char stored_username[MAX_USERNAME_LENGTH];
    if (sscanf(line, "%[^,]", stored_username) == 1) {
      if (strcmp(stored_username, username) == 0) {
        fclose(file);
        return NULL; // Username already exists
      }
    }
  }

  // Allocate memory for the user struct
  user = malloc(sizeof(*user));
  if (user == NULL) {
    fprintf(stderr, "User registration memory allocation failed\n");
    fclose(file);
    return NULL;
  }

  // Copy user data into the struct
  strncpy(user->username, username, MAX_USERNAME_LENGTH - 1);
  user->username[MAX_USERNAME_LENGTH - 1] = '\0';

  strncpy(user->password, password, MAX_PASSWORD_LENGTH - 1);
  user->password[MAX_PASSWORD_LENGTH - 1] = '\0';

  strncpy(user->language, language, MAX_LANGUAGE_LENGTH - 1);
  user->language[MAX_LANGUAGE_LENGTH - 1] = '\0';

  // Add new user to file
  fprintf(file, "%s,%s,%s\n", username, password, language);
  fclose(file);

  return user;
}

user *login(const char *username, const char *password) {
  FILE *file = fopen(USERS_FILE, "r");
  if (file == NULL) {
    perror("Error opening file");
    return NULL;
  }

  char
      line[MAX_USERNAME_LENGTH + MAX_PASSWORD_LENGTH + MAX_LANGUAGE_LENGTH + 3];
  user *user = NULL;

  while (fgets(line, sizeof(line), file)) {
    char stored_username[MAX_USERNAME_LENGTH];
    char stored_password[MAX_PASSWORD_LENGTH];
    char stored_language[MAX_LANGUAGE_LENGTH];

    // Parse all three fields: username, password, and language
    if (sscanf(line, "%[^,],%[^,],%[^\n]", stored_username, stored_password,
               stored_language) == 3) {
      if (strcmp(stored_username, username) == 0 &&
          strcmp(stored_password, password) == 0) {
        user = malloc(sizeof(*user));
        if (user == NULL) {
          fprintf(stderr, "User login memory allocation failed\n");
          fclose(file);
          return NULL;
        }

        // Copy the data into the user struct
        strncpy(user->username, stored_username, MAX_USERNAME_LENGTH - 1);
        user->username[MAX_USERNAME_LENGTH - 1] = '\0';

        strncpy(user->password, stored_password, MAX_PASSWORD_LENGTH - 1);
        user->password[MAX_PASSWORD_LENGTH - 1] = '\0';

        strncpy(user->language, stored_language, MAX_LANGUAGE_LENGTH - 1);
        user->language[MAX_LANGUAGE_LENGTH - 1] = '\0';

        break; // We found the user, no need to continue searching
      }
    }
  }

  fclose(file);
  return user;
}
