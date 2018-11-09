#include <stddef.h>
#include <malloc.h>
#include "include/converter.h"
#include <png.h>

static int8_t read_file(char *file, size_t input_len, uint8_t *input) {
    FILE *fd = fopen(file, "rb");
    if (fd == NULL) {
        fprintf(stderr, "Could not open %s", file);
        return 1;
    }
    size_t res = fread(input, 1, input_len, fd);
    fclose(fd);
    
    if (res == input_len) {
        return 0;
    } else {
        return 1;
    }
}

static void raw12_to_raw8_lo(const uint8_t *raw12, size_t input_len, uint8_t *raw8) {
    for (int i = 0, j = 0; i < input_len; i += 3, j += 2) {
        raw8[j] = (uint8_t) ((raw12[i] << 4 & 0xFF) | (raw12[i + 2] & 0xF));
        raw8[j + 1] = (uint8_t) ((raw12[i + 1] << 4 & 0xFF) | (raw12[i + 2] >> 4));
    }
}

static void raw12_to_raw8_hi(const uint8_t *raw12, size_t input_len, uint8_t *raw8) {
    for (int i = 0, j = 0; i < input_len; i += 3, j += 2) {
        raw8[j] = raw12[i];
        raw8[j + 1] = raw12[i + 1];
    }
}


static int8_t write_png(char *filename, uint32_t width, uint32_t height, const uint8_t *buffer, char *title) {
    int8_t res = 0;
    FILE *fp = NULL;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytep row = NULL;

    fp = fopen(filename, "wb");
    if (fp == NULL) {
        fprintf(stderr, "Could not open file %s for writing\n", filename);
        res = 1;
        goto finalise;
    }

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
        fprintf(stderr, "Could not allocate write struct\n");
        res = 1;
        goto finalise;
    }

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

    png_init_io(png_ptr, fp);

    png_set_IHDR(png_ptr, info_ptr, width, height,
                 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    if (title != NULL) {
        png_text title_text;
        title_text.compression = PNG_TEXT_COMPRESSION_NONE;
        title_text.key = "Title";
        title_text.text = title;
        png_set_text(png_ptr, info_ptr, &title_text, 1);
    }

    png_write_info(png_ptr, info_ptr);

    row = (png_bytep) malloc(3 * width * sizeof(png_byte));

    int x, y;
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            row[x * 3] = buffer[y * width + x];
            row[x * 3 + 1] = buffer[y * width + x];
            row[x * 3 + 2] = buffer[y * width + x];
        }
        png_write_row(png_ptr, row);
    }

    png_write_end(png_ptr, NULL);

    finalise:
    if (fp != NULL) fclose(fp);
    if (info_ptr != NULL) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
    if (png_ptr != NULL) png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
    if (row != NULL) free(row);

    return res;
}

int8_t convert_raw12_to_png(char *src_file, char *dst_file_lo, char *dst_file_hi, uint16_t input_width, uint16_t input_height) {
    int8_t res = 0;

    size_t image_len = input_width * input_height;
    size_t input_len = (size_t) (image_len * 1.5);
    
    uint8_t *input = malloc(input_len);
    if (input == NULL) {
        return 1;
    }
    
    res = read_file(src_file, input_len, input);
    if (res != 0) {
        goto free_input;
    }

    uint8_t *raw8_lo = malloc(image_len);
    if (raw8_lo == NULL) {
        res = 1;
        goto free_input;
    }
    raw12_to_raw8_lo(input, input_len, raw8_lo);

    res = write_png(dst_file_lo, input_width, input_height, raw8_lo, src_file);
    if (res) {
        goto free_input;
    }

    uint8_t *raw8_hi = malloc(image_len);
    if (raw8_hi == NULL) {
        res = 1;
        goto free_input;
    }
    raw12_to_raw8_hi(input, input_len, raw8_hi);

    res = write_png(dst_file_hi, input_width, input_height, raw8_hi, src_file);

    free_input:
    free(input);
    
    return res;
}

