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
#include "../include/webcam.h"
#include "unistd.h"
#include "../include/bus.h"

/*
 * Структура событий (прерываний) БУСа
 */
typedef struct Event {
    int8_t status; // статус чтения прерывания, 0 - ок, 3 - ошибка
    int8_t code; // код прерывания, 0 - поезд ушёл, 1 - сигнал дальнего оповещения, 3 - прошло первое колесо
} Event;

// пайп передачи данных от потка БУСа в главный поток
static int events_pipe[2];

static void ec(void *user_data, int errcode) {
    fprintf(stderr, "Webcam error, user_data: %s, code: %d\n", (char *) user_data, errcode);
}

static void bus_cb(void *user_data, int8_t status, int8_t code) {
    printf("bus event\n");
    Event e = {.status = status, .code = code};
    ssize_t r = write(events_pipe[1], &e, sizeof(e));
    assert(r == sizeof(e));
}

int main(int argc, char **argv) {
    char *storage_dir = NULL;
    char *dev = "/dev/video0";
    int c;

    while ((c = getopt(argc, argv, "s:d:")) != -1) {
        switch (c) {
            case 's':
                storage_dir = optarg;
                break;
            case 'd':
                dev = optarg;
                break;
            case '?':
                continue;
            default:
                abort();
        }
    }
    if (storage_dir == NULL) {
        fprintf(stderr, "Usage: lpx-control -s <storage dir> [-d <device>]");
        return 1;
    }

    int r = pipe(events_pipe);
    if (r < 0) {
        perror("Events pipe");
        exit(1);
    }

    Storage *s;
    storage_open(storage_dir, &s);

    Webcam *w;
    if (LPX_SUCCESS != webcam_init(s, dev, &w, NULL, ec)) {
        printf("webcam error\n");
        goto close_storage;
    }

    struct timeval time;
    gettimeofday(&time, NULL);
    char *train_id = itoa(tv2ms(time));
    printf("Starting streaming\n");
    r = webcam_start_stream(w, train_id);

    struct pollfd pfds[2];
    // поллим стандартный ввод, для корректного завершения программы по нажатию любой клавиши
    pfds[0].fd = 0;
    pfds[0].events = POLLIN;
    pfds[1].fd = events_pipe[0];
    pfds[1].events = POLLIN;

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

        // сейчас эти события никогда не прилетают
        // раньше они прилетали от БУСа, а в будщем планируется, что они будут прилитать от чего-то по GPIO
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
            free(train_id);
        }
    }

    close_webcam:
    webcam_close(w);

    close_storage:
    storage_close(s);
}

