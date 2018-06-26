#include <stdint.h>
#include <stdlib.h>
#include <archive.h>
#include <lpxstd.h>
#include <memory.h>
#include <archive_entry.h>
#include <libgen.h>
#include <stdbool.h>
#include <poll.h>
#include "../include/stream.h"

typedef struct StreamArchiveStream {
    /**
     * Записываемый архив
     */
    struct archive *archive;

    /**
     * Абсолютные пути к файлам, которые должны попасть в архив.
     */
    char **files;
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
     * Ентри архива записываемая в данный момент
     */
    struct archive_entry *entry;

    /**
     * libarchive использует "push" модель - мы в неё "проталкиваем" входные байты, а она "сливает" байты архива
     * на диск, в память или коллбэк.
     *
     * microhttpd использует "pull" модель - он из нас "вытягивает" входные данный в буффер в памяти, и потом сливает
     * этот буффер в ХТТП ответ.
     *
     * Для того чтобы подружить эти две модели используется пайп. Когда к нам приходит microhttpd за куском данных,
     * мы начинаем читать данные с диска и проталкивать их в пайп, пока архиватор не запишет в пайп нужное серверку кол-во данных,
     * после чего мы вытягиваем данные из пайпа и отдаём их серверу.
     */
    int *pipe;
    size_t pipe_size;

    /**
     * Флаг конца архива. Устанавливается в true, когда все файлы добавлены в архив и архив закрыт
     * (в случае зипа записан центральный каталог)
     */
    bool eof;
} StreamArchiveStream;

static int archive_open_cb(struct archive *archive, void *_client_data) {
    return ARCHIVE_OK;
}

static la_ssize_t archive_write_cb(struct archive *archive,
                                   void *_client_data,
                                   const void *_buffer, size_t _length) {
    StreamArchiveStream *stream = _client_data;
    ssize_t written = write(stream->pipe[1], _buffer, _length);
    stream->pipe_size += written;
    return written;
}

int archive_close_cb(struct archive *archive, void *_client_data) {
    StreamArchiveStream *stream = _client_data;
    stream->eof = true;
    return ARCHIVE_OK;
}

static void free_stream(StreamArchiveStream *stream);

StreamArchiveStream *stream_create_archive_stream(struct archive *archive, char **files, size_t files_size) {
    StreamArchiveStream *res = xcalloc(1, sizeof(StreamArchiveStream));
    res->archive = archive;
    res->files = files;
    res->files_size = files_size;
    res->next_file = 0;
    res->file = NULL;
    res->pipe = xcalloc(2, sizeof(int));
    pipe(res->pipe);
    res->pipe_size = 0;
    res->eof = false;

    int r = archive_write_open(archive, res, archive_open_cb, archive_write_cb, archive_close_cb);
    if (r != ARCHIVE_OK) {
        free_stream(res);
        return NULL;
    }

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

static void close_current_file(StreamArchiveStream *stream) {
    fclose(stream->file);
    stream->file = NULL;
    archive_entry_free(stream->entry);
}

static int8_t stream_finish(StreamArchiveStream *stream) {
    int r1 = archive_write_close(stream->archive);
    return (int8_t) (r1 == ARCHIVE_OK ? LPX_SUCCESS : LPX_IO);
}

static int8_t fill_pipe(StreamArchiveStream *stream, size_t size) {
    int8_t res = LPX_SUCCESS;
    if (stream->file == NULL) {
        if (stream->next_file == stream->files_size) {
            size_t cur_size = stream->pipe_size;
            res = stream_finish(stream);
            if (res != LPX_SUCCESS) {
                return res;
            }
            return (int8_t) (stream->pipe_size > cur_size ? LPX_SUCCESS : EOF);
        }
        char *filename = stream->files[stream->next_file++];
        stream->file = fopen(filename, "rb");
        if (stream->file == NULL) {
            return STRM_IO;
        }
        struct stat st;
        stat(filename, &st);
        stream->entry = archive_entry_new();
        char *file = basename(filename);
        archive_entry_set_pathname(stream->entry, file);
        free(filename);
        archive_entry_set_size(stream->entry, st.st_size);
        archive_entry_set_filetype(stream->entry, AE_IFREG);
        archive_entry_set_perm(stream->entry, 0644);
        int r = archive_write_header(stream->archive, stream->entry);
        if (r != ARCHIVE_OK) {
            close_current_file(stream);
            return STRM_IO;
        }
    }
    uint8_t *rbuf = xmalloc(size);
    size_t len = fread(rbuf, sizeof(uint8_t), size, stream->file);
    if (len < size) {
        if (feof(stream->file)) {
            close_current_file(stream);
        } else {
            res = STRM_IO;
            goto free_rbuf;
        }
    }
    ssize_t written = archive_write_data(stream->archive, rbuf, len);
    if (written < 0) {
        res = LPX_IO;
    }

    free_rbuf:
    free(rbuf);
    return res;
}

ssize_t stream_read(StreamArchiveStream *stream, uint8_t *buf, size_t max) {
    while (stream->pipe_size < max && stream->eof == false) {
        int8_t res = fill_pipe(stream, max);
        if (res == LPX_SUCCESS) {
            continue;
        } else if (res == EOF) {
            break;
        } else if (res == STRM_IO) {
            return res;
        }
    }
    if (stream->pipe_size == 0) {
        return EOF;
    }

    ssize_t readed = read(stream->pipe[0], buf, max);
    if (readed < 0) {
        return LPX_IO;
    }
    stream->pipe_size -= readed;
    return readed;
}

static void free_stream(StreamArchiveStream *stream) {
    close(stream->pipe[0]);
    close(stream->pipe[0]);;
    free(stream->files);
    free(stream->pipe);
    free(stream);
}

void stream_close(StreamArchiveStream *archive_stream) {
    archive_write_free(archive_stream->archive);
    free_stream(archive_stream);
}

