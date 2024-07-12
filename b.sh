#!/bin/sh

DEBUG_CLIENT=0
DEBUG_SERVER=0

for arg in "$@"
do
    case $arg in
        debug-client)
        DEBUG_CLIENT=1
        shift
        ;;
        debug-server)
        DEBUG_SERVER=1
        shift
        ;;
    esac
done

if [ $DEBUG_SERVER -eq 1 ]; then
    echo "SERVER DEBUG MODE ACTIVE! (YOU NEED TO MANUALLY RUN THE CLIENT)"
    gcc --debug -o ./server/s ./server/server.c ./hash_table/hash_table.c ./hash_table/prime.c -lm
else
    gcc -o ./server/s ./server/server.c ./hash_table/hash_table.c ./hash_table/prime.c -lm
    ./server/s
fi

if [ $DEBUG_CLIENT -eq 1 ]; then
    echo "CLIENT DEBUG MODE ACTIVE!"
    gcc --debug -o ./client/c ./client/client.c ./auth/user_auth.c
else
    gcc -o ./client/c ./client/client.c ./auth/user_auth.c
fi
