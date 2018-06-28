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
#include "../include/stream.h"

typedef struct VideoStreamBytesStream {
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

} VideoStreamBytesStream;

VideoStreamBytesStream *stream_open(char **files, size_t files_size) {
    VideoStreamBytesStream *res = xcalloc(1, sizeof(VideoStreamBytesStream));
    res->file_paths = files;
    res->files_size = files_size;
    res->next_file = 0;
    res->file = NULL;

    return res;
}

int32_t stream_find_frame(FrameMeta **index, uint32_t index_len, uint64_t time) {
    int64_t stream_base = index[0]->start_time;
    for (int i = 0; i < index_len - 1; i++) {
        int64_t frameOffset = index[i]->start_time - stream_base;
        int64_t nextFrameOffset = index[i + 1]->start_time - stream_base;
        if (labs(nextFrameOffset - time) > labs(frameOffset - time)) {
            return i;
        }
    }
    return -1;
}

static void close_current_file(VideoStreamBytesStream *stream) {
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
    int8_t res = LPX_SUCCESS;
    if (stream->file == NULL) {
        char *next_file_path;
        res = open_next_file(stream, &next_file_path);
        if (res != LPX_SUCCESS) {
            return res;
        }

        char *file_name = basename(next_file_path);
        size_t name_len = strlen(file_name) + 1;
        memcpy(buf, file_name, name_len);
        size -= name_len;
        buf += name_len;
        *read += name_len;
        
        struct stat st;
        int r = stat(next_file_path, &st);
        if (r != 0) {
            return LPX_IO;
        }
        uint64_t fsize = (uint64_t) st.st_size;
        size_t size_len = sizeof(fsize);
        
        memcpy(buf, &fsize, size_len);
        size -= size_len;
        buf += size_len;
        *read += size_len;
    }

    size_t len = fread(buf, sizeof(uint8_t), size, stream->file);
    if (len < size) {
        if (feof(stream->file)) {
            close_current_file(stream);
        } else {
            res = LPX_IO;
        }
    }

    *read += len;

    return res;
}

ssize_t stream_read(VideoStreamBytesStream *stream, uint8_t *buf, size_t max) {
    size_t available = max;
    while (available > 0) {
        size_t read = 0;
        int8_t res = read_part(stream, buf, available, &read);
        if (res == LPX_SUCCESS) {
            buf += read;
            available -= read;
            assert(available >= 0);
            continue;
        } else if (res == EOF) {
            return EOF;
        } else if (res == LPX_IO) {
            return STRM_IO;
        }
    }
    return max - available;
}

void stream_close(VideoStreamBytesStream *stream) {
    if (stream->file) {
        fclose(stream->file);
    }
    for (int i = 0; i < stream->files_size; i++) {
        free(stream->file_paths[i]);
    }
    free(stream->file_paths);
    free(stream);
}

