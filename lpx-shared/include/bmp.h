#ifndef LPX_BMP_H
#define LPX_BMP_H

#include <stdint.h>
#include <stdio.h>

uint8_t raw12_to_bmp(const uint8_t *raw_12, size_t width, size_t height, uint8_t **bmp, size_t *bmp_size);

#endif
