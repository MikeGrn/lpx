#ifndef REPO_CONVERTER_H
#define REPO_CONVERTER_H

#include <stdint.h>

int8_t convert_raw12_to_png(char *src_file, char *dst_file_lo, char *dst_file_hi, uint16_t input_width, uint16_t input_height);

#endif //REPO_CONVERTER_H
