#include <fcntl.h>
#include <linux/videodev2.h>
#include <stropts.h>
#include <sys/mman.h>
#include <sys/eventfd.h>
#include <memory.h>
#include <stdbool.h>
#include <pthread.h>
#include "../include/webcam.h"
#include "lpxstd.h"
#include "list.h"
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <poll.h>

typedef struct Thread {
    Webcam *webcam;
    char *train_id;
    bool running;
    pthread_mutex_t running_mutex;
    uint32_t frame_index; // порядковый номер следующего фрейма
    struct timeval frame_req_time; // системное (астрономическое) время запроса следующего фрейма
    struct timeval frame_ready_time; // системное (астрономическое) время получения текущего фрейма
    int stop_fd; // дескриптор файла событий, через который передаётся сигнал на завершение работы потока
} Thread;

typedef struct Webcam {
    Storage *storage;
    void *user_data; // пользовательские данные, передаваемые в каллбэк
    error_callback ecb;
    int webcam_fd;
    uint8_t *frame_buffer;
    struct v4l2_buffer buffer_info;
    Thread *thread;
    pthread_t tid;
} Webcam;

int8_t webcam_init(Storage *storage, char *device, Webcam **webcam, void *user_data, error_callback ecb) {
    int8_t res = LPX_SUCCESS;
    *webcam = xmalloc(sizeof(Webcam));
    memset(*webcam, 0, sizeof(Webcam));
    Webcam *w = *webcam;
    w->storage = storage;
    w->ecb = ecb;
    w->user_data = user_data;

    if ((w->webcam_fd = open(device, O_RDWR)) < 0) {
        res = LPX_IO;
        perror("open");
        goto error;
    }

    struct v4l2_capability cap = {0};
    if (ioctl(w->webcam_fd, VIDIOC_QUERYCAP, &cap) < 0) {
        res = WC_CAP;
        perror("VIDIOC_QUERYCAP");
        goto close_webcam;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        res = WC_CAP;
        fprintf(stderr, "The device does not handle single-planar video capture.\n");
        goto close_webcam;
    }

    struct v4l2_format format = {0};
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    format.fmt.pix.width = 640;
    format.fmt.pix.height = 480;
    format.fmt.pix.field = V4L2_FIELD_NONE;
    if (ioctl(w->webcam_fd, VIDIOC_S_FMT, &format) < 0) {
        res = LPX_IO;
        perror("VIDIOC_S_FMT");
        goto close_webcam;
    }

    struct v4l2_requestbuffers buf_request = {0};
    buf_request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf_request.memory = V4L2_MEMORY_MMAP;
    buf_request.count = 1;
    if (ioctl(w->webcam_fd, VIDIOC_REQBUFS, &buf_request) < 0) {
        res = LPX_IO;
        perror("VIDIOC_REQBUFS");
        goto close_webcam;
    }

    w->buffer_info.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    w->buffer_info.memory = V4L2_MEMORY_MMAP;
    w->buffer_info.index = 0;
    if (ioctl(w->webcam_fd, VIDIOC_QUERYBUF, &w->buffer_info) < 0) {
        res = LPX_IO;
        perror("VIDIOC_QUERYBUF");
        goto close_webcam;
    }

    w->frame_buffer = mmap(NULL, w->buffer_info.length, PROT_READ | PROT_WRITE, MAP_SHARED, w->webcam_fd,
                           w->buffer_info.m.offset);
    if (w->frame_buffer == MAP_FAILED) {
        res = LPX_IO;
        perror("mmap");
        goto close_webcam;
    }

    return res;

    // завершение по ошибке
    close_webcam:
    close(w->webcam_fd);

    error:
    free(*webcam);
    *webcam = NULL;

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
    Webcam *webcam = thread->webcam;
    thread->frame_index = 0;
    List *frames = lst_create();

    struct pollfd pfd[2];
    pfd[0].fd = webcam->webcam_fd;
    pfd[0].events = POLLIN;
    pfd[1].fd = thread->stop_fd;
    pfd[1].events = POLLIN;

    printf("started\n");
    while (isRunning(thread)) {
        int pr = poll(pfd, ALEN(pfd), -1);
        if (pr < 0) {
            webcam->ecb(webcam->user_data, errno);
            perror("Poll webcam");
            break;
        }
        if (pfd[0].revents != pfd[0].events) {
            // позвали stop, isRunning вернёт true и поток завершится
            continue;
        }
        if (-1 == ioctl(webcam->webcam_fd, VIDIOC_DQBUF, &webcam->buffer_info)) {
            webcam->ecb(webcam->user_data, errno);
            perror("Retrieving Frame");
            break;
        }
        int r = gettimeofday(&thread->frame_ready_time, NULL);
        assert(r == 0);

        r = storage_store_frame(webcam->storage, thread->train_id, thread->frame_index++, webcam->frame_buffer,
                                webcam->buffer_info.length);
        if (LPX_SUCCESS != r) {
            if (errno != 0) {
                perror("Frame writing");
            }
            webcam->ecb(webcam->user_data, r);
            fprintf(stderr, "Stream storage failed, errcode: %d\n", r);
            break;
        }

        FrameMeta *frame = xmalloc(sizeof(FrameMeta));
        memset(frame, 0, sizeof(FrameMeta));
        frame->start_time = tv2mks(thread->frame_req_time);
        frame->end_time = tv2mks(thread->frame_ready_time);
        lst_append(frames, frame);

        r = gettimeofday(&thread->frame_req_time, NULL);
        assert(r == 0);
        if (ioctl(webcam->webcam_fd, VIDIOC_QBUF, &webcam->buffer_info) < 0) {
            webcam->ecb(webcam->user_data, errno);
            perror("VIDIOC_QBUF");
            break;
        }
    }

    if (ioctl(webcam->webcam_fd, VIDIOC_STREAMOFF, &webcam->buffer_info.type) < 0) {
        webcam->ecb(webcam->user_data, errno);
        perror("VIDIOC_STREAMOFF");
    }

    uint32_t frames_cnt = lst_size(frames);
    FrameMeta **frame_array = frames_cnt == 0 ? NULL : xcalloc(frames_cnt, sizeof(FrameMeta *));
    if (frames_cnt > 0) {
        lst_to_array(frames, (void **) frame_array);
        int8_t r = storage_store_stream_idx(webcam->storage, thread->train_id, frame_array, frames_cnt);
        if (LPX_SUCCESS != r) {
            if (errno != 0) {
                perror("Stream index writing");
            }
            webcam->ecb(webcam->user_data, r);
            fprintf(stderr, "Stream storage failed, errcode: %d\n", r);
        }
    }

    for (int i = 0; i < frames_cnt; i++) {
        free(frame_array[i]);
    }
    free(frame_array);
    lst_free(frames);

    printf("exited\n");
    return NULL;
}

static void free_thread(Thread *thread) {
    free(thread->train_id);
    free(thread);
}

int8_t webcam_start_stream(Webcam *webcam, char *train_id) {
    int8_t res = LPX_SUCCESS;
    if (LPX_SUCCESS != storage_prepare(webcam->storage, train_id)) {
        return WC_STRG;
    }
    Thread *thread = xmalloc(sizeof(Thread));
    memset(thread, 0, sizeof(Thread));
    webcam->thread = thread;
    thread->running = 1;
    thread->webcam = webcam;
    size_t train_id_len = strnlen(train_id, MAX_INT_LEN);
    thread->train_id = xcalloc(train_id_len + 1, sizeof(char));
    strncpy(thread->train_id, train_id, train_id_len);
    thread->stop_fd = eventfd(0, EFD_CLOEXEC);
    if (thread->stop_fd == -1) {
        res = LPX_IO;
        perror("Event fd");
        goto free_thread;
    }
    int r = pthread_mutex_init(&thread->running_mutex, NULL);
    if (0 != r) {
        res = WC_THREAD;
        fprintf(stderr, "Could not initialize running_mutex, errcode: %d\n", r);
        goto free_thread;
    }

    if (-1 == ioctl(webcam->webcam_fd, VIDIOC_STREAMON, &webcam->buffer_info.type)) {
        res = LPX_IO;
        perror("Start Capture");
        goto destroy_mutex;
    }

    r = gettimeofday(&thread->frame_req_time, NULL);
    assert(r == 0);
    if (ioctl(webcam->webcam_fd, VIDIOC_QBUF, &webcam->buffer_info) < 0) {
        res = LPX_IO;
        perror("Start Capture");
        goto stop_streaming;
    }

    pthread_t tid;
    r = pthread_create(&tid, NULL, webcam_handle_stream, thread);
    if (0 != r) {
        res = WC_THREAD;
        fprintf(stderr, "Could not create thread, errcode: %d\n", r);
        goto stop_streaming;
    }
    webcam->tid = tid;

    return res;

    // завершение по ошибке
    stop_streaming:
    if (ioctl(webcam->webcam_fd, VIDIOC_STREAMOFF, &webcam->buffer_info.type) < 0) {
        perror("VIDIOC_STREAMOFF");
    }

    destroy_mutex:
    r = pthread_mutex_destroy(&thread->running_mutex);
    assert(r == 0);

    free_thread:
    free_thread(webcam->thread);
    webcam->thread = NULL;
    webcam->tid = 0;

    return res;
}

int8_t webcam_stop_stream(Webcam *webcam) {
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
}

bool webcam_streaming(Webcam *webcam) {
    return webcam->thread != NULL;
}

void webcam_close(Webcam *webcam) {
    webcam_stop_stream(webcam);
    munmap(webcam->frame_buffer, webcam->buffer_info.length);
    close(webcam->webcam_fd);
    free(webcam);
}