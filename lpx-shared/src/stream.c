#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <lpxstd.h>
#include <memory.h>
#include <libgen.h>
#include <stdbool.h>
#include <poll.h>
#include <assert.h>
#include <sys/stat.h>
#include <bmp.h>
#include "../include/stream.h"

typedef struct VideoStreamBytesStream {
    /**
     * Флаг того, был ли прочитан заголовок (4 байта количества фреймов)
     */
    bool header_read;

    /**
     * Абсолютные пути к файлам, которые должны попасть в архив.
     */
    char **file_paths;
    size_t files_size;

    /**
     * Индекс следующего файла который должен быть добавлен в архив.
     */
    uint32_t next_file;

    /**
     * Файл добавляемый в архив в данный момент
     */
    FILE *file;

    /**
     * Указатель на буффер с raw-фреймом
     */
    uint8_t *raw_buf;

    /**
     * Указатель на буффер с bmp-версией файла
     */
    uint8_t *bmp_start;

    /**
     * Указатель на читаемую часть буффера с bmp-версией файла
     */
    uint8_t *bmp;
    
    /**
     * Указатель на первый адрес после буфера с bmp-версией файла
     */
    uint8_t *bmp_eof;

} VideoStreamBytesStream;

VideoStreamBytesStream *stream_open(char **files, size_t files_size) {
    VideoStreamBytesStream *res = xcalloc(1, sizeof(VideoStreamBytesStream));
    res->header_read = false;
    res->file_paths = files;
    res->files_size = files_size;
    res->next_file = 0;
    res->file = NULL;

    return res;
}

ssize_t stream_find_frame(FrameMeta **index, size_t index_size, uint64_t time_offset) {
    int64_t stream_base = index[0]->start_time;
    for (size_t i = 0; i < index_size - 1; i++) {
        int64_t frameOffset = index[i]->start_time - stream_base;
        int64_t nextFrameOffset = index[i + 1]->start_time - stream_base;
        if (labs(nextFrameOffset - time_offset) > labs(frameOffset - time_offset)) {
            return i;
        }
    }
    if (stream_base + time_offset < index[index_size - 1]->end_time) {
        return index_size - 1;
    } else {
        return -1;
    }
}

ssize_t stream_find_frame_abs(FrameMeta **index, size_t index_size, uint64_t time) {
    for (size_t i = 0; i < index_size; i++) {
        if (index[i]->start_time <= time && index[i]->end_time >= time) {
            return time;
        }
    }
    return -1;
}

static void close_current_file(VideoStreamBytesStream *stream) {
    if (stream->raw_buf) {
        free(stream->raw_buf);
        stream->raw_buf = NULL;
    }
    if (stream->bmp_start) {
        free(stream->bmp_start);
        stream->bmp_start = NULL;
    }
    fclose(stream->file);
    stream->file = NULL;
}

static int8_t open_next_file(VideoStreamBytesStream *stream, char **next_file_name) {
    if (stream->next_file == stream->files_size) {
        return EOF;
    }

    int8_t res = LPX_SUCCESS;

    char *filename = stream->file_paths[stream->next_file++];
    stream->file = fopen(filename, "rb");
    if (stream->file == NULL) {
        return LPX_IO;
    }

    *next_file_name = filename;

    return res;
}

/**
 * Возвращает LPX_SUCCESS, если данные были успешно записаны в пайп, EOF, если стрим закончился, LPX_IO, если случилась
 * ошибка ввода-вывода. Количество прочитанных байт записывается в read.
 */
static int8_t read_part(VideoStreamBytesStream *stream, uint8_t *buf, size_t size, size_t *read) {
    *read = 0;

    if (stream->header_read == false) {
        uint32_t fsize = (uint32_t) stream->files_size;
        size_t files_cnt_size = sizeof(uint32_t);
        memcpy(buf, &fsize, files_cnt_size);
        size -= files_cnt_size;
        buf += files_cnt_size;
        *read += files_cnt_size;
        stream->header_read = true;
    }

    int8_t res = LPX_SUCCESS;
    if (stream->file == NULL) {
        char *next_file_path;
        res = open_next_file(stream, &next_file_path);
        if (res != LPX_SUCCESS) {
            return res;
        }

        char *file_name = basename(next_file_path);
        size_t name_size = strlen(file_name) + 1;
        memcpy(buf, file_name, name_size);
        size -= name_size;
        buf += name_size;
        *read += name_size;

        struct stat st;
        int r = stat(next_file_path, &st);
        if (r != 0) {
            return LPX_IO;
        }

        size_t raw_buf_size = (size_t) st.st_size;
        stream->raw_buf = xmalloc(raw_buf_size);

        size_t raw_read = fread(stream->raw_buf, sizeof(uint8_t), raw_buf_size, stream->file);
        if (raw_read < raw_buf_size) {
            return LPX_IO;
        }
        
        size_t bmp_size;
        if (raw12_to_bmp(stream->raw_buf, 1280, 800, &stream->bmp_start, &bmp_size)) {
            return LPX_IO;
        }
        free(stream->raw_buf);
        stream->bmp = stream->bmp_start;
        stream->bmp_eof = stream->bmp + sizeof(uint8_t) * bmp_size;

        uint64_t fsize = (uint64_t) bmp_size;
        size_t fsize_size = sizeof(fsize);

        memcpy(buf, &fsize, fsize_size);
        size -= fsize_size;
        buf += fsize_size;
        *read += fsize_size;
    }

    size_t to_cpy = size < stream->bmp_eof - stream->bmp ? size : stream->bmp_eof - stream->bmp;
    memcpy(buf, stream->bmp, to_cpy);
    stream->bmp += to_cpy;
    if (to_cpy < size) {
        close_current_file(stream);
    }

    *read += to_cpy;

    return res;
}

ssize_t stream_read(VideoStreamBytesStream *stream, uint8_t *buf, size_t max) {
    size_t available = max;
    while (available > 0) {
        size_t read = 0;
        int8_t res = read_part(stream, buf, available, &read);
        available -= read;
        assert(available >= 0);
        if (res == LPX_SUCCESS) {
            buf += read;
            continue;
        } else if (res == EOF) {
            if (max == available) {
                return EOF;
            } else {
                break;
            }
        } else if (res == LPX_IO) {
            return STRM_IO;
        }
    }
    return max - available;
}

void stream_close(VideoStreamBytesStream *stream) {
    if (stream->raw_buf) {
        free(stream->raw_buf);
    }
    if (stream->bmp_start) {
        free(stream->bmp_start);
    }
    if (stream->file) {
        fclose(stream->file);
    }
    for (int i = 0; i < stream->files_size; i++) {
        free(stream->file_paths[i]);
    }
    free(stream->file_paths);
    free(stream);
}

