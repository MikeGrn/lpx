#include <stdio.h>
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>
#include <poll.h>
#include <assert.h>
#include <stdbool.h>
#include "list.h"
#include "lpxstd.h"
#include "stream_storage.h"
#include "webcam2.h"
#include "unistd.h"
#include "bus2.h"

typedef struct Event {
    int8_t status;
    int8_t code;
} Event;

static int events_pipe[2];

bool webcam_streaming(Webcam *pWebcam);

static void ec(void *user_data, int errcode) {
    fprintf(stderr, "Webcam error, user_data: %s, code: %d\n", (char *) user_data, errcode);
}

static void bus_cb(void *user_data, int8_t status, int8_t code) {
    printf("bus event\n");
    Event e = {.status = status, .code = code};
    ssize_t r = write(events_pipe[1], &e, sizeof(e));
    assert(r == sizeof(e));
}

int main() {
    int r = pipe(events_pipe);
    if (r < 0) {
        perror("Events pipe");
        exit(1);
    }

    Storage *s;
    storage_open("/home/azhidkov/tmp/lpx-out", &s);

    Webcam *w;
    webcam_init(s, "/dev/video0", &w, "user_data", ec);

    Bus *b;
    if (LPX_SUCCESS != bus_init(&b, w, bus_cb)) {
        printf("bus error\n");
        goto cleanup;
    }

    struct pollfd pfds[2];
    pfds[0].fd = 0;
    pfds[0].events = POLLIN;
    pfds[1].fd = events_pipe[0];
    pfds[1].events = POLLIN;

    char *train_id;
    while (1) {
        Event e = {0};
        r = poll(pfds, ALEN(pfds), -1);
        if (pfds[0].revents == POLLIN) {
            break;
        }
        if (pfds[1].revents != POLLIN) {
            printf("re: %d\n", pfds[1].revents);
            continue;
        }
        read(events_pipe[0], &e, sizeof(e));
        printf("%d code: %d\n", e.status, e.code);
        if (e.status != LPX_SUCCESS) {
            break;
        }

        if (e.code == BUS_INT_TRAIN_IN) {
            struct timeval time;
            gettimeofday(&time, NULL);
            train_id = itoa(tv2ms(time));
            printf("Starting streaming\n");
            r = webcam_start_stream(w, train_id);
            if (r != LPX_SUCCESS) {
                printf("Streaming error: %d\n", r);
            }
        } else if (e.code == BUS_INT_TRAIN_LEAVE) {
            if (!webcam_streaming(w)) {
                continue;
            }
            webcam_stop_stream(w);
            uint64_t *wheel_offsets = NULL;
            uint32_t wheel_offsets_len = 0;
            bus_last_train_wheel_time_offsets(b, &wheel_offsets, &wheel_offsets_len);
            for (int i = 0; i< wheel_offsets_len; i++) {
                FrameMeta **index = 0;
                uint32_t index_len = 0;
                storage_read_stream_idx(s, train_id, &index, &index_len);
                int32_t frame_idx = stream_find_frame(index, index_len, wheel_offsets[i]);
                printf("%ld %d\n", wheel_offsets[i], frame_idx);
            }
            free(wheel_offsets);
            free(train_id);
        }
    }

    close_bus:
    bus_close(b);

    cleanup:
    webcam_close(w);

    storage_close(s);
}

