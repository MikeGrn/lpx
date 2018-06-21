#include <stdio.h>

#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <microhttpd.h>
#include <memory.h>
#include <stdlib.h>

#define PORT 8888

static int print_out_key(void *cls, enum MHD_ValueKind kind, const char *key,
                         const char *value) {
    printf("%s: %s\n", key, value);
    return MHD_YES;
}

static int handle_stream(struct MHD_Connection *connection, const char *url, const char *method) {
    const char *time = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "time");
    if (time == NULL) {

        const char *page = "time get parameter expected";
        struct MHD_Response *response;
        int ret;

        response = MHD_create_response_from_buffer(strlen(page), (void *) page, MHD_RESPMEM_PERSISTENT);
        ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
        MHD_destroy_response(response);

        return ret;
    }


}

static int answer_to_connection(void *cls, struct MHD_Connection *connection,
                         const char *url,
                         const char *method, const char *version,
                         const char *upload_data,
                         size_t *upload_data_size, void **con_cls) {
    if (strcmp(method, "GET") != 0) {
        return MHD_NO;
    }
    if (NULL == *con_cls) {
        *con_cls = connection;
        return MHD_YES;
    }

    if (strcmp(url, "/stream") == 0) {
        return handle_stream(connection, url, method);
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
    struct MHD_Daemon *daemon;

    daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, PORT, NULL, NULL,
                              &answer_to_connection, NULL, MHD_OPTION_END);
    if (NULL == daemon) {
        return 1;
    }
    getchar();

    MHD_stop_daemon(daemon);
    return 0;
}