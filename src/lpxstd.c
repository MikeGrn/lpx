#include <stdio.h>
#include "lpxstd.h"
#include <stdint.h>

__time_t tv2mks(struct timeval tv) {
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

uint32_t s2mks(uint32_t seconds) {
    return seconds * 1000000;
}

void printArray(char *prefix, const unsigned char *arr, int len) {
    printf("%s", prefix);
    for (int i = 0; i < len; i++) {
        printf("0x%02X ", arr[i]);
    }
    printf("\n");
}


