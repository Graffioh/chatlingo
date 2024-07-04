#include <stdio.h>

#include "hash_table.h"

int main() {
  ht_hash_table *ht = ht_new();
  ht_insert(ht, "Hello", "Ciao");

  char *str = ht_search(ht, "Hello");

  printf("KEY '%s' FOUND!, VALUE: '%s'\n", "Hello", str);

  ht_del_hash_table(ht);

  return 0;
}
