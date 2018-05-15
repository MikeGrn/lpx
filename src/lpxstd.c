#include "lpxstd.h"
#include <stdint.h>

__time_t toMicroSeconds(struct timeval tv) {
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

