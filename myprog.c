#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX 257

struct FileMetadata {
    char name[256];
    unsigned int permissions;
    unsigned long size;
};

struct FileMetadata captureMetadata(const char *path) {
    struct FileMetadata metadata;
    struct stat st;

    if (stat(path, &st) == 0) {
        strncpy(metadata.name, path, sizeof(metadata.name));
        metadata.permissions = st.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
        metadata.size = st.st_size;
    } else {
        perror("stat");
        exit(EXIT_FAILURE);
    }

    return metadata;
}

void captureDirectory(const char *basePath) {
    char path[MAX];
    struct dirent *dp;
    DIR *dir = opendir(basePath);

    if (!dir) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    while ((dp = readdir(dir)) != NULL) {
        if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0) {
            sprintf(path, "%s/%s", basePath, dp->d_name);
            struct FileMetadata metadata = captureMetadata(path);
            printf("%s\n", metadata.name);
            if (S_ISDIR(metadata.permissions)) {
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

   
