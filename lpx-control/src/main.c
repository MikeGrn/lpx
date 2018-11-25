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
#include "../include/camera.h"
#include "unistd.h"
#include "../include/train_sensor.h"

/*
 * Структура событий сенсора поезада
 */
typedef struct Event {
    int8_t status; // статус чтения сенсора, 0 - ок, 3 - ошибка
    int8_t code; // статус поезда, 0 - поезд ушёл, 1 - сигнал дальнего оповещения
} Event;

// пайп передачи данных от потка сенсора в главный поток
static int events_pipe[2];

static void ec(void *user_data, int errcode) {
    fprintf(stderr, "Camera error, user_data: %s, code: %d\n", (char *) user_data, errcode);
}

static void train_sensor_cb(void *user_data, int8_t status, int8_t code) {
    printf("train sensor event\n");
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

    Camera *cam;
    if (LPX_SUCCESS != camera_init(s, &cam, NULL, ec)) {
        printf("camera error\n");
        goto close_storage;
    }

    TrainSensor *ts;
    if (LPX_SUCCESS != train_sensor_init(&ts, NULL, train_sensor_cb)) {
        printf("train sensor error\n");
        goto close_camera;
    }
    struct timeval time;
    gettimeofday(&time, NULL);
    char *train_id = itoa(tv2ms(time));
/*    printf("Starting streaming\n");
    r = camera_start_stream(w, train_id);*/

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

        if (e.code == TS_INT_TRAIN_IN) {
            struct timeval time;
            gettimeofday(&time, NULL);
            train_id = itoa(tv2ms(time));
            printf("Starting streaming\n");
            r = camera_start_stream(cam, train_id);
            if (r != LPX_SUCCESS) {
                printf("Streaming error: %d\n", r);
            }
        } else if (e.code == TS_INT_TRAIN_LEAVE) {
            if (!camera_streaming(cam)) {
                continue;
            }
            camera_stop_stream(cam);
            if (train_id) {
                free(train_id);
                train_id = NULL;
            }
        }
        printf("\n");
    }

    close_train_sensor:
    train_sensor_close(ts);

    close_camera:
    camera_close(cam);

    close_storage:
    storage_close(s);
}

