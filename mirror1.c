// mirror1.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PORT 8081

int should_handle_connection();

int main() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 3);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        if (should_handle_connection()) {
            // Fork a child process to handle the request
            if (fork() == 0) {
                close(server_fd);
                crequest(client_fd);
                exit(0);
            }
        }
        close(client_fd);
    }

    return 0;
}

int should_handle_connection() {
    // Implement your logic here to handle connections for mirror1
    return 1;  // For demonstration, always handle connections
}

void crequest(int client_fd) {
    char buffer[1024];
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        read(client_fd, buffer, sizeof(buffer));
        printf("Mirror1 command received: %s\n", buffer);

        // Process commands specific to mirror1 if any

        char *message = "Response from Mirror1";
        write(client_fd, message, strlen(message));

        if (strcmp(buffer, "quitc") == 0) break;
    }
    close(client_fd);
}
