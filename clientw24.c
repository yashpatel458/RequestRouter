// clientw24.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define PORT 8080
#define SERVER_IP "127.0.0.1"  // Update this with the actual server IP address

int validate_command(const char* command);

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};
    char command[256];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("\nSocket creation error\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported\n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed\n");
        return -1;
    }

    printf("Connected to server\n");

    while (1) {
        printf("Enter command: ");
        scanf(" %255[^\n]", command);

        if (!validate_command(command)) {
            printf("Invalid command\n");
            continue;
        }

        send(sock, command, strlen(command), 0);
        printf("Command sent: %s\n", command);

        if (strcmp(command, "quitc") == 0) {
            break;
        }

        int bytes_read = read(sock, buffer, sizeof(buffer));
        if (bytes_read > 0) {
            printf("Response from server: %s\n", buffer);
        } else {
            printf("No response from server.\n");
        }
    }

    close(sock);
    return 0;
}

int validate_command(const char* command) {
    // Improved validation logic
    if (strncmp(command, "dirlist -a", 10) == 0 || strncmp(command, "dirlist -t", 10) == 0) {
        return 1;  // Valid dirlist commands
    } else if (strncmp(command, "w24fn ", 6) == 0) {
        // Validate w24fn command further if necessary
        return 1;
    } else if (strncmp(command, "w24fz ", 6) == 0) {
        // Validate w24fz command further if necessary
        return 1;
    } else if (strncmp(command, "w24ft ", 6) == 0) {
        // Validate w24ft command further if necessary
        return 1;
    } else if (strncmp(command, "w24fdb ", 7) == 0 || strncmp(command, "w24fda ", 7) == 0) {
        // Validate w24fdb and w24fda commands further if necessary
        return 1;
    } else if (strcmp(command, "quitc") == 0) {
        return 1;  // Valid quit command
    }
    return 0;  // Invalid command
}
