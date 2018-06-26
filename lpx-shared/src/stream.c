#include <stdint.h>
#include <stdlib.h>
#include <archive.h>
#include <lpxstd.h>
#include <memory.h>
#include <archive_entry.h>
#include <libgen.h>
#include "../include/stream.h"

typedef struct StreamArchiveStream {
    struct archive *archive;
    char **files;
    size_t files_size;
    uint32_t next_file;
    FILE *file;
    struct archive_entry *entry;
    int *pipe;
    archive_open_callback *open_cb;
    archive_write_callback *write_cb;
    archive_close_callback *close_cb;
} StreamArchiveStream;

static int archive_open_cb(struct archive *archive, void *_client_data) {
    return ARCHIVE_OK;
}

static la_ssize_t archive_write_cb(struct archive *archive,
                                   void *_client_data,
                                   const void *_buffer, size_t _length) {
    StreamArchiveStream *stream = _client_data;
    ssize_t written = write(stream->pipe[1], _buffer, _length);
    return written;
}

int archive_close_cb(struct archive *archive, void *_client_data) {
    return ARCHIVE_OK;
}

StreamArchiveStream *stream_create_archive_stream(struct archive *archive, char **files, size_t files_size) {
    StreamArchiveStream *res = xcalloc(1, sizeof(StreamArchiveStream));
    res->archive = archive;
    res->files = files;
    res->files_size = files_size;
    res->next_file = 0;
    res->file = NULL;
    res->pipe = xcalloc(2, sizeof(int));
    pipe(res->pipe);
    res->open_cb = archive_open_cb;
    res->write_cb = archive_write_cb;
    res->close_cb = archive_close_cb;
    return res;
}

void stream_archive_callbacks(StreamArchiveStream *stream, archive_open_callback **open_cb,
                              archive_write_callback **write_cb, archive_close_callback **close_cb) {
    *open_cb = stream->open_cb;
    *write_cb = stream->write_cb;
    *close_cb = stream->close_cb;
}

int *stream_pipe(StreamArchiveStream *stream) {
    return stream->pipe;
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

ssize_t stream_write_block(StreamArchiveStream *stream) {
    if (stream->file == NULL) {
        if (stream->next_file == stream->files_size) {
            return -1;
        }
        char *filename = stream->files[stream->next_file++];
        stream->file = fopen(filename, "rb");
        if (stream->file == NULL) {
            return -2;
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
            return -2;
        }
    }
    ssize_t res;
    size_t size = 10240;
    uint8_t *rbuf = xmalloc(size);
    size_t len = fread(rbuf, sizeof(uint8_t), size, stream->file);
    if (len < size) {
        if (feof(stream->file)) {
            close_current_file(stream);
        } else {
            res = -2;
            goto free_rbuf;
        }
    }
    res = archive_write_data(stream->archive, rbuf, len);
    free_rbuf:
    free(rbuf);
    return res;
}

void stream_free(StreamArchiveStream *stream) {
    free(stream->files);
    free(stream->pipe);
    free(stream);
}

void stream_finish(StreamArchiveStream *stream) {
    int r = archive_write_close(stream->archive);
    r = archive_write_free(stream->archive);
}

void stream_close(StreamArchiveStream *stream) {
    close(stream->pipe[0]);
    close(stream->pipe[0]);
    stream_free(stream);
}