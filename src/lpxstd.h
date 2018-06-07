//
// Created by azhidkov on 11.05.18.
//

#ifndef LPX_LPXSTD_H
#define LPX_LPXSTD_H

#include <stdint.h>
#include <stdlib.h>

#define ALEN(arr) ((sizeof (arr)) / sizeof ((arr)[0]))

#define LPX_SUCCESS 0
#define LPX_IO      1

uint64_t tv2mks(struct timeval tv);

uint64_t s2mks(uint32_t seconds);

void printArray(char *prefix, const unsigned char *arr, int len);

void* xmalloc(size_t n);

void* xcalloc(size_t n, size_t size);

char* append_path(char *base, char* child);

int8_t file_size(FILE *file, off_t *size);

#endif //LPX_LPXSTD_H
