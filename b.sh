#!/bin/sh

gcc -o ./server/s ./server/server.c
gcc -o ./client/c ./client/client.c

./server/s