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

void captureDirectory(const char *basePath) {
    char path[MAX];
    struct dirent *dp;
    DIR *dir = opendir(basePath);

    if (!dir) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }


    closedir(dir);
}

int main(int argc, char *argv[]) {

    printf("Before:\n");
    captureDirectory(argv[1]);

    printf("\nAfter:\n");
    captureDirectory(argv[1]);

    return 0;
}
