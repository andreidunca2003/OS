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

void read_existing_snapshot(const char *snapshot_path, struct FileDetails *snapshot, int *count) {
    FILE *file = fopen(snapshot_path, "r");
    if (file == NULL) {
        *count = 0;
        return;
    }

    *count = 0;
    while (fscanf(file, "%s %ld %lld %o", snapshot[*count].name, &snapshot[*count].modified_time, &snapshot[*count].file_size, &snapshot[*count].file_permissions) == 4) {
        (*count)++;
    }

    fclose(file);
}

bool is_snapshot_same(struct FileDetails *old_snapshot, int old_count, struct FileDetails *new_snapshot, int new_count) {
    if (old_count != new_count) {
        return false;
    }

    for (int i = 0; i < old_count; i++) {
        if (strcmp(old_snapshot[i].name, new_snapshot[i].name) != 0 ||
            old_snapshot[i].modified_time != new_snapshot[i].modified_time ||
            old_snapshot[i].file_size != new_snapshot[i].file_size ||
            old_snapshot[i].file_permissions != new_snapshot[i].file_permissions) {
            return false;
        }
    }

    return true;
}

void write_snapshot(const char *snapshot_path, struct FileDetails *snapshot, int count) {
    FILE *file = fopen(snapshot_path, "w");
    if (file == NULL) {
        perror("Error opening snapshot file for writing");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < count; i++) {
        fprintf(file, "%s %ld %lld %o\n", snapshot[i].name, snapshot[i].modified_time, snapshot[i].file_size, snapshot[i].file_permissions);
    }

    fclose(file);
}

void capture_directory_snapshot(const char *directory_path, struct FileDetails *previous_snapshot, int previous_count, const char *quarantine_directory) {
    printf("Capturing snapshot for directory: %s\n", directory_path);

    char snapshot_file_path[MAX_PATH_LENGTH];
    snprintf(snapshot_file_path, MAX_PATH_LENGTH, "%s/DirectorySnapshot.txt", directory_path);

    struct FileDetails new_snapshot[MAX_FILES];
    int new_count = 0;

    DIR *directory;
    struct dirent *file;
    directory = opendir(directory_path);
    if (directory == NULL) {
        perror("Error opening directory");
        exit(EXIT_FAILURE);
    }

    while ((file = readdir(directory)) != NULL && new_count < MAX_FILES) {
        if (strcmp(file->d_name, ".") != 0 && strcmp(file->d_name, "..") != 0) {
            struct stat file_status;
            char file_path[MAX_PATH_LENGTH];
            snprintf(file_path, MAX_PATH_LENGTH, "%s/%s", directory_path, file->d_name);

            if (stat(file_path, &file_status) == -1) {
                perror("Error getting file status");
                exit(EXIT_FAILURE);
            }

            if (strcmp(file->d_name, "DirectorySnapshot.txt") == 0) continue;
            struct FileDetails file_metadata;
            strcpy(file_metadata.name, file->d_name);
            file_metadata.modified_time = file_status.st_mtime;
            file_metadata.file_size = file_status.st_size;

            int pipe_fd[2];
            if (pipe(pipe_fd) == -1) {
                perror("Pipe failed");
                exit(EXIT_FAILURE);
            }

            pid_t child_pid = fork();
            if (child_pid == -1) {
                perror("Fork failed");
                exit(EXIT_FAILURE);
            } else if (child_pid == 0) { // Child process
                close(pipe_fd[0]); // Close unused read end
                dup2(pipe_fd[1], STDOUT_FILENO); // Redirect stdout to pipe
                close(pipe_fd[1]);

                execl("./check_permissions.sh", "./check_permissions.sh", file_path, (char *)NULL);
                perror("execl failed");
                exit(EXIT_FAILURE);
            } else { // Parent process
                close(pipe_fd[1]); // Close unused write end

                char permissions_str[4];
                read(pipe_fd[0], permissions_str, sizeof(permissions_str));
                close(pipe_fd[0]);

                file_metadata.file_permissions = strtol(permissions_str, NULL, 8);

                if (file_metadata.file_permissions == 0) {
                    char quarantine_path[MAX_PATH_LENGTH];
                    snprintf(quarantine_path, MAX_PATH_LENGTH, "%s/%s", quarantine_directory, file->d_name);
                    rename(file_path, quarantine_path);
                }

                new_snapshot[new_count++] = file_metadata;

                if (S_ISDIR(file_status.st_mode)) {
                    capture_directory_snapshot(file_path, previous_snapshot, previous_count, quarantine_directory);
                }
            }
        }
    }

    closedir(directory);

    if (!is_snapshot_same(previous_snapshot, previous_count, new_snapshot, new_count)) {
        write_snapshot(snapshot_file_path, new_snapshot, new_count);
    }
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
            struct FileDetails old_snapshot[MAX_FILES];
            int old_count;

            char snapshot_file_path[MAX_PATH_LENGTH];
            snprintf(snapshot_file_path, MAX_PATH_LENGTH, "%s/DirectorySnapshot.txt", argv[i]);

            read_existing_snapshot(snapshot_file_path, old_snapshot, &old_count);
            capture_directory_snapshot(argv[i], old_snapshot, old_count, argv[1]);

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
