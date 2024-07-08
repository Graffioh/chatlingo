#include "user_auth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool register_user(const char *username, const char *password,
                   const char *language) {
  FILE *file = fopen(USERS_FILE, "a+");
  if (file == NULL) {
    perror("Error opening file");
    return false;
  }

  // Check if username already exists
  char
      line[MAX_USERNAME_LENGTH + MAX_PASSWORD_LENGTH + MAX_LANGUAGE_LENGTH + 3];
  rewind(file);
  while (fgets(line, sizeof(line), file)) {
    if (strncmp(line, username, strlen(username)) == 0) {
      fclose(file);
      return false;
    }
  }

  // Add new user
  fprintf(file, "%s,%s,%s\n", username, password, language);
  fclose(file);
  return true;
}

user *login(const char *username, const char *password) {
  FILE *file = fopen(USERS_FILE, "r");
  if (file == NULL) {
    perror("Error opening file");
    return false;
  }

  char
      line[MAX_USERNAME_LENGTH + MAX_PASSWORD_LENGTH + MAX_LANGUAGE_LENGTH + 3];

  user *user = NULL;

  // Check if the user is already present in the file
  while (fgets(line, sizeof(line), file)) {
    char stored_username[MAX_USERNAME_LENGTH];
    char stored_password[MAX_PASSWORD_LENGTH];
    sscanf(line, "%[^,],%[^,]", stored_username, stored_password);

    if (strcmp(stored_username, username) == 0 &&
        strcmp(stored_password, password) == 0) {
      fclose(file);

      user = malloc(sizeof(*user));
      if (user == NULL) {
        fprintf(stderr, "User login memory allocation failed\n");
        return NULL;
      }
    }
  }

  fclose(file);
  return user;
}
