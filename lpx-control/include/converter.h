#ifndef REPO_CONVERTER_H
#define REPO_CONVERTER_H

#include <stdint.h>
#include <stdio.h>

typedef struct MemBuf {
    uint8_t *buffer;
    size_t size;
} MemBuf;

MemBuf *create_mem_buf();

void free_mem_buf(MemBuf *mem_buf);

int8_t write_png_to_mem(uint8_t *src, MemBuf *mem_buf, uint16_t input_width, uint16_t input_height);

#endif //REPO_CONVERTER_H
