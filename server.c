#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PORT 8080
#define BUFSIZE 1024
#define MAX_LENGTH 1000

int main() {
    int server_fd, new_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFSIZE];

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind the socket to the address
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
        0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("TCP Server is listening on port %d...\n", PORT);

    // Vocab setup
    //
    // Read from vocab.txt
    FILE *file = fopen("vocab.txt", "r");
    if (file == NULL) {
        printf("Error opening file.\n");
        return 1;
    }

    char line[MAX_LENGTH];
    while (fgets(line, MAX_LENGTH, file) != NULL) {
        printf("%s", line);
    }

    fclose(file);

    while (1) {
        // Accept a new connection
        if ((new_socket = accept(server_fd, (struct sockaddr *)&client_addr,
                                 &addr_len)) < 0) {
            perror("accept failed");
            continue;
        }

        // Print connected client's information
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        printf("Client connected: IP = %s, Port = %d\n", client_ip,
               ntohs(client_addr.sin_port));

        while (1) {
            // Receive data from client
            int bytes_received = recv(new_socket, buffer, BUFSIZE, 0);
            if (bytes_received < 0) {
                perror("recv failed");
                break;
            }

            buffer[bytes_received] = '\0';
            printf("Received from client: %s\n", buffer);

            // Send a response back to the client
            const char *response = "Message received!";
            send(new_socket, response, strlen(response), 0);

            if (strcmp(buffer, "/ciao") == 0) {
                printf("Client requested to close the connection.\n");
                break;
            }
        }

        close(new_socket);
    }

    close(server_fd);
    return 0;
}