// mirror2.c
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
#include <sys/syscall.h> // Include for SYS_statx

#if !defined(STATX_BTIME)
#define STATX_BTIME 0x00000800U
#endif

#define PORT 8082
#define PATH_MAX 4096
#define MAX_DIRS 10000
#define TEMP_FILE_LIST "filelist.txt"
#define MAX_EXTENSIONS 3 // Define maximum number of file extensions
static time_t global_threshold_time; // Declare globally

// Include global variables and all function declarations
// (You might need to adjust or redefine some paths or settings specific to this mirror)

// Forward declarations of functions
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

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        // Fork a child process to handle the request just like in the main server
        if (fork() == 0) {
            close(server_fd);
            crequest(client_fd);
            close(client_fd);
            exit(0);
        }
        close(client_fd);
    }

    return 0;
}

// Implement the crequest function exactly as it's in the main server
void crequest(int client_fd) {
    char buffer[1024];

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
        if (bytes_read <= 0) {
            break;  // Read error or connection closed by client
        }

        buffer[bytes_read] = '\0'; // Ensure string is null-terminated
        printf("Command received: %s\n", buffer);

        // Example command handlers, make sure to implement these based on your actual application logic
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
             printf("Client Disconnected");
            break; // Exit the loop and close the connection
        }
    }
    close(client_fd); // Close the client socket at the end of the session
}

// Implement all the other handling functions as they are in the main server

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
    if (size1 > size2 || size1 < 0 || size2 < 0)
    {
        char *msg = "Invalid size range.\n";
        write(client_fd, msg, strlen(msg));
        return;
    }

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
        snprintf(tarCommand, sizeof(tarCommand), "tar -czf %s -T %s --transform='s|.*/||' 2> /dev/null", tarFilePath, TEMP_FILE_LIST);

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
            char *msg = "Failed to create tar file.\n";
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
            char *msg = "Failed to create tar file.\n";
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
    if (strlen(date_str) != 10) {
        return -1; // Length must be exactly 10 characters for valid YYYY-MM-DD format
    }

    // Check for correct delimiter positions
    if (date_str[4] != '-' || date_str[7] != '-') {
        return -1; // Delimiters must be at the correct positions
    }

    // Parse the date string
    if (strptime(date_str, "%Y-%m-%d", &tm) == NULL)
    {
        return -1; // Parsing error
    }

    // Validate ranges (optional, strptime does some checking but more could be done if necessary)
    if (tm.tm_year < 0 || tm.tm_mon < 0 || tm.tm_mday < 0 || tm.tm_mon > 11 || tm.tm_mday > 31) {
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
        char *msg = "Failed to create tar file.\n";
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
    if (strlen(date_str) != 10) {
        return -1; // Length must be exactly 10 characters for valid YYYY-MM-DD format
    }

    // Check for correct delimiter positions
    if (date_str[4] != '-' || date_str[7] != '-') {
        return -1; // Delimiters must be at the correct positions
    }

    // Parse the date string
    if (strptime(date_str, "%Y-%m-%d", &tm) == NULL)
    {
        return -1; // Parsing error
    }

    // Validate ranges (optional, strptime does some checking but more could be done if necessary)
    if (tm.tm_year < 0 || tm.tm_mon < 0 || tm.tm_mday < 0 || tm.tm_mon > 11 || tm.tm_mday > 31) {
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
        char *msg = "Failed to create tar file.\n";
        write(client_fd, msg, strlen(msg));
    }

    // Cleanup
    unlink(TEMP_FILE_LIST);
}
