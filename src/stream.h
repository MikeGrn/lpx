#ifndef LPX_STREAM_H
#define LPX_STREAM_H

#include <stdint.h>

/*
 * Структура записи в индексе потока
 */
typedef struct FrameMeta {
    int64_t start_time; // ситемное (астрономическое) время запроса фрейма в микросекундах
    int64_t end_time; // систмное (астрономическое) время получения фрейма в микросекундах
} FrameMeta;

/*
 * Поиск индекса фрейма ближайшего к заданному time
 * index - индекс фреймов
 * index - длина индекса фреймов
 * time - время в микросекундах относительно момента начала стриминга (времени запроса первого фрейма)
 */
int32_t stream_find_frame(FrameMeta **index, uint32_t index_len, uint64_t time);

#endif //LPX_STREAM_H
