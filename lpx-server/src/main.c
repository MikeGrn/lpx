#include <stdio.h>

#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <microhttpd.h>
#include <memory.h>
#include <stdlib.h>
#include <zconf.h>
#include <stream_storage.h>
#include <lpxstd.h>
#include <fcntl.h>
#include <list.h>
#include <assert.h>

#define PORT 8888

typedef struct LpxServer {
    Storage *storage;
} LpxServer;

typedef struct ValuesIter {
    List *res;
    char *key;
} ValuesIter;

static int list_parameters(void *cls, enum MHD_ValueKind kind, const char *key, const char *value) {
    ValuesIter *iter = cls;
    if (strcmp(key, iter->key) == 0) {
        lst_append(iter->res, value);
    }
    return MHD_YES;
}

static ssize_t stream_reader_callback(void *cls, uint64_t pos, char *buf, size_t max) {
    static uint64_t spos = 0;
    assert(pos == 0 || pos == spos);
    if (pos == 0) {
        spos = pos;
    }
    VideoStreamBytesStream *stream = cls;
    ssize_t res = stream_read(stream, (uint8_t *) buf, max);
    if (res > 0) {
        spos += res;
    }
    return res;
}

static void stream_close_callback(void *cls) {
    VideoStreamBytesStream *stream = cls;
    stream_close(stream);
}

static int bad_request(struct MHD_Connection *connection, char *msg) {
    struct MHD_Response *response;
    int ret;

    response = MHD_create_response_from_buffer(strlen(msg), (void *) msg, MHD_RESPMEM_PERSISTENT);
    ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
    MHD_destroy_response(response);

    return ret;
}

static int internal_error(struct MHD_Connection *connection) {
    struct MHD_Response *response;
    int ret;

    char *msg = "Internal error";
    response = MHD_create_response_from_buffer(strlen(msg), (void *) msg, MHD_RESPMEM_PERSISTENT);
    ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
    MHD_destroy_response(response);

    return ret;
}

static int not_found(struct MHD_Connection *connection) {
    struct MHD_Response *response;
    int ret;

    char *msg = "Not found";
    response = MHD_create_response_from_buffer(strlen(msg), (void *) msg, MHD_RESPMEM_PERSISTENT);
    ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
    MHD_destroy_response(response);

    return ret;
}

static int handle_stream(LpxServer *lpx, struct MHD_Connection *connection, const char *url, const char *method) {
    const char *stream_time_str = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "stream_time");
    if (stream_time_str == NULL) {
        return bad_request(connection, "stream_time GET parameter expected");
    }

    char *null;
    int64_t time = strtoll(stream_time_str, &null, 10);
    if (time == LLONG_MIN || time == LLONG_MAX || *null != 0) {
        return bad_request(connection, "invalid stream_time GET parameter expected");
    }

    char *stream_id = NULL;
    storage_find_stream(lpx->storage, time, &stream_id);

    // todo: free
    ValuesIter iter = {.res = lst_create(), .key = "frame_time"};
    MHD_get_connection_values(connection, MHD_GET_ARGUMENT_KIND, list_parameters, &iter);
    VideoStreamBytesStream *stream = NULL;
    size_t frames_cnt = lst_size(iter.res);
    if (frames_cnt > 0) {
        const char **frame_times_str = xcalloc(frames_cnt, sizeof(char *));
        lst_to_array(iter.res, (const void **) frame_times_str);

        uint64_t *frame_times = xcalloc(frames_cnt, sizeof(uint64_t));
        for (int i = 0; i < frames_cnt; i++) {
            int64_t sframe_time = strtoll(frame_times_str[i], &null, 10);
            if (sframe_time == LLONG_MIN || sframe_time == LLONG_MAX || *null != 0) {
                return bad_request(connection, "invalid frame_time GET parameter expected");
            }
            frame_times[i] = (uint64_t) sframe_time;
        }
        free(frame_times_str);
        qsort(frame_times, frames_cnt, sizeof(uint64_t), uint64_t_cmp);
        FrameMeta **index = NULL;
        size_t index_size = 0;
        storage_read_stream_idx(lpx->storage, stream_id, &index, &index_size);
        List *frame_indexes = lst_create();
        for (int i = 0; i < frames_cnt; i++) {
            ssize_t sidx = stream_find_frame(index, index_size, frame_times[i]);
            if (sidx < 0) {
                // запросили оффсет больше, чем конец последнего фрейма
                break;
            }
            size_t *idx = xmalloc(sizeof(size_t));
            *idx = (size_t) sidx;
            lst_append(frame_indexes, (const void *) idx);
        }
        free(frame_times);
        int8_t res = storage_open_stream_frames(lpx->storage, stream_id, frame_indexes, &stream);
        lst_deep_free(frame_indexes);
        free_array((void **) index, index_size);
        if (res != LPX_SUCCESS) {
            return internal_error(connection);
        }
        lst_free(iter.res);
    } else {
        size_t offset = 0;
        const char *offset_str = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "offset");
        if (offset_str == NULL) {
            offset = 0;
        } else {
            ssize_t soffset = strtoll(offset_str, &null, 10);
            if (soffset == LLONG_MIN || soffset == LLONG_MAX || *null != 0 || soffset < 0) {
                return bad_request(connection, "invalid offset GET parameter");
            }
            offset = (size_t) soffset;
        }

        if (stream_id == NULL) {
            return not_found(connection);
        }
        int8_t res = storage_open_stream(lpx->storage, stream_id, offset, &stream);
        if (res != LPX_SUCCESS) {
            return internal_error(connection);
        }
    }

    struct MHD_Response *response;

    response = MHD_create_response_from_callback(MHD_SIZE_UNKNOWN, 1024, stream_reader_callback, stream,
                                                 stream_close_callback);
    MHD_add_response_header(response, "Content-Type", "application/zip");
    char *filename = xcalloc(1024, sizeof(char));
    sprintf(filename, "attachment; filename=\"%s.bin\"", stream_id);
    MHD_add_response_header(response, "Content-Disposition", filename);
    free(filename);
    int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);

    free(stream_id);

    return ret;
}

static int answer_to_connection(void *cls, struct MHD_Connection *connection,
                                const char *url,
                                const char *method, const char *version,
                                const char *upload_data,
                                size_t *upload_data_size, void **con_cls) {
    LpxServer *lpx = cls;
    if (strcmp(method, "GET") != 0) {
        return MHD_NO;
    }
    if (NULL == *con_cls) {
        *con_cls = connection;
        return MHD_YES;
    }

    if (strcmp(url, "/stream") == 0) {
        return handle_stream(lpx, connection, url, method);
    } else {
        const char *page = "No such function";
        struct MHD_Response *response;
        int ret;

        response = MHD_create_response_from_buffer(strlen(page), (void *) page, MHD_RESPMEM_PERSISTENT);
        ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
        MHD_destroy_response(response);

        return ret;
    }
}

int main() {
    Storage *storage = NULL;
    storage_open("/home/azhidkov/tmp/lpx-out", &storage);
    LpxServer lpx = {.storage = storage};
    struct MHD_Daemon *daemon;

    daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, PORT, NULL, NULL,
                              &answer_to_connection, &lpx, MHD_OPTION_END);
    if (NULL == daemon) {
        return 1;
    }
    getchar();

    MHD_stop_daemon(daemon);
    storage_close(storage);

    return 0;
}