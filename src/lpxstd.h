//
// Created by azhidkov on 11.05.18.
//

#ifndef LPX_LPXSTD_H
#define LPX_LPXSTD_H

#include <stdint.h>
#include <stdlib.h>

#define ALEN(arr) ((sizeof (arr)) / sizeof ((arr)[0]))

__time_t tv2mks(struct timeval tv);

uint32_t s2mks(uint32_t seconds);

void printArray(char *prefix, const unsigned char *arr, int len);

#endif //LPX_LPXSTD_H
