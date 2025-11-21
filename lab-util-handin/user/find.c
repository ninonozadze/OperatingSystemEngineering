#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

#define MAX_PATH 512

char* get_base(char *path) {
    static char base[DIRSIZ+1];
    char *last_slash = path + strlen(path);

    while (last_slash > path && *last_slash != '/') {
        last_slash--;
    }
    last_slash++;

    if (strlen(last_slash) >= DIRSIZ) {
        return last_slash;
    }

    memset(base, 0, sizeof(base));
    memmove(base, last_slash, strlen(last_slash));

    return base;
}

void print_file_if_match(char *path, char *target) {
    if (strcmp(get_base(path), target) == 0) {
        printf("%s\n", path);
    }
}

int open_directory(char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(2, "Error: Cannot open directory %s\n", path);
    }
    return fd;
}

int get_file_stat(int fd, char *path, struct stat *st) {
    if (fstat(fd, st) < 0) {
        fprintf(2, "Error: Cannot stat %s\n", path);
        return -1;
    }
    return 0;
}

void search_directory(char *path, char* target) {
    char filepath[MAX_PATH];
    struct dirent d;
    struct stat s;

    int fd = open_directory(path);
    if (fd < 0) return;

    if (get_file_stat(fd, path, &s) < 0) {
        close(fd);
        return;
    }

    strcpy(filepath, path);
    char *p = filepath + strlen(filepath);
    *p++ = '/';
    *p = 0;

    while (read(fd, &d, sizeof(d)) == sizeof(d)) {
        if (d.inum == 0) continue;

        memmove(p, d.name, DIRSIZ);
        p[DIRSIZ] = 0;

        if (stat(filepath, &s) < 0) {
            fprintf(2, "Error: Cannot stat %s\n", filepath);
            continue;
        }

        if (s.type == T_FILE) {
            print_file_if_match(filepath, target);
        } else if (s.type == T_DIR) {
            if (strcmp(d.name, ".") != 0 && strcmp(d.name, "..") != 0) {
                search_directory(filepath, target);
            }
        }
    }

    close(fd);
}

int main(int argc, char *argv[]) {

    char *dir = argv[1];
    char *file = argv[2];

    search_directory(dir, file);
    exit(0);
}