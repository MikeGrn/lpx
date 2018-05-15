//
// Created by azhidkov on 11.05.18.
//

#ifndef LPX_LPXSTD_H
#define LPX_LPXSTD_H

#include <stdint.h>
#include <stdlib.h>

#define ALEN(arr) ((sizeof (arr)) / sizeof ((arr)[0]))

__time_t toMicroSeconds(struct timeval tv);

#endif //LPX_LPXSTD_H
