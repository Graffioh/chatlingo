#!/bin/sh

gcc -o ./server/s ./server/server.c ./hash_table/hash_table.c ./hash_table/prime.c -lm
gcc -o ./client/c ./client/client.c

./server/s
