#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Define a structure to hold file metadata
struct FileMetadata {
    char name[256];  // File name
    mode_t mode;     // File mode (permissions)
    off_t size;      // File size
};

// Function to capture file metadata
struct FileMetadata captureMetadata(const char *path) {
    struct FileMetadata metadata;
    struct stat st;

    if (stat(path, &st) == 0) {
        strncpy(metadata.name, path, sizeof(metadata.name));
        metadata.mode = st.st_mode;
        metadata.size = st.st_size;
    } else {
        // Handle error
        perror("stat");
        exit(EXIT_FAILURE);
    }

    return metadata;
}

// Function to recursively capture directory structure and file metadata
void captureDirectory(const char *basePath) {
    char path[PATH_MAX];
    struct dirent *dp;
    DIR *dir = opendir(basePath);

    if (!dir) {
        // Handle error
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    while ((dp = readdir(dir)) != NULL) {
        if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0) {
            sprintf(path, "%s/%s", basePath, dp->d_name);
            struct FileMetadata metadata = captureMetadata(path);
            
            // Print or store metadata as required
            printf("%s\n", metadata.name);
            
            if (S_ISDIR(metadata.mode)) {
                // If it's a directory, recursively capture its contents
                captureDirectory(path);
            }
        }
    }

    closedir(dir);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <directory_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    printf("Before monitoring:\n");
    captureDirectory(argv[1]);

    printf("\nMonitoring...\n");
    printf("Press Enter to capture snapshot after monitoring.\n");
    getchar();

    printf("\nAfter monitoring:\n");
    captureDirectory(argv[1]);

    return 0;

}