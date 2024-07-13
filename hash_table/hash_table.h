#ifndef HASHTABLE_H
#define HASHTABLE_H

typedef struct {
  char *key;
  char *value;
} ht_item;

typedef struct {
  int base_size;
  int size;
  int count;
  ht_item **items;
} ht_hash_table;

ht_hash_table *ht_new();

void ht_del_hash_table(ht_hash_table *ht);

void ht_insert(ht_hash_table *ht, const char *key, const char *value);
char *ht_search(ht_hash_table *ht, const char *key);
void ht_delete(ht_hash_table *h, const char *key);

#endif // HASHTABLE_H
