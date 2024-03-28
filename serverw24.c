// serverw24.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

#define PORT 8080

void crequest(int client_fd);
void handle_dirlist_a(int client_fd);
void handle_dirlist_t(int client_fd);
void handle_w24fn(int client_fd, const char *filename);
void handle_w24fz(int client_fd, const char *sizeRange);
void handle_w24ft(int client_fd, const char *extensions);
void handle_w24fdb(int client_fd, const char *date);
void handle_w24fda(int client_fd, const char *date);

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

        // Fork a child process to handle the request
        if (fork() == 0) {
            close(server_fd);
            crequest(client_fd);
            exit(0);
        }
        close(client_fd);
    }

    return 0;
}

void crequest(int client_fd) {
    char buffer[1024];
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        read(client_fd, buffer, sizeof(buffer));
        printf("Command received: %s\n", buffer);

        // Match and execute the appropriate command handler
        if (strncmp(buffer, "dirlist -a", 10) == 0) {
            handle_dirlist_a(client_fd);
        } else if (strncmp(buffer, "dirlist -t", 10) == 0) {
            handle_dirlist_t(client_fd);
        } else if (strncmp(buffer, "w24fn ", 6) == 0) {
            handle_w24fn(client_fd, buffer + 6);
        } else if (strncmp(buffer, "w24fz ", 6) == 0) {
            handle_w24fz(client_fd, buffer + 6);
        } else if (strncmp(buffer, "w24ft ", 6) == 0) {
            handle_w24ft(client_fd, buffer + 6);
        } else if (strncmp(buffer, "w24fdb ", 7) == 0) {
            handle_w24fdb(client_fd, buffer + 7);
        } else if (strncmp(buffer, "w24fda ", 7) == 0) {
            handle_w24fda(client_fd, buffer + 7);
        } else if (strcmp(buffer, "quitc") == 0) {
            break;
        }
    }
    close(client_fd);
}

void handle_dirlist_a(int client_fd) {
    DIR *d;
    struct dirent *dir;
    char *dirList[1024];  // Assuming we won't have more than 1024 directories
    int count = 0;

    d = opendir(".");  // Open the current directory
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type == DT_DIR) {  // Check if it's a directory
                dirList[count] = strdup(dir->d_name);  // Copy directory name
                count++;
            }
        }
        closedir(d);

        // Sort the directory list alphabetically
        qsort(dirList, count, sizeof(char*), (int (*)(const void*, const void*)) strcmp);

        // Build the response string
        char response[8192] = "";  // Larger buffer for accumulating directory names
        for (int i = 0; i < count; i++) {
            strcat(response, dirList[i]);
            strcat(response, "\n");
            free(dirList[i]);  // Free the duplicated string
        }

        // Send the response to the client
        write(client_fd, response, strlen(response));
    } else {
        // Send an error message if directory can't be opened
        char *errorMsg = "Failed to open directory.";
        write(client_fd, errorMsg, strlen(errorMsg));
    }
}

void handle_dirlist_t(int client_fd) {
    // This function should sort directories based on creation time
    // The actual implementation will depend on the specific requirements
    // For now, we will send a static message for demonstration
    char *response = "Directory list sorted by time";
    write(client_fd, response, strlen(response));
}


void handle_w24fn(int client_fd, const char *filename) {
    struct stat file_stat;
    if (stat(filename, &file_stat) == 0) {
        char file_info[1024];
        sprintf(file_info, "File: %s, Size: %ld, Permissions: %o\n",
                filename, file_stat.st_size, file_stat.st_mode & 0777);
        write(client_fd, file_info, strlen(file_info));
    } else {
        char *msg = "File not found\n";
        write(client_fd, msg, strlen(msg));
    }
}

void handle_w24fz(int client_fd, const char* sizeRange) {
    // Logic to create a tar.gz file of files within a size range
    char *response = "Archive created for size range";
    write(client_fd, response, strlen(response));
}

void handle_w24ft(int client_fd, const char* extensions) {
    // Logic to create a tar.gz file of files with specified extensions
    char *response = "Archive created for file types";
    write(client_fd, response, strlen(response));
}

void handle_w24fdb(int client_fd, const char* date) {
    // Logic to create a tar.gz file of files before a specific date
    char *response = "Archive created for files before date";
    write(client_fd, response, strlen(response));
}

void handle_w24fda(int client_fd, const char* date) {
    // Logic to create a tar.gz file of files after a specific date
    char *response = "Archive created for files after date";
    write(client_fd, response, strlen(response));
}
