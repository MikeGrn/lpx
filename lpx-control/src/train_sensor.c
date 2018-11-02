#include <stdio.h>
#include <memory.h>
#include <stdbool.h>
#include <pthread.h>
#include <assert.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <zconf.h>
#include <fcntl.h>
#include "../include/train_sensor.h"
#include "lpxstd.h"
#include "../include/wiringPi.h"

typedef struct TrainSensor {
    void *user_data; // пользовательские данные, передаваемые в коллбэк
    train_sensor_callback icb;
    bool running;
    pthread_mutex_t running_mutex;
    pthread_t tid;
} TrainSensor;

static bool isRunning(TrainSensor *train_sensor) {
    bool ret = 0;
    int r = pthread_mutex_lock(&train_sensor->running_mutex);
    assert(r == 0 && "Could not not lock running_mutex");
    ret = train_sensor->running;
    r = pthread_mutex_unlock(&train_sensor->running_mutex);
    assert(r == 0 && "Could not not unlock running_mutex");
    return ret;
}

static int interrupt_pipe[2];
static uint8_t flag = 1;

static void handle_interrupt() {
    printf("Interrupt!\n");
    ssize_t r = write(interrupt_pipe[1], &flag, sizeof(flag));
    assert(r == sizeof(flag));
}

static void *handle_device(void *ts) {
    printf("TrainSensor thread started\n");
    TrainSensor *train_sensor = ts;
    wiringPiISR(0, INT_EDGE_RISING, handle_interrupt);

    struct pollfd pfds[1];
    pfds[0].fd = interrupt_pipe[0];
    pfds[0].events = POLLIN;
    while (true) {

        poll(pfds, 1, -1);
        if (pfds[0].revents != POLLIN) {
            printf("re: %d\n", pfds[0].revents);
            continue;
        }

        read(interrupt_pipe[0], &flag, sizeof(flag));
        if (!isRunning(ts)) {
            break;
        }

        uint8_t value = 0;

        if (digitalRead(0)) {
            value = 1;
        } else {
            value = 0;
        }

        if (train_sensor->icb) {
            train_sensor->icb(train_sensor->user_data, LPX_SUCCESS, value);
        }

        sleep(1);
    }
    return NULL;
}

int32_t train_sensor_init(TrainSensor **train_sensor, void *user_data, train_sensor_callback tscb) {
    int8_t res = LPX_SUCCESS;

    int r = pipe(interrupt_pipe);
    if (r < 0) {
        perror("Interrupt pipe");
        return TS_THREAD;
    }

    wiringPiSetup();
    pinMode(0, INPUT);
    pullUpDnControl(0, PUD_DOWN);

    *train_sensor = xmalloc(sizeof(TrainSensor));
    memset(*train_sensor, 0, sizeof(TrainSensor));
    TrainSensor *ts = *train_sensor;
    ts->user_data = user_data;
    ts->running = 1;
    ts->icb = tscb;

    r = pthread_mutex_init(&ts->running_mutex, NULL);
    if (0 != r) {
        res = TS_THREAD;
        fprintf(stderr, "Could not initialize train_sensor running_mutex, errcode: %d\n", r);
        goto free_train_sensor;
    }

    pthread_t tid;
    r = pthread_create(&tid, NULL, handle_device, ts);
    if (0 != r) {
        res = TS_THREAD;
        fprintf(stderr, "Could not create train_sensor thread, errcode: %d\n", r);
        goto destroy_mutex;
    }
    ts->tid = tid;

    return res;

    // завершение по ошибке
    destroy_mutex:
    pthread_mutex_destroy(&ts->running_mutex);

    free_train_sensor:
    free(*train_sensor);

    return res;
}

static void stop(TrainSensor *train_sensor) {
    ssize_t r = pthread_mutex_lock(&train_sensor->running_mutex);
    assert(r == 0 && "Could not not lock running_mutex");
    train_sensor->running = 0;
    r = write(interrupt_pipe[1], &flag, sizeof(flag));
    assert(r == sizeof(flag));
    r = pthread_mutex_unlock(&train_sensor->running_mutex);
    printf("Stop flag is set\n");
    assert(r == 0 && "Could not not unlock running_mutex");
}

void train_sensor_close(TrainSensor *train_sensor) {
    stop(train_sensor);
    pthread_join(train_sensor->tid, NULL);
    free(train_sensor);
}