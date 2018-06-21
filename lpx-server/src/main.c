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

#define PORT 8888

typedef struct LpxServer {
    Storage *storage;
    List *garbage;
} LpxServer;

static int print_out_key(void *cls, enum MHD_ValueKind kind, const char *key,
                         const char *value) {
    printf("%s: %s\n", key, value);
    return MHD_YES;
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
    const char *timeStr = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "time");
    if (timeStr == NULL) {
        return bad_request(connection, "time get parameter expected");
    }

    int64_t time = strtoll(timeStr, NULL, 10);
    if (time == LLONG_MIN || time == LLONG_MAX) {
        return bad_request(connection, "invalid time get parameter expected");
    }

    char *stream = NULL;
    storage_find_stream(lpx->storage, time, &stream);
    if (stream == NULL) {
        return not_found(connection);
    }

    char *fname = xcalloc(L_tmpnam, sizeof(char));
    fname = tmpnam(fname);
    if (fname == NULL) {
        goto internal_error;
    }

    int8_t r = storage_compress(lpx->storage, stream, fname);
    if (r != LPX_SUCCESS) {
        goto internal_error;
    }

    struct MHD_Response *response;

    int fd = open(fname, O_RDONLY);
    off_t size;
    fd_size(fd, &size);
    response = MHD_create_response_from_fd((size_t) size, fd);
    MHD_add_response_header(response, "Content-Type", "application/zip");
    char *filename = xcalloc(1024, sizeof(char));
    sprintf(filename, "attachment; filename=\"%s.zip\"", stream);
    MHD_add_response_header(response, "Content-Disposition", filename);
    free(filename);
    int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    lst_append(lpx->garbage, fname);

    free(stream);

    return ret;

    internal_error:
    free(stream);

    close(fd);

    return internal_error(connection);
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
    List *garbage = lst_create();
    storage_open("/home/azhidkov/tmp/lpx-out", &storage);
    LpxServer lpx = {.storage = storage, .garbage = garbage};
    struct MHD_Daemon *daemon;

    daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, PORT, NULL, NULL,
                              &answer_to_connection, &lpx, MHD_OPTION_END);
    if (NULL == daemon) {
        return 1;
    }
    getchar();

    MHD_stop_daemon(daemon);
    storage_close(storage);

    uint32_t tmp_files_size = lst_size(lpx.garbage);
    char **tmp_files = xcalloc(sizeof(char *), tmp_files_size);
    lst_to_array(lpx.garbage, (void **) tmp_files);
    for (int i = 0; i < tmp_files_size; i++) {
        char *tmp_file = tmp_files[i];
        unlink(tmp_file);
        free(tmp_file);
    }
    free(tmp_files);
    lst_free(lpx.garbage);

    return 0;
}