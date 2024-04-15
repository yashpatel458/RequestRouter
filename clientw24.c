// clientw24.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>  // Include the header file for inet_pton
#define BUFFER_SIZE 65536  // Increase buffer size

#define PORT 8080
#define SERVER_IP "127.0.0.1"  // Update this with the actual server IP address

int validate_command(const char* command);

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
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

    /*
    inet_ptron  converts  a  character string representing an IPv4 or IPv6 address into a binary address structure.  The af argument must be either AF_INET or AF_INET6.  
    The src argument points to a character string containing an IPv4 or IPv6 network address in dotted-decimal format, "ddd.ddd.ddd.ddd" or "xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx".  
    The dst argument points to a buffer where the function stores the resulting address structure.  The size of this buffer is determined by the af argument.  This function returns 1 if the address was valid for the specified address family, or 0 if the address was not parseable in the specified address family, or -1 if some other error occurred.
    
    */ 

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { // Socket descriptor, Address of the server, Size of the address
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
         memset(buffer, 0, sizeof(buffer));  // Clear buffer before reading
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
