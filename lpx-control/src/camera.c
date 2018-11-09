#include <fcntl.h>
#include <sys/mman.h>
#include <sys/eventfd.h>
#include <memory.h>
#include <stdbool.h>
#include <pthread.h>
#include "../include/camera.h"
#include "lpxstd.h"
#include "list.h"
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include "../include/raspiraw.h"

typedef struct Thread {
    Camera *camera;
    char *train_id;
    bool running;
    pthread_mutex_t running_mutex;
    uint32_t frame_index; // порядковый номер следующего фрейма
    struct timeval frame_req_time; // системное (астрономическое) время запроса следующего фрейма
    struct timeval frame_ready_time; // системное (астрономическое) время получения текущего фрейма
    int stop_fd; // дескриптор файла событий, через который передаётся сигнал на завершение работы потока
} Thread;


typedef struct Camera {
    Storage *storage;
    Raspiraw *raspiraw;
    void *user_data; // пользовательские данные, передаваемые в каллбэк
    error_callback ecb;
    int webcam_fd;
    uint8_t *frame_buffer;
    Thread *thread;
    pthread_t tid;
} Camera;

int8_t camera_init(Storage *storage, char *device, Camera **camera, void *user_data, error_callback ecb) {
    int8_t res = LPX_SUCCESS;
    *camera = xmalloc(sizeof(Camera));
    memset(*camera, 0, sizeof(Camera));
    Raspiraw *raspiraw;
    raspiraw_init(&raspiraw, storage);
    Camera *c = *camera;
    c->storage = storage;
    c->ecb = ecb;
    c->user_data = user_data;
    c->raspiraw = raspiraw;

/*    if ((w->webcam_fd = open(device, O_RDWR)) < 0) {
        res = LPX_IO;
        perror("open");
        goto error;
    }*/

    return res;

    // завершение по ошибке
    close_webcam:
    close(c->webcam_fd);

    error:
    free(*camera);
    *camera = NULL;

    return res;
}

static bool isRunning(Thread *thread) {
    bool ret = 0;
    int r = pthread_mutex_lock(&thread->running_mutex);
    assert(r == 0 && "Could not not lock running_mutex");
    ret = thread->running;
    r = pthread_mutex_unlock(&thread->running_mutex);
    assert(r == 0 && "Could not not unlock running_mutex");
    return ret;
}

static void stop(Thread *thread) {
    int r = pthread_mutex_lock(&thread->running_mutex);
    assert(r == 0 && "Could not not lock running_mutex");
    thread->running = 0;
    uint64_t f = 1;
    // будим poll в цикле потока
    r = (int) write(thread->stop_fd, &f, sizeof(f));
    assert(r != -1);
    r = pthread_mutex_unlock(&thread->running_mutex);
    printf("Stop flag is set\n");
    assert(r == 0 && "Could not not unlock running_mutex");
}

static void *webcam_handle_stream(void *t) {
    Thread *thread = t;
    Camera *camera = thread->camera;
    thread->frame_index = 0;
    List *frames = lst_create();

    struct pollfd pfd[1];
    pfd[0].fd = thread->stop_fd;
    pfd[0].events = POLLIN;

    raspiraw_start(camera->raspiraw);
    printf("started\n");
    while (isRunning(thread)) {
        int pr = poll(pfd, ALEN(pfd), -1);
        if (pr < 0) {
            camera->ecb(camera->user_data, errno);
            perror("Poll camera");
            break;
        }
        if (pfd[0].revents != pfd[0].events) {
            // позвали stop, isRunning вернёт true и поток завершится
            continue;
        }
        int r = gettimeofday(&thread->frame_ready_time, NULL);
        assert(r == 0);

/*        r = storage_store_frame(camera->storage, thread->train_id, thread->frame_index++, camera->frame_buffer,
                                camera->buffer_info.length);
        if (LPX_SUCCESS != r) {
            if (errno != 0) {
                perror("Frame writing");
            }
            camera->ecb(camera->user_data, r);
            fprintf(stderr, "Stream storage failed, errcode: %d\n", r);
            break;
        }

        FrameMeta *frame = xmalloc(sizeof(FrameMeta));
        memset(frame, 0, sizeof(FrameMeta));
        frame->start_time = tv2mks(thread->frame_req_time);
        frame->end_time = tv2mks(thread->frame_ready_time);
        lst_append(frames, frame);

        r = gettimeofday(&thread->frame_req_time, NULL);*/
    }

    raspiraw_stop(camera->raspiraw);

    size_t frames_cnt = lst_size(frames);
    FrameMeta **frame_array = frames_cnt == 0 ? NULL : xcalloc(frames_cnt, sizeof(FrameMeta *));
    if (frames_cnt > 0) {
        lst_to_array(frames, (const void **) frame_array);
        int8_t r = storage_store_stream_idx(camera->storage, thread->train_id, frame_array, frames_cnt);
        if (LPX_SUCCESS != r) {
            if (errno != 0) {
                perror("Stream index writing");
            }
            camera->ecb(camera->user_data, r);
            fprintf(stderr, "Stream storage failed, errcode: %d\n", r);
        }
    }

    free_array((void **) frame_array, frames_cnt);
    lst_free(frames);

    printf("exited\n");
    return NULL;
}

static void free_thread(Thread *thread) {
    free(thread->train_id);
    free(thread);
}

int8_t camera_start_stream(Camera *camera, char *train_id) {
    int8_t res = LPX_SUCCESS;
    if (LPX_SUCCESS != storage_prepare(camera->storage, train_id)) {
        return CAM_STRG;
    }
    Thread *thread = xmalloc(sizeof(Thread));
    memset(thread, 0, sizeof(Thread));
    camera->thread = thread;
    thread->running = 1;
    thread->camera = camera;
    raspiraw_set_train_id(camera->raspiraw, train_id);
    size_t train_id_size = strnlen(train_id, MAX_INT_LEN);
    thread->train_id = xcalloc(train_id_size + 1, sizeof(char));
    strncpy(thread->train_id, train_id, train_id_size);
    thread->stop_fd = eventfd(0, EFD_CLOEXEC);
    if (thread->stop_fd == -1) {
        res = LPX_IO;
        perror("Event fd");
        goto free_thread;
    }
    int r = pthread_mutex_init(&thread->running_mutex, NULL);
    if (0 != r) {
        res = CAM_THREAD;
        fprintf(stderr, "Could not initialize running_mutex, errcode: %d\n", r);
        goto free_thread;
    }

    r = gettimeofday(&thread->frame_req_time, NULL);

    pthread_t tid;
    r = pthread_create(&tid, NULL, webcam_handle_stream, thread);
    if (0 != r) {
        res = CAM_THREAD;
        fprintf(stderr, "Could not create thread, errcode: %d\n", r);
        goto stop_streaming;
    }
    camera->tid = tid;

    return res;

    // завершение по ошибке
    stop_streaming:

    destroy_mutex:
    r = pthread_mutex_destroy(&thread->running_mutex);
    assert(r == 0);

    free_thread:
    free_thread(camera->thread);
    camera->thread = NULL;
    camera->tid = 0;

    return res;
}

int8_t camera_stop_stream(Camera *webcam) {
    if (webcam->thread == NULL) {
        printf("not streaming ignore stop request\n");
        return LPX_SUCCESS;
    }

    stop(webcam->thread);
    int r = pthread_join(webcam->tid, NULL);
    assert(r == 0);
    r = close(webcam->thread->stop_fd);
    assert(r == 0);
    r = pthread_mutex_destroy(&webcam->thread->running_mutex);
    assert(r == 0);
    free_thread(webcam->thread);
    webcam->thread = NULL;

    return LPX_SUCCESS;
}

bool camera_streaming(Camera *camera) {
    return camera->thread != NULL;
}

void camera_close(Camera *camera) {
    camera_stop_stream(camera);
    close(camera->webcam_fd);
    free(camera);
}