#ifndef LPX_STREAM_H
#define LPX_STREAM_H

#include <stdint.h>
#include <archive.h>

/**
 * Структура записи в индексе потока
 */
typedef struct FrameMeta {
    int64_t start_time; // ситемное (астрономическое) время запроса фрейма в микросекундах
    int64_t end_time; // систмное (астрономическое) время получения фрейма в микросекундах
} FrameMeta;

/**
 * Поток байт зип-архива видео потока. Видео поток читается с диска и сжимается на лету и не буферезируется.
 */
typedef struct StreamArchiveStream StreamArchiveStream;

/**
 * Поиск индекса фрейма ближайшего к заданному time
 * index - индекс фреймов
 * index - длина индекса фреймов
 * time - время в микросекундах относительно момента начала стриминга (времени запроса первого фрейма)
 */
int32_t stream_find_frame(FrameMeta **index, uint32_t index_len, uint64_t time);

StreamArchiveStream *stream_create_archive_stream(struct archive *archive, char **files, size_t files_size);

void stream_archive_callbacks(StreamArchiveStream *stream, archive_open_callback **open_cb,
                              archive_write_callback **write_cb, archive_close_callback **close_cb);

int *stream_pipe(StreamArchiveStream *stream);

ssize_t stream_write_block(StreamArchiveStream *archiveStream);

void stream_free(StreamArchiveStream *stream);

void stream_finish(StreamArchiveStream *stream);

void stream_close(StreamArchiveStream *archiveStream);

#endif //LPX_STREAM_H
