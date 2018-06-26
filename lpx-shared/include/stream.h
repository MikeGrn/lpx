#ifndef LPX_STREAM_H
#define LPX_STREAM_H

#include <stdint.h>
#include <archive.h>

/**
 * Ошибка генерации потока архива стрима
 */
#define STRM_IO -2

/**
 * Ошибка работы с архивом
 */
#define STRM_ARCHIVE 2

/**
 * Структура записи в индексе потока
 */
typedef struct FrameMeta {
    int64_t start_time; // ситемное (астрономическое) время запроса фрейма в микросекундах
    int64_t end_time; // систмное (астрономическое) время получения фрейма в микросекундах
} FrameMeta;

/**
 * Поток байт зип-архива видео потока. Файлы потока читаеются с диска блоками заданного размера и сжимаются на лету.
 */
typedef struct StreamArchiveStream StreamArchiveStream;

/**
 * Поиск индекса фрейма ближайшего к заданному time
 * index - индекс фреймов
 * index - длина индекса фреймов
 * time - время в микросекундах относительно момента начала стриминга (времени запроса первого фрейма)
 */
int32_t stream_find_frame(FrameMeta **index, uint32_t index_len, uint64_t time);

/**
 * Инициализирует структура архива потока, содержащего заданные файлы. В случае ошибки возвращает NULL.
 */
StreamArchiveStream *stream_create_archive_stream(struct archive *archive, char **files, size_t files_size);

/**
 * Записывает до `max` байт архива в буффер. Возвращает количество реально записанных байт, EOF в случае
 * когда стрим был целиком прочитан и STRM_IO в случае ошибок генерации архива стрима
 */
ssize_t stream_read(StreamArchiveStream *stream, uint8_t *buf, size_t max);

/**
 * Закрывет архив и освобождает все ресурсы
 */
void stream_close(StreamArchiveStream *archive_stream);

#endif //LPX_STREAM_H
