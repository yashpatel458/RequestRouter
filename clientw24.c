// clientw24.c

/*
    ✨ ASP SECTION 5
    🚀 Submitted by:
    👨🏻‍💻 Yash Patel - 110128551 && Malhar Raval - 110128144
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>

#define BUFFER_SIZE 65536     // Define buffer size for socket communication
#define PORT 8080             // Server port
#define SERVER_IP "127.0.0.1" // Server IP

// Function Declartion
int validate_command(const char *command);

int main()
{
    int sock = 0;                   // Declare variable for socket file descriptor
    struct sockaddr_in serv_addr;   // Declare struct for server address
    char buffer[BUFFER_SIZE] = {0}; // Declare buffer for socket communication
    char command[256];              // Declare variable for user command input

    sock = socket(AF_INET, SOCK_STREAM, 0); // Create a socket
    if (sock < 0)
    {
        printf("\nSocket creation error\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;   // Set address family to IPv4
    serv_addr.sin_port = htons(PORT); // Set port number

    // Convert IPv4 address from text to binary form
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0)
    {
        printf("\nInvalid address/ Address not supported\n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    { // Socket descriptor, Address of the server, Size of the address
        printf("\nConnection Failed ❌\n");
        return -1;
    }

    printf("Connected to server ✅\n");

    while (1)
    {
        printf("\nEnter any command: \n (dirlist -a | dirlist -t | w24fn <filename> | w24fz <size1> <size2> | w24ft <extension list> | w24fdb <date> | w24fda <date>)\n\n");
        scanf(" %255[^\n]", command);

        if (!validate_command(command))
        {
            printf("Invalid command 🥲 \n");
            continue;
        }

        send(sock, command, strlen(command), 0);
        printf("\nCommand sent: %s\n", command);

        if (strcmp(command, "quitc") == 0)
        {
            break;
        }
        memset(buffer, 0, sizeof(buffer)); // Clear buffer before reading

        int bytes_read = read(sock, buffer, sizeof(buffer));
        if (bytes_read > 0)
        {
            printf("\n🟢 Response from server: %s\n", buffer);
        }
        else
        {
            printf("\n🔴 No response from server.\n");
        }
    }

    close(sock);
    return 0;
}

// Function to check if a string is a natural number
// Running a loop through each characters entered by the user to check if it's a digit
int is_natural_number(const char *str)
{
    while (*str)
    {
        if (!isdigit(*str))
        {
            return 0;
        }
        str++;
    }
    return 1;
}

// Function to validate all the commands input by user
// Returns 1 for true when command exists and Returns 0 for false
int validate_command(const char *command)
{
    if (strncmp(command, "dirlist -a", 10) == 0 || strncmp(command, "dirlist -t", 10) == 0)
    {
        return 1;
    }
    else if (strncmp(command, "w24fn ", 6) == 0)
    {
        return 1;
    }
    else if (strncmp(command, "w24fz ", 6) == 0)
    {
        int size1, size2;
        if (sscanf(command + 6, "%d %d", &size1, &size2) != 2)
        {
            printf("Invalid command format. Please use: w24fz <file size1> <file size2>\n");
            return 0;
        }
        if (size1 > size2 || size1 < 0 || size2 < 0 || !is_natural_number(strchr(command + 6, ' ') + 1) || !is_natural_number(strrchr(command + 6, ' ') + 1))
        {
            printf("Invalid size range from the client side.\n");
            return 0;
        }
        printf("Size range: %d - %d\n", size1, size2);
        return 1;
    }
    else if (strncmp(command, "w24ft ", 6) == 0)
    {
        char arg1[256], arg2[256], arg3[256];
        if (sscanf(command + 6, "%s %s %s", arg1, arg2, arg3) <= 0)
        {
            printf("Please use: w24ft <arg1> <arg2> <arg3>\n");
            return 0;
        }
        // Check number of arguments
        int arg_count = 0;                 // Declare variable for argument count
        const char *arg_ptr = command + 6;
        while (*arg_ptr != '\0')           // Loop for the arguments counter
        {
            if (*arg_ptr == ' ')
            {
                arg_count++;
            }
            arg_ptr++;
        }
        arg_count++; // Increment for the last argument

        if (arg_count > 3)  // Check if more than 3 arguments are provided
        {
            printf("Too many arguments. Please provide a maximum of 3 arguments.\n");
            return 0;
        }
        return 1;
    }
    else if (strncmp(command, "w24fdb ", 7) == 0 || strncmp(command, "w24fda ", 7) == 0)
    {
        return 1;
    }
    else if (strcmp(command, "quitc") == 0)
    {
        return 1;
    }
    return 0;
}
