#!/bin/sh

gcc -o ./server/s ./server/server.c ./hash_table/hash_table.c ./hash_table/prime.c ./client_queue/client_queue.c -lm

gcc -o ./client/c ./client/client.c ./auth/user_auth.c

./server/s
