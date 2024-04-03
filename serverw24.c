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
#include <ftw.h>
#include <libgen.h>
#include <limits.h>
#include <tar.h>


#define PORT 8080
#define PATH_MAX 4096
#define MAX_EXTENSIONS 3
#define TAR_FILE "/tmp/files.tar.gz"
#define TAR_FILE_Date "/tmp/filtered_files.tar.gz"

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
            char filename[PATH_MAX];
             sscanf(buffer + 6, "%s", filename); // Extract the filename properly
             handle_w24fn(client_fd, filename);

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

// ------------------------------------------------------------------//

int dir_time_compare(const void *a, const void *b) {
    struct stat stat1, stat2;
    stat(*(const char **)a, &stat1);
    stat(*(const char **)b, &stat2);
    return stat1.st_mtime - stat2.st_mtime;  // Compare modification times
}

void handle_dirlist_t(int client_fd) {
    DIR *d;
    struct dirent *dir;
    char *dirList[1024];  // Assuming we won't have more than 1024 directories
    int count = 0;

    d = opendir(".");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type == DT_DIR && strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                dirList[count++] = strdup(dir->d_name);
            }
        }
        closedir(d);

        // Sort the directory list based on modification time
        qsort(dirList, count, sizeof(char*), dir_time_compare);

        char response[8192] = "";
        for (int i = 0; i < count; i++) {
            strcat(response, dirList[i]);
            strcat(response, "\n");
            free(dirList[i]);
        }
        write(client_fd, response, strlen(response));
    } else {
        char *errorMsg = "Failed to open directory.";
        write(client_fd, errorMsg, strlen(errorMsg));
    }
}

// ------------------------------------------------------------------//


struct file_info {
    char path[PATH_MAX];
    off_t size;
    mode_t mode;
    time_t mtime;
};

static struct file_info found_file;
static char target_filename[256];

static int find_file_in_directory(const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        return 0;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG && strcmp(target_filename, entry->d_name) == 0) {
            // Construct the full path for the file
            snprintf(found_file.path, PATH_MAX, "%s/%s", dir_path, entry->d_name);

            struct stat file_stat;
            if (stat(found_file.path, &file_stat) == 0) {
                found_file.size = file_stat.st_size;
                found_file.mode = file_stat.st_mode;
                found_file.mtime = file_stat.st_mtime;
                closedir(dir);
                return 1;  // File found
            }
        } else if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char next_dir_path[PATH_MAX];
            snprintf(next_dir_path, PATH_MAX, "%s/%s", dir_path, entry->d_name);
            
            if (find_file_in_directory(next_dir_path)) {
                closedir(dir);
                return 1;  // File found in sub-directory
            }
        }
    }

    closedir(dir);
    return 0;  // File not found
}

void handle_w24fn(int client_fd, const char *filename) {
    memset(&found_file, 0, sizeof(found_file));
    strncpy(target_filename, filename, sizeof(target_filename) - 1);

    char *home_dir = getenv("HOME"); // Get the home directory path
    if (!home_dir) {
        home_dir = "/"; // Fallback to root if HOME is not set
    }

    if (find_file_in_directory(home_dir)) {
        char file_info[1024] = {0}; // Initialize buffer to zero
        sprintf(file_info, "File: %s, Size: %lld, Permissions: %o, Last Modified: %s",
                found_file.path, (long long)found_file.size, found_file.mode & 0777,
                ctime(&found_file.mtime));
        write(client_fd, file_info, strlen(file_info));
    } else {
        char msg[1024] = "File not found\n"; // Initialize message buffer
        write(client_fd, msg, strlen(msg));
    }
}


// ------------------------------------------------------------------//


struct size_filter {
    off_t min_size;
    off_t max_size;
};

static struct size_filter global_size_filter;

// Callback function for nftw
static int check_file_size(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    if (typeflag == FTW_F && sb->st_size >= global_size_filter.min_size && sb->st_size <= global_size_filter.max_size) {
        // Here you can add logic to process each file that meets the size criteria,
        // such as adding the file path to a list or directly appending it to a tar file
    }
    return 0;  // Continue traversing
}

void handle_w24fz(int client_fd, const char *sizeRange) {
    long size1, size2;
    sscanf(sizeRange, "%ld %ld", &size1, &size2);
    if (size1 > size2) {
        char *msg = "Invalid size range.\n";
        write(client_fd, msg, strlen(msg));
        return;
    }

    global_size_filter.min_size = size1;
    global_size_filter.max_size = size2;

    struct size_filter filter = { .min_size = size1, .max_size = size2 };
    
    // Path to the temporary tar file
    char tarPath[] = "/tmp/files.tar.gz";
    // Command to create tar.gz file
    char tarCommand[PATH_MAX];
    sprintf(tarCommand, "tar -czf %s -T /dev/null", tarPath); // Initialize empty tar.gz

    // Run tar command to initialize the archive
    system(tarCommand);

    // Traverse the directory tree
    nftw("/", check_file_size, 20, 0);

    // Check if the tar.gz file has been updated (means files were added)
    struct stat tar_stat;
    if (stat(tarPath, &tar_stat) == 0 && tar_stat.st_size > 0) {
        // Send the tar.gz file to the client
    } else {
        char *msg = "No file found within the specified size range.\n";
        write(client_fd, msg, strlen(msg));
    }

    // Remove the temporary tar file
    unlink(tarPath);
}


// ------------------------------------------------------------------//


static char *extensions[MAX_EXTENSIONS];
static int ext_count = 0;

static int check_file_extension(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    if (typeflag == FTW_F) {
        const char *ext = strrchr(fpath, '.');
        if (ext) {
            ext++;  // Move past the dot
            for (int i = 0; i < ext_count; i++) {
                if (strcmp(ext, extensions[i]) == 0) {
                    // File matches one of the extensions
                    // Add logic to append this file to the tar archive
                    char cmd[1024];
                    snprintf(cmd, sizeof(cmd), "tar -rf %s \"%s\"", TAR_FILE, fpath);
                    system(cmd);
                    break;
                }
            }
        }
    }
    return 0;  // Continue traversing
}

void handle_w24ft(int client_fd, const char *extensionList) {
    char *token = strtok((char *)extensionList, " ");
    while (token && ext_count < MAX_EXTENSIONS) {
        extensions[ext_count++] = strdup(token);
        token = strtok(NULL, " ");
    }

    // Initialize the tar file
    system("tar -cf " TAR_FILE " --files-from /dev/null");

    nftw("~", check_file_extension, 20, FTW_NS);

    // Check if the tar file has any content
    struct stat tar_stat;
    if (stat(TAR_FILE, &tar_stat) == 0 && tar_stat.st_size > 0) {
        // Send the tar.gz file to the client
        // Logic to send the file
    } else {
        char *msg = "No file found matching the specified extensions.\n";
        write(client_fd, msg, strlen(msg));
    }

    // Cleanup
    for (int i = 0; i < ext_count; i++) {
        free(extensions[i]);
    }
    ext_count = 0;
    unlink(TAR_FILE);  // Remove the temporary tar file
}


// ------------------------------------------------------------------//


static time_t target_date;
static int file_date_filter_db(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    if (typeflag == FTW_F) {
        if (difftime(sb->st_mtime, target_date) <= 0) { // Check if the file modification time is less than or equal to target date
            char cmd[1024];
            snprintf(cmd, sizeof(cmd), "tar -rf %s \"%s\"", TAR_FILE_Date, fpath);
            system(cmd);  // Append the file to the tar archive
        }
    }
    return 0; // Continue traversing
}

void handle_w24fdb(int client_fd, const char *dateStr) {
    struct tm tm;
    memset(&tm, 0, sizeof(struct tm));
    strptime(dateStr, "%Y-%m-%d", &tm);
    target_date = mktime(&tm);

    // Initialize the tar file
    system("tar -cf " TAR_FILE_Date " --files-from /dev/null");

    nftw("~", file_date_filter_db, 20, FTW_NS);

    // Check if the tar file has any content
    struct stat tar_stat;
    if (stat(TAR_FILE_Date, &tar_stat) == 0 && tar_stat.st_size > 0) {
        // Logic to send the tar.gz file to the client
    } else {
        char *msg = "No file found before the specified date.\n";
        write(client_fd, msg, strlen(msg));
    }

    unlink(TAR_FILE_Date); // Remove the temporary tar file
}

// ------------------------------------------------------------------//


static time_t target_date;

static int file_date_filter_da(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    if (typeflag == FTW_F) {
        if (difftime(sb->st_mtime, target_date) >= 0) { // Check if the file modification time is greater than or equal to target date
            char cmd[1024];
            snprintf(cmd, sizeof(cmd), "tar -rf %s \"%s\"", TAR_FILE_Date, fpath);
            system(cmd);  // Append the file to the tar archive
        }
    }
    return 0; // Continue traversing
}

void handle_w24fda(int client_fd, const char *dateStr) {
    struct tm tm;
    memset(&tm, 0, sizeof(struct tm));
    strptime(dateStr, "%Y-%m-%d", &tm);
    target_date = mktime(&tm);

    // Initialize the tar file
    system("tar -cf " TAR_FILE_Date " --files-from /dev/null");

    nftw("~", file_date_filter_da, 20, FTW_NS);

    // Check if the tar file has any content
    struct stat tar_stat;
    if (stat(TAR_FILE_Date, &tar_stat) == 0 && tar_stat.st_size > 0) {
        // Logic to send the tar.gz file to the client
    } else {
        char *msg = "No file found after the specified date.\n";
        write(client_fd, msg, strlen(msg));
    }

    unlink(TAR_FILE_Date); // Remove the temporary tar file
}
