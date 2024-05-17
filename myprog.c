#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include <stdbool.h>
#include <sys/wait.h>

#define MAX_PATH_LENGTH 4096
#define MAX_FILES 100

struct FileDetails {
    char name[MAX_PATH_LENGTH];
    time_t modified_time;
    off_t file_size;
    mode_t file_permissions;
};

void capture_directory_snapshot(const char *directory_path, struct FileDetails *previous_snapshot, const char *quarantine_directory) {
    printf("Capturing snapshot for directory: %s\n", directory_path);

    char snapshot_file_path[MAX_PATH_LENGTH];
    snprintf(snapshot_file_path, MAX_PATH_LENGTH, "%s/DirectorySnapshot.txt", directory_path);

    int snapshot_file = open(snapshot_file_path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (snapshot_file == -1) {
        perror("Error opening snapshot file");
        exit(EXIT_FAILURE);
    }

    dprintf(snapshot_file, "Directory snapshot: %s\n", directory_path);
    dprintf(snapshot_file, "---------------------------------\n");

    DIR *directory;
    struct dirent *file;
    directory = opendir(directory_path);
    if (directory == NULL) {
        perror("Error opening directory");
        exit(EXIT_FAILURE);
    }

    int file_count = 0;

    while ((file = readdir(directory)) != NULL && file_count < MAX_FILES) {
        if (strcmp(file->d_name, ".") != 0 && strcmp(file->d_name, "..") != 0) {
            struct stat file_status;
            char file_path[MAX_PATH_LENGTH];
            snprintf(file_path, MAX_PATH_LENGTH, "%s/%s", directory_path, file->d_name);

            if (stat(file_path, &file_status) == -1) {
                perror("Error getting file status");
                exit(EXIT_FAILURE);
            }

            if(strcmp(file->d_name, "DirectorySnapshot.txt") == 0) continue;
            struct FileDetails file_metadata;
            strcpy(file_metadata.name, file->d_name);
            file_metadata.modified_time = file_status.st_mtime;
            file_metadata.file_size = file_status.st_size;

            int pipe_fd[2];
            pipe(pipe_fd);
            pid_t child_pid = fork();
            if(child_pid == -1) {
                perror("Fork failed");
                exit(EXIT_FAILURE);
            } else if(child_pid == 0) {
                close(pipe_fd[0]);
                if((S_ISREG(file_status.st_mode) || S_ISDIR(file_status.st_mode)) && stat(file_path, &file_status) != -1 && ((file_status.st_mode & S_IRUSR) || (file_status.st_mode & S_IWUSR) || (file_status.st_mode & S_IXUSR))){
                    file_metadata.file_permissions = file_status.st_mode;
                } else {
                    file_metadata.file_permissions = 0;
                }
                write(pipe_fd[1], &file_metadata.file_permissions, sizeof(mode_t));
                close(pipe_fd[1]);
                exit(EXIT_SUCCESS);
            } else {
                close(pipe_fd[1]);
                mode_t child_file_permissions;
                read(pipe_fd[0], &child_file_permissions, sizeof(mode_t));
                file_metadata.file_permissions = child_file_permissions;
                close(pipe_fd[0]);
            }

            if(file_metadata.file_permissions == 0){
                char quarantine_path[MAX_PATH_LENGTH];
                snprintf(quarantine_path, MAX_PATH_LENGTH, "%s/%s", quarantine_directory, file->d_name);
                rename(file_path, quarantine_path);
            }

            file_count++;
            dprintf(snapshot_file, "File: %s\n", file_metadata.name);
            dprintf(snapshot_file, "Size: %lld bytes\n", (long long)file_metadata.file_size);
            dprintf(snapshot_file, "Permissions: %o\n", file_metadata.file_permissions);
            dprintf(snapshot_file, "Last Modified: %s", ctime(&file_metadata.modified_time));
            dprintf(snapshot_file, "\n");

            if (S_ISDIR(file_status.st_mode)) {
                capture_directory_snapshot(file_path, previous_snapshot, quarantine_directory);
            }
        }
    }

    closedir(directory);
    close(snapshot_file);
}

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 12) {
        printf("Usage: %s -q quarantine_directory directory1 [directory2 ... directory10]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    for (int i = 2; i < argc; i++) {
        pid_t process_id = fork();
        if (process_id == -1) {
            perror("Fork failed");
            exit(EXIT_FAILURE);
        } else if (process_id == 0) { // Child process
            capture_directory_snapshot(argv[i], NULL, argv[1]);
            exit(EXIT_SUCCESS);
        }
    }

    int exit_status;
    pid_t child_pid;
    while ((child_pid = wait(&exit_status)) > 0) {
        printf("Child process with PID %d finished with code %d.\n", child_pid, WEXITSTATUS(exit_status));
    }

    return 0;
}
