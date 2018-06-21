#include <stdio.h>
#include "../include/lpxstd.h"
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <dirent.h>
#include <list.h>

uint64_t tv2mks(struct timeval tv) {
    return tv.tv_sec * 1000000ULL + tv.tv_usec;
}

uint64_t tv2ms(struct timeval tv) {
    return (tv.tv_sec * 1000ULL) + (tv.tv_usec / 1000ULL);
}

void printArray(char *prefix, const unsigned char *arr, int len) {
    printf("%s", prefix);
    for (int i = 0; i < len; i++) {
        printf("0x%02X ", arr[i]);
    }
    printf("\n");
}

void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (p == NULL) {
        // На линуксе вроде как никогда не случается - когда кончается память, линукс запускает OOM-killer,
        // который убивает случайные процессы, чтобы освободить память, так что скорее должен прилететь SIGKILL
        fprintf(stderr, "Could not allocate %ld bytes", n);
        abort();
    }
    return p;
}

void *xcalloc(size_t n, size_t size) {
    void *p = calloc(n, size);
    if (p == NULL) {
        fprintf(stderr, "Could not allocate %ld bytes", n);
        abort();
    }
    return p;
}

char *append_path(char *base, char *child) {
    size_t size = sizeof(char) * (strlen(base) + 1 + strlen(child) + 1);
    char *path = xcalloc(size, sizeof(char));
    snprintf(path, size, "%s/%s", base, child);
    return path;
}

int8_t file_size(FILE *file, off_t *size) {
    int fd = fileno(file);
    struct stat buf;
    if (fstat(fd, &buf) == -1) {
        return LPX_IO;
    }
    *size = buf.st_size;

    return LPX_SUCCESS;
}

char *itoa(uint64_t i) {
    char *res = xmalloc(MAX_INT_LEN);
    snprintf(res, MAX_INT_LEN, "%" PRId64, i);
    return res;
}


int8_t list_directory(char *dir, char ***children, size_t *len) {
    DIR *dp;
    struct dirent *dir_entry;

    dp = opendir(dir);
    List *chldrn = lst_create();
    if (dp != NULL) {
        while ((dir_entry = readdir(dp)) != NULL) {
            if (strcmp(dir_entry->d_name, ".") == 0 || strcmp(dir_entry->d_name, "..") == 0) {
                continue;
            }
            char *name = strndup(dir_entry->d_name, 256);
            lst_append(chldrn, name);
        }
        closedir(dp);
        uint32_t entries = lst_size(chldrn);
        char **res = xcalloc(entries, sizeof(char*));
        lst_to_array(chldrn, (void **) res);
        *children = res;
        *len = entries;
        lst_free(chldrn);
    } else {
        perror("Couldn't open the directory");
        return LPX_IO;
    }

    return LPX_SUCCESS;
}