#include <stdio.h>
#include <math.h>
#include <malloc.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

static void fill_bmp_header(size_t width, size_t height, size_t image_size, size_t file_size, uint8_t *bitmap) {
    // -- FILE HEADER -- //

    // bitmap signature
    bitmap[0] = 0x42; // B
    bitmap[1] = 0x4d; // M

    // file size
    *((size_t *) &bitmap[2]) = file_size;

    // reserved field (in hex. 00 00 00 00)
    memset(&bitmap[6], 0, sizeof(uint8_t) * 4);

    *((uint32_t *) &bitmap[10]) = (uint32_t) 54 + (256 * 4);

    // -- BITMAP HEADER -- //

    // header size
    bitmap[14] = 0x28; // 40
    memset(&bitmap[15], 0, sizeof(uint8_t) * 3);

    // width of the image
    *((uint32_t *) &bitmap[18]) = (uint32_t) width;

    // height of the image
    *((uint32_t *) &bitmap[22]) = (uint32_t) height;

    // reserved field
    bitmap[26] = 1;
    bitmap[27] = 0;

    // number of bits per pixel
    bitmap[28] = 8;
    bitmap[29] = 0;

    // compression method (no compression here)
    memset(&bitmap[30], 0, sizeof(uint8_t) * 4);

    // size of pixel data
    *((uint32_t *) &bitmap[34]) = (uint32_t) image_size;

    // horizontal resolution of the image - pixels per meter (2835)
    memset(&bitmap[38], 0, sizeof(uint8_t) * 4);

    // vertical resolution of the image - pixels per meter (2835)
    memset(&bitmap[42], 0, sizeof(uint8_t) * 4);

    // color pallette information
    bitmap[46] = 0xFF;
    memset(&bitmap[47], 0, sizeof(uint8_t) * 3);

    // number of important colors
    memset(&bitmap[50], 0, sizeof(uint8_t) * 4);

    for (uint16_t i = 0; i < 256; i++) {
        bitmap[54 + i * 4 + 0] = (uint8_t) i;
        bitmap[54 + i * 4 + 1] = (uint8_t) i;
        bitmap[54 + i * 4 + 2] = (uint8_t) i;
        bitmap[54 + i * 4 + 3] = 0;
    }
}

static uint8_t *generate_bmp(size_t width, size_t height, const uint8_t *bitmap, size_t *bmp_size) {
    size_t image_size = width * height;
    size_t file_size = 54 + (256 * 4) + image_size;
    uint8_t *content = malloc(sizeof(uint8_t) * file_size);
    fill_bmp_header(width, height, image_size, file_size, content);

    // -- PIXEL DATA -- //
    for (int i = 54 + (256 * 4); i < file_size; i++) {
        content[i] = bitmap[i - (54 + 256 * 4)];
    }

    *bmp_size = file_size;

    return content;
}

static uint8_t map_pixel(uint16_t pixel) {
    static uint8_t map[UINT16_MAX];
    static bool map_filled = false;
    if (!map_filled) {
        for (int i = 0; i < UINT16_MAX; i++) {
            map[i] = (uint8_t) i;
        }
        map_filled = true;
    }

    return map[pixel];
}


static uint8_t *raw12_to_bitmap(const uint8_t *input_buffer, size_t width, size_t height) {
    uint8_t *buffer = (uint8_t *) malloc(width * height * sizeof(uint8_t));
    if (buffer == NULL) {
        fprintf(stderr, "Could not create image buffer\n");
        return NULL;
    }

    for (int i = 0, j = 0; j < width * height; i += 3, j += 2) {
        uint16_t p1 = ((uint16_t) input_buffer[i]) << ((uint8_t) 4);
        p1 |= (input_buffer[i + 2] & ((uint8_t) 0xF));

        uint16_t p2 = ((uint16_t) input_buffer[i + 1]) << ((uint8_t) 4);;
        p2 |= (input_buffer[i + 2] >> ((uint8_t) 4));

        buffer[j] = map_pixel(p1);
        buffer[j + 1] = map_pixel(p2);
    }

    return buffer;
}

uint8_t raw12_to_bmp(const uint8_t *raw_12, size_t width, size_t height, uint8_t **bmp, size_t *bmp_size) {

    uint8_t *bitmap = raw12_to_bitmap(raw_12, width, height);
    if (bitmap == NULL) {
        return 1;
    }

    *bmp = generate_bmp(width, height, bitmap, bmp_size);
    free(bitmap);
}
