#ifndef LPX_RASPIRAW_H
#define LPX_RASPIRAW_H

#include "stream_storage.h"

#define RR_MMAL 1 // ошибки MMAL
#define RR_CFG  2 // ошибки конфигурации
#define RR_IO   3 // ошибки ввода вывода по i2c

typedef struct Raspiraw Raspiraw;

typedef void (*raw_frame_callback)(uint8_t *frame, size_t frame_len, void *user_data);

int8_t raspiraw_init(Raspiraw **raspiraw, raw_frame_callback callback);

int8_t raspiraw_start(Raspiraw *raspiraw, void *user_data);

int8_t raspiraw_stop(Raspiraw *raspiraw);

int8_t raspiraw_close(Raspiraw *raspiraw);

#endif //LPX_RASPIRAW_H
