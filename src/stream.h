#ifndef LPX_STREAM_H
#define LPX_STREAM_H

#include <stdint.h>

/**
 * Структура записи в индексе потока
 */
typedef struct FrameMeta {
    int64_t start_time;
    int64_t end_time;
} FrameMeta;

int32_t stream_find_frame(FrameMeta **index, uint32_t index_len, uint64_t time);

#endif //LPX_STREAM_H
