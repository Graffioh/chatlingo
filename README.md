# chatlingo

**DISCLAIMER**: This is a toy project

Client-Server chat app implemented with sockets and a TCP connection.

Enter in a room, and chat with others, but there is a catch, based on the room your messages will be translated!

The translation is really basic, because it was not the purpose of this project, the vocabulary is implemented with an hash table to ensure O(1) time complexity for search operations.

User authentication via a simple .txt file.

Everything is multi-threaded, so rooms, multiple clients and inactivity detection mechanism.

## Features

- User authentication
- 2 Chat rooms (English -> Italian and viceversa)
- Full room queue
- Inactivity kick

## How to run

### Without docker compose

- Run b.sh (it will compile and run the server)
- Open a new terminal window
- Run /client/c (as many as you want)

### With docker compose

- Run docker-compose up server
- Open a new terminal window for every client that you want to connect
- For every client run docker-compose run client

## Demo

https://github.com/user-attachments/assets/1b83271f-7c5d-47d4-9880-f0788c2eefab

