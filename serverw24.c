// serverw24.c

/*
    ✨ ASP SECTION 5
    🚀 Submitted by:
    👨🏻‍💻 Yash Patel - 110128551 && Malhar Raval - 110128144
*/

#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

// Defining constant STATX_BTIME for specific operations if not already defined because statx is a more recent addition and may not be available on older systems. It requires a kernel version of 4.11 or newer.
#if !defined(STATX_BTIME)
#define STATX_BTIME 0x00000800U
#endif

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
#include <ftw.h>
#include <libgen.h>
#include <limits.h>
#include <tar.h>
#include <sys/syscall.h>
#include <arpa/inet.h>

// Define constants for server ports and IP addresses
#define PORT 8080              // serverw24 port
#define MIRROR1_IP "127.0.0.1" // mirror1 IP address
#define MIRROR1_PORT 8081      // mirror1 port
#define MIRROR2_IP "127.0.0.1" // mirror2 IP address
#define MIRROR2_PORT 8082      // mirror2 port

// Define constants for file paths and limits
#define PATH_MAX 4096
#define MAX_EXTENSIONS 3
#define TAR_FILE "/w24project/temp.tar.gz"
#define TAR_FILE_Date "/tmp/filtered_files.tar.gz"
#define TEMP_FILE_LIST "filelist.txt"
#define MAX_DIRS 10000 // Expected number of directories
#define CONNECTION_COUNTER_FILE "connection_count.txt"

// Global variable to hold the threshold date as time_t
static time_t global_threshold_time;

// Function prototypes for handling client requests
void crequest(int client_fd);
void handle_dirlist_a(int client_fd);
void handle_dirlist_t(int client_fd);
void handle_w24fn(int client_fd, const char *filename);
void handle_w24fz(int client_fd, const char *sizeRange);
void handle_w24ft(int client_fd, const char *extensions);
void handle_w24fdb(int client_fd, const char *date);
void handle_w24fda(int client_fd, const char *date);

/*
This function checks if path w24project exists in the home directory with the help of struct stat
if it doesn't exist, it creates the directory with read, write and execute permission for the user
*/
void ensure_directory_exists()
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/w24project", getenv("HOME"));
    struct stat st = {0};

    if (stat(path, &st) == -1)
    {
        mkdir(path, 0700);
    }
}

/*
This function reset_connection_count opens a file specified by CONNECTION_COUNTER_FILE and writes '0' to it, effectively resetting the connection count.
If the file cannot be opened, it outputs an error message.
*/
void reset_connection_count()
{
    FILE *file = fopen(CONNECTION_COUNTER_FILE, "w");
    if (file)
    {
        fprintf(file, "0"); // Reset the counter to zero
        fclose(file);
    }
    else
    {
        perror("Failed to open connection counter file");
    }
}

/*
The function read_connection_count opens a file specified by CONNECTION_COUNTER_FILE in read mode, reads an integer from it into count, and then returns this count.
If the file cannot be opened, it returns 0.
*/
int read_connection_count()
{
    FILE *file = fopen(CONNECTION_COUNTER_FILE, "r");
    int count = 0;
    if (file)
    {
        fscanf(file, "%d", &count);
        fclose(file);
    }
    return count;
}

/*
The function increment_connection_count reads the current connection count from a file, increments it by one, and then writes the updated count back to the file. 
*/
void increment_connection_count()
{
    int count = read_connection_count();
    FILE *file = fopen(CONNECTION_COUNTER_FILE, "w");
    if (file)
    {
        fprintf(file, "%d", count + 1);
        fclose(file);
    }
}

/*
The function determine_server takes an integer count as input and determines which server to use based on this count. 
For counts 1 to 3, it returns 1 (indicating serverw24); 
for counts 4 to 6, it returns 2 (indicating mirror1); 
for counts 7 to 9, it returns 3 (indicating mirror2). 
For counts 10 and above, it rotates among all three servers.
*/
int determine_server(int count)
{
    if (count <= 3)
        return 1; // serverw24
    else if (count <= 6)
        return 2; // mirror1
    else if (count <= 9)
        return 3; // mirror2
    else
        return ((count - 1) % 3) + 1; // Rotate among all three servers
}

/* 
The function redirect_to_mirror establishes a connection to a mirror server specified by ip and port, and then continuously forwards data between the original client and the mirror server. 
If the connection to the mirror server fails, it outputs an error message and returns. If the connection is successful, it creates a child process that reads data from the original client, sends it to the mirror server, reads the response from the mirror server, and sends it back to the original client. 
This process continues indefinitely until the connection to the mirror server is closed.
*/
void redirect_to_mirror(int original_client_fd, const char *ip, int port)
{
    int m_sock_fd = socket(AF_INET, SOCK_STREAM, 0); // Create a new socket for communication with the mirror server
    struct sockaddr_in mirror_addr; // struct for mirror address

    mirror_addr.sin_family = AF_INET;  // Address family for IPv4
    mirror_addr.sin_port = htons(port);  // htons() - converts the port number to network byte order | host to network short/long
    mirror_addr.sin_addr.s_addr = inet_addr(ip); // inet_addr -  converts converts the IP address in dotted-decimal notation (like "127.0.0.1") to the appropriate binary format.

    if (connect(m_sock_fd, (struct sockaddr *)&mirror_addr, sizeof(mirror_addr)) < 0)
    {
        perror("Connect failed");
        return;
    }

    int pid = fork(); // Create a new process to handle communication with the mirror server

    if (pid == 0)
    {
        while (1)
        {
            char buffer[1024];
            int bytes_read = read(original_client_fd, buffer, sizeof(buffer));
            if (bytes_read > 0)
            {
                send(m_sock_fd, buffer, bytes_read, 0); // Send data to the mirror

                // Wait for the response from the mirror and send it back to the original client
                bytes_read = read(m_sock_fd, buffer, sizeof(buffer));
                if (bytes_read > 0)
                {
                    send(original_client_fd, buffer, bytes_read, 0);
                }
            }
        }
        close(m_sock_fd); // Close the connection to the mirror
    }
}

int main()
{
    reset_connection_count(); // First we want to reset the connection count to 0
    ensure_directory_exists(); // Secondly, we want to make sure to create directory to store files returned from server
    int server_fd, client_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0); // socket() - creates a listening socket with IPv4 and TCP connection
    if (server_fd < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    address.sin_family = AF_INET;         // Address family for IPv4
    address.sin_addr.s_addr = INADDR_ANY; // INADDR_ANY - Binds to all available interfaces
    address.sin_port = htons(PORT);       // htons() - converts the port number to network byte order | host to network short/long

    bind(server_fd, (struct sockaddr *)&address, sizeof(address)); // bind() - binds to IP and port address

    listen(server_fd, 100); // server listens for incoming client connections

    while (1)
    {
        client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen); // accept() - accepts first incoming client connections in the queue, if no connection is present, it blocks and waits for a connection

        /*
        ARG1 // server_fd - socket descriptor
        ARG2 // (struct sockaddr *)&address - pointer to the address structure
        ARG3 // (socklen_t*)&addrlen - size of the address structure
        */

        if (client_fd < 0)
        {
            perror("accept");
            continue;
        }

        increment_connection_count();

        // Example server_id check, assuming current server is serverw24 and has ID 1

        int current_connection = read_connection_count();
        int server_to_handle = determine_server(current_connection);

        if (server_to_handle == 1)
        {
            // Handle connection here if serverw24 should handle it
            printf("Server selected for connection %d\n", current_connection);
            fflush(stdout); // Ensure the output is displayed immediately
            pid_t pid = fork();
            if (pid == 0)
            { // Child process
                close(server_fd);
                crequest(client_fd);
                close(client_fd);
                exit(0);
            }
            close(client_fd); // Parent closes the client socket
        }
        else if (server_to_handle == 2)
        {
            // Redirect to mirror1
            printf("Mirror 1 selected for connection %d\n", current_connection);
            fflush(stdout); // Ensure the output is displayed immediately
            redirect_to_mirror(client_fd, MIRROR1_IP, MIRROR1_PORT);

            close(client_fd);
        }
        else if (server_to_handle == 3)
        {
            // Redirect to mirror2
            printf("Mirror 2 selected for connection %d\n", current_connection);
            fflush(stdout); // Ensure the output is displayed immediately
            redirect_to_mirror(client_fd, MIRROR2_IP, MIRROR2_PORT);
            close(client_fd);
        }
    }

    return 0;
}

void crequest(int client_fd)
{
    char buffer[1024];

    while (1)
    {
        memset(buffer, 0, sizeof(buffer));                                // Clear the buffer at the start of each loop
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1); // Leave space for null terminator

        if (bytes_read <= 0)
        {
            // Read error or connection closed by client
            break;
        }

        buffer[bytes_read] = '\0'; // Ensure string is null-terminated
        printf("Command received: %s\n", buffer);

        if (strncmp(buffer, "dirlist -a", 10) == 0)
        {
            handle_dirlist_a(client_fd);
        }
        else if (strncmp(buffer, "dirlist -t", 10) == 0)
        {
            handle_dirlist_t(client_fd);
        }
        else if (strncmp(buffer, "w24fn ", 6) == 0)
        {
            handle_w24fn(client_fd, buffer + 6);
        }
        else if (strncmp(buffer, "w24fz ", 6) == 0)
        {
            handle_w24fz(client_fd, buffer + 6);
        }
        else if (strncmp(buffer, "w24ft ", 6) == 0)
        {
            handle_w24ft(client_fd, buffer + 6);
        }
        else if (strncmp(buffer, "w24fdb ", 7) == 0)
        {
            handle_w24fdb(client_fd, buffer + 7);
        }
        else if (strncmp(buffer, "w24fda ", 7) == 0)
        {
            handle_w24fda(client_fd, buffer + 7);
        }
        else if (strcmp(buffer, "quitc") == 0)
        {
            printf("Client Disconnected");
            break; // Exit the loop and close the connection
        }
    }
    close(client_fd); // Close the client socket at the end of the session
}

//  OPTION 1 ------------------------------------------------------------------//

int compare_strings(const void *a, const void *b)
{
    const char *str1 = *(const char **)a;
    const char *str2 = *(const char **)b;
    return strcasecmp(str1, str2);
}

void list_subdirectories_recursive(const char *base_path, char **dirList, int *count, int max_count)
{
    DIR *d = opendir(base_path);
    if (!d)
    {
        return;
    }

    struct dirent *dir;
    while ((*count < max_count) && (dir = readdir(d)) != NULL)
    {
        if (dir->d_type == DT_DIR && dir->d_name[0] != '.')
        { // Skip hidden directories
            if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0)
            {
                dirList[*count] = strdup(dir->d_name);
                (*count)++;
            }

            char next_path[PATH_MAX];
            snprintf(next_path, PATH_MAX, "%s/%s", base_path, dir->d_name);
            list_subdirectories_recursive(next_path, dirList, count, max_count);
        }
    }
    closedir(d);
}

void handle_dirlist_a(int client_fd)
{
    char *dirList[MAX_DIRS];
    int count = 0;

    char *home_dir = getenv("HOME");
    if (!home_dir)
    {
        home_dir = "/";
    }

    list_subdirectories_recursive(home_dir, dirList, &count, MAX_DIRS);

    qsort(dirList, count, sizeof(char *), compare_strings);

    char bigBuffer[65536] = {0}; // Adjust size as needed
    for (int i = 0; i < count; i++)
    {
        strcat(bigBuffer, dirList[i]);
        strcat(bigBuffer, "\n");
        printf("%s\n", dirList[i]); // Print on server side
        free(dirList[i]);           // Free after copying to buffer
    }

    if (count == 0)
    {
        strcpy(bigBuffer, "No subdirectories found.\n");
    }

    write(client_fd, bigBuffer, strlen(bigBuffer));
}

// OPTION 2 ------------------------------------------------------------------//

typedef struct
{
    char *full_path;
    char *dir_name;
    struct timespec btime; // Adjusted for timespec
} DirEntry;

int dir_time_compare(const void *a, const void *b)
{
    const DirEntry *dir1 = (const DirEntry *)a;
    const DirEntry *dir2 = (const DirEntry *)b;
    if (dir1->btime.tv_sec < dir2->btime.tv_sec)
        return -1;
    if (dir1->btime.tv_sec > dir2->btime.tv_sec)
        return 1;
    if (dir1->btime.tv_nsec < dir2->btime.tv_nsec)
        return -1;
    if (dir1->btime.tv_nsec > dir2->btime.tv_nsec)
        return 1;
    return 0;
}

void list_subdirectories_recursive_t(const char *base_path, DirEntry dirList[], int *count, int max_count)
{
    DIR *d = opendir(base_path);
    if (!d)
    {
        return;
    }

    struct dirent *dir;
    struct statx statbuf;
    char path[1024];
    while ((*count < max_count) && (dir = readdir(d)) != NULL)
    {
        if (dir->d_type == DT_DIR && dir->d_name[0] != '.')
        {
            if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0)
            {
                snprintf(path, sizeof(path), "%s/%s", base_path, dir->d_name);
                if (syscall(SYS_statx, AT_FDCWD, path, AT_SYMLINK_NOFOLLOW, STATX_BTIME, &statbuf) == 0)
                {
                    dirList[*count].full_path = strdup(path);
                    dirList[*count].dir_name = strdup(dir->d_name);
                    // Assigning the fields individually
                    dirList[*count].btime.tv_sec = statbuf.stx_btime.tv_sec;
                    dirList[*count].btime.tv_nsec = statbuf.stx_btime.tv_nsec;
                    (*count)++;
                }

                list_subdirectories_recursive_t(path, dirList, count, max_count);
            }
        }
    }
    closedir(d);
}

void handle_dirlist_t(int client_fd)
{
    DirEntry dirList[MAX_DIRS]; // Assuming MAX_DIRS is defined and large enough
    int count = 0;

    char *home_dir = getenv("HOME");
    if (!home_dir)
    {
        home_dir = "/";
    }

    list_subdirectories_recursive_t(home_dir, dirList, &count, MAX_DIRS);
    qsort(dirList, count, sizeof(DirEntry), dir_time_compare);

    char response[65536] = ""; // Ensure the buffer is large enough
    for (int i = 0; i < count; i++)
    {
        strcat(response, dirList[i].dir_name);
        strcat(response, "\n");
        printf("%s\n", dirList[i].dir_name); // Print only directory name on the server side
        free(dirList[i].full_path);
        free(dirList[i].dir_name);
    }

    write(client_fd, response, strlen(response));

    if (count == 0)
    {
        const char *msg = "No subdirectories found.\n";
        write(client_fd, msg, strlen(msg));
        printf("%s", msg); // Also print on the server side
    }
}

//  OPTION 3 ------------------------------------------------------------------//

struct file_info
{
    char path[PATH_MAX];
    off_t size;
    mode_t mode;
    struct timespec btime;
};

static struct file_info found_file;
static char target_filename[256];

static int find_file_in_directory(const char *dir_path)
{
    DIR *dir = opendir(dir_path);
    if (dir == NULL)
    {
        return 0;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_REG && strcmp(target_filename, entry->d_name) == 0)
        {
            // Construct the full path for the file
            snprintf(found_file.path, PATH_MAX, "%s/%s", dir_path, entry->d_name);

            struct statx file_stat;
            if (syscall(SYS_statx, AT_FDCWD, found_file.path, AT_SYMLINK_NOFOLLOW, STATX_ALL, &file_stat) == 0)
            {
                found_file.size = file_stat.stx_size;
                found_file.mode = file_stat.stx_mode;
                found_file.btime.tv_sec = file_stat.stx_btime.tv_sec;
                found_file.btime.tv_nsec = file_stat.stx_btime.tv_nsec;
                closedir(dir);
                return 1; // File found
            }
        }
    }

    // Only search in subdirectories if the file wasn't found in the current directory
    rewinddir(dir); // Reset directory stream to the beginning
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
        {
            char next_dir_path[PATH_MAX];
            snprintf(next_dir_path, PATH_MAX, "%s/%s", dir_path, entry->d_name);

            if (find_file_in_directory(next_dir_path))
            {
                closedir(dir);
                return 1; // File found in sub-directory
            }
        }
    }

    closedir(dir);
    return 0; // File not found
}

void handle_w24fn(int client_fd, const char *filename)
{
    strcpy(target_filename, filename);
    char *home_dir = getenv("HOME");
    if (!home_dir)
    {
        home_dir = "/";
    }

    if (find_file_in_directory(home_dir))
    {
        char response[1024];
        char time_str[256];

        // Convert the birth time to human-readable format
        struct tm *tm_info = localtime(&found_file.btime.tv_sec);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

        snprintf(response, sizeof(response), "File: %s, Size: %ld bytes, Created: %s, Permissions: %o\n",
                 found_file.path, found_file.size, time_str, found_file.mode & 0777); // Mask mode to display standard Unix permissions

        write(client_fd, response, strlen(response));
    }
    else
    {
        const char *msg = "File not found\n";
        write(client_fd, msg, strlen(msg));
    }
}

//  OPTION 4  ------------------------------------------------------------------//

struct size_filter
{
    off_t min_size;
    off_t max_size;
};

static struct size_filter global_size_filter;

static int check_file_size(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    // Skip hidden directories and hidden files
    if ((typeflag == FTW_D && fpath[ftwbuf->base] == '.') ||
        (typeflag == FTW_F && fpath[ftwbuf->base] == '.'))
    {
        return FTW_CONTINUE; // Skip hidden directories and files
    }

    if (typeflag == FTW_F && sb->st_size >= global_size_filter.min_size && sb->st_size <= global_size_filter.max_size)
    {
        FILE *fp = fopen(TEMP_FILE_LIST, "a");
        if (fp)
        {
            fprintf(fp, "%s\n", fpath);
            fclose(fp);
        }
    }
    return 0; // Continue traversing
}

void handle_w24fz(int client_fd, const char *sizeRange)
{
    long size1, size2;
    sscanf(sizeRange, "%ld %ld", &size1, &size2);

    // Validate the size range
    // if (size1 > size2 || size1 < 0 || size2 < 0)
    // {
    //     char *msg = "Invalid size range.\n";
    //     write(client_fd, msg, strlen(msg));
    //     return;
    // }

    global_size_filter.min_size = size1;
    global_size_filter.max_size = size2;

    // Clear the temporary file list before starting
    fclose(fopen(TEMP_FILE_LIST, "w"));

    // Use nftw to walk through the file system from the home directory
    nftw(getenv("HOME"), check_file_size, 20, FTW_PHYS);

    // Check if there are any files found
    struct stat list_stat;
    if (stat(TEMP_FILE_LIST, &list_stat) == 0 && list_stat.st_size > 0)
    {
        char tarFilePath[1024];
        snprintf(tarFilePath, sizeof(tarFilePath), "%s/w24project/temp.tar.gz", getenv("HOME"));
        char tarCommand[1024];
        snprintf(tarCommand, sizeof(tarCommand), "tar -cvzf %s -T %s --transform='s|.*/||' 2> /dev/null", tarFilePath, TEMP_FILE_LIST);

        int result = system(tarCommand);

        // Check if the tar file was created successfully and has content
        struct stat tarStat;
        if (result == 0 && stat(tarFilePath, &tarStat) == 0 && tarStat.st_size > 0)
        {
            char *msg = "Tar file created and sent.\n";
            write(client_fd, msg, strlen(msg));
            // Logic to send the tar file should be here
        }
        else
        {
            char *msg = "Success\n";
            write(client_fd, msg, strlen(msg));
        }
    }
    else
    {
        char *msg = "No files found within the specified size range.\n";
        write(client_fd, msg, strlen(msg));
    }

    // Cleanup the temporary file list after use
    unlink(TEMP_FILE_LIST);
}

//  OPTION 5 ------------------------------------------------------------------//

static char *extensions[MAX_EXTENSIONS];
static int ext_count = 0;

static int check_file_extension(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    if (typeflag == FTW_F && fpath[ftwbuf->base] != '.')
    {
        const char *ext = strrchr(fpath, '.');
        if (ext)
        {
            ext++; // Move past the dot
            for (int i = 0; i < ext_count; i++)
            {
                if (strcmp(ext, extensions[i]) == 0)
                {
                    FILE *fp = fopen(TEMP_FILE_LIST, "a");
                    if (fp)
                    {
                        fprintf(fp, "%s\n", fpath);
                        fclose(fp);
                    }
                    break;
                }
            }
        }
    }
    return 0; // Continue traversing
}

void handle_w24ft(int client_fd, const char *extensionList)
{
    // Reset extension count for each call
    ext_count = 0;

    // Parse the extension list and count the extensions
    char *token = strtok((char *)extensionList, " ");
    while (token)
    {
        if (ext_count < MAX_EXTENSIONS)
        {
            extensions[ext_count++] = token;
            token = strtok(NULL, " ");
        }
        else
        {
            write(client_fd, "Too many file types specified. Max 3 allowed.\n", 45);
            return;
        }
    }

    if (ext_count == 0)
    {
        write(client_fd, "No file types specified.\n", 26);
        return;
    }

    // Clear the temporary file list
    fclose(fopen(TEMP_FILE_LIST, "w"));

    nftw(getenv("HOME"), check_file_extension, 20, FTW_PHYS);

    struct stat statbuf;
    if (stat(TEMP_FILE_LIST, &statbuf) == 0 && statbuf.st_size > 0)
    {
        char tarFilePath[1024];
        snprintf(tarFilePath, sizeof(tarFilePath), "%s/w24project/temp.tar.gz", getenv("HOME"));
        char tarCommand[1024];
        snprintf(tarCommand, sizeof(tarCommand), "tar -czf %s -T %s --transform='s|.*/||' 2> /dev/null", tarFilePath, TEMP_FILE_LIST);

        if (system(tarCommand) == 0)
        {
            char *msg = "Tar file created.\n";
            write(client_fd, msg, strlen(msg));
            // Code to send the file to the client goes here
        }
        else
        {
            char *msg = "Success\n";
            write(client_fd, msg, strlen(msg));
        }
    }
    else
    {
        write(client_fd, "No file found matching specified types.\n", 41);
    }

    // Cleanup the temporary file list after use
    unlink(TEMP_FILE_LIST);
}

//  OPTION 6 ------------------------------------------------------------------//

// Utility function to convert date string to time_t
time_t parse_date_db(const char *date_str)
{
    struct tm tm = {0};

    // Validate the date format strictly as YYYY-MM-DD
    if (strlen(date_str) != 10)
    {
        return -1; // Length must be exactly 10 characters for valid YYYY-MM-DD format
    }

    // Check for correct delimiter positions
    if (date_str[4] != '-' || date_str[7] != '-')
    {
        return -1; // Delimiters must be at the correct positions
    }

    // Parse the date string
    if (strptime(date_str, "%Y-%m-%d", &tm) == NULL)
    {
        return -1; // Parsing error
    }

    // Validate ranges (optional, strptime does some checking but more could be done if necessary)
    if (tm.tm_year < 0 || tm.tm_mon < 0 || tm.tm_mday < 0 || tm.tm_mon > 11 || tm.tm_mday > 31)
    {
        return -1;
    }

    // Return time in seconds since the epoch
    return mktime(&tm);
}

// File check callback for nftw
static int check_file_date_db(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    // Only include regular files and check against the global threshold time
    if (typeflag == FTW_F && sb->st_ctime <= global_threshold_time)
    {
        FILE *fp = fopen(TEMP_FILE_LIST, "a");
        if (fp)
        {
            fprintf(fp, "%s\n", fpath);
            fclose(fp);
        }
    }
    return 0; // Continue traversal
}

// Global variable to hold the threshold date as time_t
static time_t global_threshold_time;

void handle_w24fdb(int client_fd, const char *dateStr)
{
    global_threshold_time = parse_date_db(dateStr);
    if (global_threshold_time == -1)
    {
        const char *msg = "Invalid date format. Accepted format is (YYYY-MM-DD)\n";
        write(client_fd, msg, strlen(msg));
        return;
    }

    // Clear the temporary file list
    fclose(fopen(TEMP_FILE_LIST, "w"));

    // Walk through the file system from the home directory
    nftw(getenv("HOME"), check_file_date_db, 20, FTW_PHYS);

    // Prepare to create a tar.gz file from the list of found files
    char tarFilePath[1024];
    snprintf(tarFilePath, sizeof(tarFilePath), "%s/w24project/temp.tar.gz", getenv("HOME"));
    char tarCommand[1024];
    snprintf(tarCommand, sizeof(tarCommand), "tar -czf %s -T %s --transform='s|.*/||' 2> /dev/null", tarFilePath, TEMP_FILE_LIST);
    if (system(tarCommand) == 0)
    {
        struct stat tarStat;
        if (stat(tarFilePath, &tarStat) == 0 && tarStat.st_size > 0)
        {
            char *msg = "Tar file created and sent.\n";
            write(client_fd, msg, strlen(msg));
            // Here should be the logic to actually send the file to the client
        }
        else
        {
            char *msg = "No files found with specified date.\n";
            write(client_fd, msg, strlen(msg));
        }
    }
    else
    {
        char *msg = "Failed\n";
        write(client_fd, msg, strlen(msg));
    }

    // Cleanup
    unlink(TEMP_FILE_LIST);
}

// ------------------------------------------------------------------//

// Utility function to convert date string to time_t
time_t parse_date_da(const char *date_str)
{
    struct tm tm = {0};

    // Validate the date format strictly as YYYY-MM-DD
    if (strlen(date_str) != 10)
    {
        return -1; // Length must be exactly 10 characters for valid YYYY-MM-DD format
    }

    // Check for correct delimiter positions
    if (date_str[4] != '-' || date_str[7] != '-')
    {
        return -1; // Delimiters must be at the correct positions
    }

    // Parse the date string
    if (strptime(date_str, "%Y-%m-%d", &tm) == NULL)
    {
        return -1; // Parsing error
    }

    // Validate ranges (optional, strptime does some checking but more could be done if necessary)
    if (tm.tm_year < 0 || tm.tm_mon < 0 || tm.tm_mday < 0 || tm.tm_mon > 11 || tm.tm_mday > 31)
    {
        return -1;
    }

    // Return time in seconds since the epoch
    return mktime(&tm);
}

// File check callback for nftw
static int check_file_date_da(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    // Only include regular files and check against the global threshold time
    if (typeflag == FTW_F && sb->st_ctime >= global_threshold_time)
    {
        FILE *fp = fopen(TEMP_FILE_LIST, "a");
        if (fp)
        {
            fprintf(fp, "%s\n", fpath);
            fclose(fp);
        }
    }
    return 0; // Continue traversal
}

// Global variable to hold the threshold date as time_t
static time_t global_threshold_time;

void handle_w24fda(int client_fd, const char *dateStr)
{
    global_threshold_time = parse_date_da(dateStr);
    if (global_threshold_time == -1)
    {
        const char *msg = "Invalid date format. Accepted format is (YYYY-MM-DD)\n";
        write(client_fd, msg, strlen(msg));
        return;
    }

    // Clear the temporary file list
    fclose(fopen(TEMP_FILE_LIST, "w"));

    // Walk through the file system from the home directory
    nftw(getenv("HOME"), check_file_date_da, 20, FTW_PHYS);

    // Prepare to create a tar.gz file from the list of found files
    char tarFilePath[1024];
    snprintf(tarFilePath, sizeof(tarFilePath), "%s/w24project/temp.tar.gz", getenv("HOME"));
    char tarCommand[1024];
    snprintf(tarCommand, sizeof(tarCommand), "tar -czf %s -T %s --transform='s|.*/||' 2> /dev/null", tarFilePath, TEMP_FILE_LIST);
    if (system(tarCommand) == 0)
    {
        struct stat tarStat;
        if (stat(tarFilePath, &tarStat) == 0 && tarStat.st_size > 0)
        {
            char *msg = "Tar file created and sent.\n";
            write(client_fd, msg, strlen(msg));
            // Here should be the logic to actually send the file to the client
        }
        else
        {
            char *msg = "No files found with specified date.\n";
            write(client_fd, msg, strlen(msg));
        }
    }
    else
    {
        char *msg = "Success\n";
        write(client_fd, msg, strlen(msg));
    }

    // Cleanup
    unlink(TEMP_FILE_LIST);
}
