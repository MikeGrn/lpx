#include <stddef.h>
#include <malloc.h>
#include "include/converter.h"
#include <png.h>
#include <memory.h>
#include <lpxstd.h>

static void raw12_to_raw8_lo(const uint8_t *raw12, size_t input_len, uint8_t *raw8) {
    for (int i = 0, j = 0; i < input_len; i += 3, j += 2) {
        raw8[j] = (uint8_t) ((raw12[i] << 4 & 0xFF) | (raw12[i + 2] & 0xF));
        raw8[j + 1] = (uint8_t) ((raw12[i + 1] << 4 & 0xFF) | (raw12[i + 2] >> 4));
    }
}

static void png_write_data(png_structp png_ptr, png_bytep data, png_size_t length) {
    MemBuf *p = png_get_io_ptr(png_ptr);
    size_t nsize = p->size + length;

    if (p->buffer) {
        p->buffer = realloc(p->buffer, nsize);
    } else {
        p->buffer = malloc(nsize);
    }

    if (!p->buffer) {
        png_error(png_ptr, "Write Error");
    }

    /* copy new bytes to end of buffer */
    memcpy(p->buffer + p->size, data, length);
    p->size += length;
}

MemBuf *create_mem_buf() {
    MemBuf *mem_buf = xmalloc(sizeof(MemBuf));
    if (mem_buf) {
        mem_buf->buffer = NULL;
        mem_buf->size = 0;
    }
    return mem_buf;
}

void free_mem_buf(MemBuf *mem_buf) {
    if (mem_buf->buffer) {
        free(mem_buf->buffer);
    }
    free(mem_buf);
}

int8_t write_png_to_mem(uint8_t *src, MemBuf *mem_buf, uint16_t input_width, uint16_t input_height) {
    int8_t res = 0;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytep row = NULL;

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
        fprintf(stderr, "Could not allocate write struct\n");
        res = 1;
        goto finalise;
    }

    png_set_write_fn(png_ptr, mem_buf, png_write_data, NULL);

    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        fprintf(stderr, "Could not allocate info struct\n");
        res = 1;
        goto finalise;
    }

    // Setup Exception handling
    if (setjmp(png_jmpbuf(png_ptr))) {
        fprintf(stderr, "Error during png creation\n");
        res = 1;
        goto finalise;
    }

    png_set_IHDR(png_ptr, info_ptr, input_width, input_height,
                 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_write_info(png_ptr, info_ptr);

    row = (png_bytep) malloc(3 * input_width * sizeof(png_byte));

    int x, y;
    for (y = 0; y < input_height; y++) {
        for (x = 0; x < input_width; x++) {
            row[x * 3] = src[y * input_width + x];
            row[x * 3 + 1] = src[y * input_width + x];
            row[x * 3 + 2] = src[y * input_width + x];
        }
        png_write_row(png_ptr, row);
    }

    png_write_end(png_ptr, NULL);

    printf("png has been written\n");
    finalise:
    if (info_ptr != NULL) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
    if (png_ptr != NULL) png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
    if (row != NULL) free(row);

    return res;
}


int8_t convert_raw12_to_png_mem(uint8_t *src, size_t src_len, MemBuf *mem_buf, uint16_t input_width, uint16_t input_height) {
    int8_t res = 0;

    size_t image_len = input_width * input_height;

    uint8_t *raw8_lo = malloc(image_len);
    if (raw8_lo == NULL) {
        return 1;
    }
    raw12_to_raw8_lo(src, src_len, raw8_lo);

    res = write_png_to_mem(raw8_lo, mem_buf, input_width, input_height);

    return res;
}
