// mirror2.c

/*
    ✨ ASP SECTION 5
    🚀 Submitted by:
    👨🏻‍💻 Yash Patel - 110128551 && Malhar Raval - 110128144
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/types.h>
#include <time.h>
#include <ftw.h>
#include <limits.h>
#include <tar.h>
#include <sys/syscall.h>

#if !defined(STATX_BTIME)
#define STATX_BTIME 0x00000800U
#endif

// Define constants for port, file paths and limits
#define PORT 8082
#define PATH_MAX 4096
#define MAX_DIRS 10000
#define TEMP_FILE_LIST "filelist.txt"
#define MAX_EXTENSIONS 3 // We need maximum of 3 extensions
static time_t global_threshold_time;

// Function declartions for handling client requests
void crequest(int client_fd);
void handle_dirlist_a(int client_fd);
void handle_dirlist_t(int client_fd);
void handle_w24fn(int client_fd, const char *filename);
void handle_w24fz(int client_fd, const char *sizeRange);
void handle_w24ft(int client_fd, const char *extensions);
void handle_w24fdb(int client_fd, const char *date);
void handle_w24fda(int client_fd, const char *date);

int main()
{
    int server_fd, client_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0); // socket() - creates a listening socket with IPv4 and TCP connection
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    address.sin_family = AF_INET;         // Address family for IPv4
    address.sin_addr.s_addr = INADDR_ANY; // INADDR_ANY - Binds to all available interfaces
    address.sin_port = htons(PORT);       // htons() - converts the port number to network byte order | host to network short/long

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) // bind() - binds to IP and port address
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) // mirror2 listens for incoming client connections
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen); // accept() - accepts first incoming client connections in the queue, if no connection is present, it blocks and waits for a connection
        if (client_fd < 0)
        {
            perror("accept");
            continue;
        }

        if (fork() == 0)
        {                     // Child process
            close(server_fd); // Close the listening socket in the child process
            crequest(client_fd);
            close(client_fd);
            exit(0);
        }
        close(client_fd); // Parent process closes the client socket
    }

    return 0;
}

/*
The crequest function continuously reads commands from a client, identified by client_fd, and calls the appropriate handler function based on the command received.
If a read error occurs, the client disconnects, or the "quitc" command is received, the function stops reading and closes the connection to the client.
*/
void crequest(int client_fd)
{
    char buffer[1024];

    while (1)
    {
        // Clear the buffer at the start of each loop
        memset(buffer, 0, sizeof(buffer));

        // Read data from the client socket into the buffer, leaving space for the null terminator
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
        if (bytes_read <= 0)
        {
            break; // Break the loop if there's a read error or the client closes the connection
        }

        buffer[bytes_read] = '\0'; // Ensure string is null-terminated
        printf("🟠 Command received: %s\n", buffer);

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

//  COMMAND 1 ------------------------------------------------------------------

/*
The compare_strings function is a comparator function used for sorting strings in a case-insensitive manner.
*/
int compare_strings(const void *a, const void *b)
{
    const char *str1 = *(const char **)a;
    const char *str2 = *(const char **)b;
    return strcasecmp(str1, str2); // Compare strings case-insensitively
}

/*
 The list_subdirectories_recursive function recursively traverses directories starting from base_path, adding the names of non-hidden subdirectories to dirList until max_count is reached.
*/
void list_subdirectories_recursive(const char *base_path, char **dirList, int *count, int max_count)
{
    DIR *d = opendir(base_path); // Open the directory specified by base_path
    if (!d)
    {
        return; // Return if directory cannot be opened
    }

    struct dirent *dir;
    while ((*count < max_count) && (dir = readdir(d)) != NULL)
    {
        if (dir->d_type == DT_DIR && dir->d_name[0] != '.') // Check if it's a directory and not hidden
        {                                                   // Skip hidden directories
            if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0)
            {
                dirList[*count] = strdup(dir->d_name); // Copy directory name to dirList array
                (*count)++;                            // Increment the count of directories
            }

            char next_path[PATH_MAX];
            snprintf(next_path, PATH_MAX, "%s/%s", base_path, dir->d_name);      // Create the next directory path
            list_subdirectories_recursive(next_path, dirList, count, max_count); // Recursively list subdirectories
        }
    }
    closedir(d);
}

/*
The handle_dirlist_a function retrieves a list of subdirectories from the home directory, sorts them, and sends the list to the client.
*/
void handle_dirlist_a(int client_fd)
{
    char *dirList[MAX_DIRS]; // Array to store directory names
    int count = 0;

    char *home_dir = getenv("HOME"); // Get the user's home directory
    if (!home_dir)
    {
        home_dir = "/";
    }

    list_subdirectories_recursive(home_dir, dirList, &count, MAX_DIRS); // List subdirectories recursively

    qsort(dirList, count, sizeof(char *), compare_strings); // Sort the directory list alphabetically

    char bigBuffer[65536] = {0}; // Buffer to store directory names and messages
    for (int i = 0; i < count; i++)
    {
        strcat(bigBuffer, dirList[i]);
        strcat(bigBuffer, "\n");
        printf("%s\n", dirList[i]); // Print directory name on the mirror2 side
        free(dirList[i]);           // Free memory allocated for directory namer
    }

    if (count == 0)
    {
        strcpy(bigBuffer, "No subdirectories found.\n"); // Message for no subdirectories
    }

    write(client_fd, bigBuffer, strlen(bigBuffer));
}

// COMMAND 2 ------------------------------------------------------------------

/*
The DirEntry struct holds information about a directory, including its full path, name, and creation time.
*/
typedef struct
{
    char *full_path;       // Full path of the directory entry
    char *dir_name;        // Name of the directory
    struct timespec btime; // Time when the directory was created (in timespec format)
} DirEntry;

/*
The dir_time_compare function is a comparator used for sorting DirEntry objects based on their creation time.
*/
int dir_time_compare(const void *a, const void *b)
{
    const DirEntry *dir1 = (const DirEntry *)a;
    const DirEntry *dir2 = (const DirEntry *)b;

    // Compare the creation times of the directories
    if (dir1->btime.tv_sec < dir2->btime.tv_sec)
        return -1;
    if (dir1->btime.tv_sec > dir2->btime.tv_sec)
        return 1;
    if (dir1->btime.tv_nsec < dir2->btime.tv_nsec)
        return -1;
    if (dir1->btime.tv_nsec > dir2->btime.tv_nsec)
        return 1;
    return 0; // Return 0 if both directories have the same creation time
}

/*
The list_subdirectories_recursive_t function recursively traverses directories starting from base_path, adding information about subdirectories to dirList until max_count is reached.
*/
void list_subdirectories_recursive_t(const char *base_path, DirEntry dirList[], int *count, int max_count)
{
    DIR *d = opendir(base_path); // Open the directory specified by base_path
    if (!d)
    {
        return; // Return if directory cannot be opened
    }

    struct dirent *dir;
    struct statx statbuf;
    char path[1024];
    while ((*count < max_count) && (dir = readdir(d)) != NULL)
    {
        if (dir->d_type == DT_DIR && dir->d_name[0] != '.') // Check if it's a directory and not hidden
        {
            if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0)
            {
                snprintf(path, sizeof(path), "%s/%s", base_path, dir->d_name); // Create the next directory path

                // Get the creation time of the directory
                if (syscall(SYS_statx, AT_FDCWD, path, AT_SYMLINK_NOFOLLOW, STATX_BTIME, &statbuf) == 0)
                {
                    dirList[*count].full_path = strdup(path);       // Copy the full path to the DirEntry structure
                    dirList[*count].dir_name = strdup(dir->d_name); // Copy the directory name to the DirEntry structure

                    // Assign the creation time to the DirEntry structure
                    dirList[*count].btime.tv_sec = statbuf.stx_btime.tv_sec;
                    dirList[*count].btime.tv_nsec = statbuf.stx_btime.tv_nsec;
                    (*count)++; // Increment the count of directories
                }

                list_subdirectories_recursive_t(path, dirList, count, max_count); // Recursively list subdirectories
            }
        }
    }
    closedir(d);
}

/*
The handle_dirlist_t function retrieves a list of subdirectories from the home directory, sorts them by creation time, and sends the list to the client.
*/
void handle_dirlist_t(int client_fd)
{
    DirEntry dirList[MAX_DIRS]; // Array to store directory entries
    int count = 0;

    char *home_dir = getenv("HOME");
    if (!home_dir)
    {
        home_dir = "/";
    }

    list_subdirectories_recursive_t(home_dir, dirList, &count, MAX_DIRS); // List subdirectories recursively
    qsort(dirList, count, sizeof(DirEntry), dir_time_compare);            // Sort the directory entries based on creation time

    char response[65536] = ""; // Buffer to store the response
    for (int i = 0; i < count; i++)
    {
        strcat(response, dirList[i].dir_name); // Concatenate directory name to response
        strcat(response, "\n");                // Add newline after each directory name
        printf("%s\n", dirList[i].dir_name);   // Print directory name on the mirror2 side
        free(dirList[i].full_path);            // Free memory allocated for full path
        free(dirList[i].dir_name);             // Free memory allocated for directory name
    }

    write(client_fd, response, strlen(response)); // Send directory list to client

    if (count == 0)
    {
        const char *msg = "No subdirectories found.\n"; // Message for no subdirectories
        write(client_fd, msg, strlen(msg));             // Send message to client
        printf("%s", msg);                              // Also print on the mirror2 side
    }
}

//  COMMAND 3 ------------------------------------------------------------------

/*
The file_info struct holds information about a file, including its path, size, mode (permissions), and creation time.
*/
struct file_info
{
    char path[PATH_MAX];
    off_t size;
    mode_t mode;
    struct timespec btime;
};

static struct file_info found_file;
static char target_filename[256];

/*
The find_file_in_directory function searches for a file with a name matching target_filename in the directory specified by dir_path and its subdirectories. If the file is found, its information is stored in found_file and the function returns 1. If the file is not found, the function returns 0.
*/
static int find_file_in_directory(const char *dir_path)
{
    // Open the directory
    DIR *dir = opendir(dir_path);
    if (dir == NULL)
    {
        return 0;
    }

    // Iterate over entries in the directory
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_REG && strcmp(target_filename, entry->d_name) == 0)
        {
            // Construct the full path for the file
            snprintf(found_file.path, PATH_MAX, "%s/%s", dir_path, entry->d_name);

            // Get file information
            struct statx file_stat; // Structure to store file information
            // Use syscall to get file information using statx syscall
            if (syscall(SYS_statx, AT_FDCWD, found_file.path, AT_SYMLINK_NOFOLLOW, STATX_ALL, &file_stat) == 0)
            {
                // If file information retrieval is successful, update found_file structure
                found_file.size = file_stat.stx_size;                   // Update file size
                found_file.mode = file_stat.stx_mode;                   // Update file mode (permissions)
                found_file.btime.tv_sec = file_stat.stx_btime.tv_sec;   // Update file birth time (seconds)
                found_file.btime.tv_nsec = file_stat.stx_btime.tv_nsec; // Update file birth time (nanoseconds)
                closedir(dir);                                          // Close the directory stream
                return 1;                                               // File found
            }
        }
    }

    // Search in subdirectories if the file wasn't found in the current directory
    rewinddir(dir); // Reset directory stream to the beginning
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
        {
            // Recursively search in subdirectory
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

/*
The handle_w24fn function is a handler for a client request to find a file.
It copies the target filename from the request, gets the home directory path, and calls find_file_in_directory to search for the file.
If the file is found, it sends a response to the client with the file's details. If the file is not found, it sends an error message to the client.
*/
void handle_w24fn(int client_fd, const char *filename)
{
    // Copy the target filename
    strcpy(target_filename, filename);

    // Get the home directory path
    char *home_dir = getenv("HOME");
    if (!home_dir)
    {
        home_dir = "/";
    }

    // Find the file in the home directory
    if (find_file_in_directory(home_dir))
    {
        char response[1024];
        char time_str[256];

        // Convert the birth time to human-readable format
        struct tm *tm_info = localtime(&found_file.btime.tv_sec);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

        // Create a response with file details
        snprintf(response, sizeof(response), "File: %s, Size: %ld bytes, Created: %s, Permissions: %o\n",
                 found_file.path, found_file.size, time_str, found_file.mode & 0777); // Mask mode to display standard Unix permissions

        write(client_fd, response, strlen(response));
    }
    else
    {
        // If file is not found, send an error message to the client
        const char *msg = "File not found\n";
        write(client_fd, msg, strlen(msg));
    }
}

//  COMMAND 4  ------------------------------------------------------------------

/*
The size_filter struct holds a range of file sizes, including a minimum and maximum size.
*/
struct size_filter
{
    off_t min_size;
    off_t max_size;
};

static struct size_filter global_size_filter;

// Function to check file sizes during file system traversal
static int check_file_size(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    // Skip hidden directories and hidden files
    if ((typeflag == FTW_D && fpath[ftwbuf->base] == '.') ||
        (typeflag == FTW_F && fpath[ftwbuf->base] == '.'))
    {
        return FTW_CONTINUE; // Skip hidden directories and files
    }

    // Check if the file is a regular file and its size is within the specified range
    if (typeflag == FTW_F && sb->st_size >= global_size_filter.min_size && sb->st_size <= global_size_filter.max_size)
    {
        // Append the file path to the temporary file list
        FILE *fp = fopen(TEMP_FILE_LIST, "a");
        if (fp)
        {
            fprintf(fp, "%s\n", fpath);
            fclose(fp);
        }
    }
    return 0; // Continue traversing
}

/*
The handle_w24fz function is a handler for a client request to find files within a certain size range.
It parses the size range from the request, updates global_size_filter with the size range, and then uses nftw to traverse the file system from the home directory, checking each file's size with check_file_size.
If any files are found within the size range, it creates a tar.gz file from the list of found files and sends a response to the client.
*/
void handle_w24fz(int client_fd, const char *sizeRange)
{
    // Variables to store the size range
    long size1, size2;
    sscanf(sizeRange, "%ld %ld", &size1, &size2);

    // Updating the global size filter with the specified size range
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
        // Prepare to create a tar.gz file from the list of found files
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
        }
        else
        {
            char *msg = "Error creating tar file.\n"; // If there is an error in creating the tar.gz file, send an error message to the client

            write(client_fd, msg, strlen(msg));
        }
    }
    else
    {
        char *msg = "No files found within the specified size range.\n";
        write(client_fd, msg, strlen(msg));
    }

    unlink(TEMP_FILE_LIST); // Cleanup the temporary file list after use
}

//  COMMAND 5 ------------------------------------------------------------------//

static char *extensions[MAX_EXTENSIONS];
static int ext_count = 0;

// Function to check file extensions during file system traversal
static int check_file_extension(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    // Check if the file is a regular file and the first character of the base name is not a dot
    if (typeflag == FTW_F && fpath[ftwbuf->base] != '.')
    {
        const char *ext = strrchr(fpath, '.');
        if (ext)
        {
            ext++; // Move past the dot
            // Compare the extension with the list of specified extensions
            for (int i = 0; i < ext_count; i++)
            {
                if (strcmp(ext, extensions[i]) == 0)
                {
                    // If the extension matches, append the file path to the temporary file list
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

/*
The handle_w24ft function is a handler for a client request to find files with certain extensions.
It clears the temporary file list, then uses nftw to traverse the file system from the home directory, checking each file's extension with check_file_extension.
If any files are found with matching extensions, it creates a tar.gz file from the list of found files and sends a response to the client.
*/
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

    fclose(fopen(TEMP_FILE_LIST, "w")); // Clear the temporary file list

    nftw(getenv("HOME"), check_file_extension, 20, FTW_PHYS); // Walk through the file system from the home directory

    // Check if the temporary file list is not empty
    struct stat statbuf;
    if (stat(TEMP_FILE_LIST, &statbuf) == 0 && statbuf.st_size > 0)
    {
        // Prepare to create a tar.gz file from the list of found files
        char tarFilePath[1024];
        snprintf(tarFilePath, sizeof(tarFilePath), "%s/w24project/temp.tar.gz", getenv("HOME"));
        char tarCommand[1024];
        snprintf(tarCommand, sizeof(tarCommand), "tar -czf %s -T %s --transform='s|.*/||' 2> /dev/null", tarFilePath, TEMP_FILE_LIST);

        // Create the tar.gz file
        if (system(tarCommand) == 0)
        {
            char *msg = "Tar file created.\n";
            write(client_fd, msg, strlen(msg));
        }
        else
        {
            char *msg = "Error creating tar file.\n";
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

//  COMMAND 6 ------------------------------------------------------------------//

/*
Utility function to convert date string to time_t
The parse_date function converts a date string to a time_t value. It validates the date format as YYYY-MM-DD, checks for correct delimiter positions, parses the date string, validates the ranges of year, month, and day, and returns the time in seconds since the epoch.
*/
time_t parse_date(const char *date_str)
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
    // Including regular files and check against the global threshold time
    if (typeflag == FTW_F && sb->st_ctime <= global_threshold_time)
    {
        FILE *fp = fopen(TEMP_FILE_LIST, "a");
        if (fp)
        {
            // Writing the file path to the temporary file list
            fprintf(fp, "%s\n", fpath);
            fclose(fp);
        }
    }
    return 0; // Continue traversal
}

static time_t global_threshold_time; // Global variable to hold the threshold date as time_t

/*
The handle_w24fdb function is a handler for a client request to find files created before a certain date.
*/
void handle_w24fdb(int client_fd, const char *dateStr)
{
    global_threshold_time = parse_date(dateStr); // Parse the date string to get the threshold time
    if (global_threshold_time == -1)
    {
        // If the date format is invalid, send an error message to the client
        const char *msg = "Invalid date format. Accepted format is (YYYY-MM-DD)\n";
        write(client_fd, msg, strlen(msg));
        return;
    }

    fclose(fopen(TEMP_FILE_LIST, "w")); // Clear the temporary file list

    nftw(getenv("HOME"), check_file_date_db, 20, FTW_PHYS); // Walk through the file system from the home directory

    // Prepare to create a tar.gz file from the list of found files
    char tarFilePath[1024];
    snprintf(tarFilePath, sizeof(tarFilePath), "%s/w24project/temp.tar.gz", getenv("HOME"));
    char tarCommand[1024];
    snprintf(tarCommand, sizeof(tarCommand), "tar -czf %s -T %s --transform='s|.*/||' 2> /dev/null", tarFilePath, TEMP_FILE_LIST);

    if (system(tarCommand) == 0)
    {
        struct stat tarStat; // struct to store the tar file stats

        // If the tar.gz file is created successfully
        if (stat(tarFilePath, &tarStat) == 0 && tarStat.st_size > 0)
        {
            char *msg = "Tar file created and sent.\n";
            write(client_fd, msg, strlen(msg));
        }
        else
        {
            char *msg = "No files found with specified date.\n";
            write(client_fd, msg, strlen(msg));
        }
    }
    else
    {
        // If there is an error in creating the tar.gz file, send an error message to the client
        char *msg = "Error creating tar file.\n";
        write(client_fd, msg, strlen(msg));
    }

    unlink(TEMP_FILE_LIST); // Cleanup to reomve temp tar file
}

//  COMMAND 7 ------------------------------------------------------------------//

// File check callback for nftw
static int check_file_date_da(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    // Including regular files and check against the global threshold time
    if (typeflag == FTW_F && sb->st_ctime >= global_threshold_time)
    {
        FILE *fp = fopen(TEMP_FILE_LIST, "a");
        if (fp)
        {
            // Writing the file path to the temporary file list
            fprintf(fp, "%s\n", fpath);
            fclose(fp);
        }
    }
    return 0; // Continue traversal
}

// Global variable to hold the threshold date as time_t
static time_t global_threshold_time;

/*
The handle_w24fda function is a handler for a client request to find files created after a certain date.
*/
void handle_w24fda(int client_fd, const char *dateStr)
{
    global_threshold_time = parse_date(dateStr); // Parse the date string to get the threshold time
    if (global_threshold_time == -1)
    {
        // If the date format is invalid, send an error message to the client
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
        struct stat tarStat; // struct to store the tar file stats

        // If the tar.gz file is created successfully
        if (stat(tarFilePath, &tarStat) == 0 && tarStat.st_size > 0)
        {
            char *msg = "Tar file created and sent.\n";
            write(client_fd, msg, strlen(msg));
        }
        else
        {
            char *msg = "No files found with specified date.\n";
            write(client_fd, msg, strlen(msg));
        }
    }
    else
    {
        // If there is an error in creating the tar.gz file, send an error message to the client
        char *msg = "Error creating tar file.\n";
        write(client_fd, msg, strlen(msg));
    }

    // Cleanup to reomve temp tar file
    unlink(TEMP_FILE_LIST);
}
