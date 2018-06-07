#include <fcntl.h>
#include <linux/videodev2.h>
#include <stropts.h>
#include <sys/mman.h>
#include <memory.h>
#include <stdbool.h>
#include <pthread.h>
#include "webcam2.h"
#include "lpxstd.h"
#include "list.h"
#include <unistd.h>

typedef struct ThreadArgs {
    Webcam *webcam;
    char *train_id;
    bool running;
    pthread_mutex_t running_mutex;
    uint32_t frame_index;
    struct timeval frame_req_time;
    struct timeval frame_ready_time;
} ThreadArgs;

typedef struct Webcam {
    Storage *storage;
    int webcam_fd;
    uint8_t *frame_buffer;
    struct v4l2_buffer buffer_info;
    ThreadArgs *thread;
    pthread_t tid;
} Webcam;

int8_t webcam_init(Storage *storage, char *device, Webcam **webcam) {
    int8_t res = LPX_SUCCESS;
    *webcam = xmalloc(sizeof(Webcam));
    memset(*webcam, 0, sizeof(Webcam));
    Webcam *w = *webcam;
    w->storage = storage;

    if ((w->webcam_fd = open(device, O_RDWR)) < 0) {
        res = WC_IO;
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
        res = WC_IO;
        perror("VIDIOC_S_FMT");
        goto close_webcam;
    }

    struct v4l2_requestbuffers buf_request = {0};
    buf_request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf_request.memory = V4L2_MEMORY_MMAP;
    buf_request.count = 1;
    if (ioctl(w->webcam_fd, VIDIOC_REQBUFS, &buf_request) < 0) {
        res = WC_IO;
        perror("VIDIOC_REQBUFS");
        goto close_webcam;
    }

    w->buffer_info.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    w->buffer_info.memory = V4L2_MEMORY_MMAP;
    w->buffer_info.index = 0;
    if (ioctl(w->webcam_fd, VIDIOC_QUERYBUF, &w->buffer_info) < 0) {
        res = WC_IO;
        perror("VIDIOC_QUERYBUF");
        goto close_webcam;
    }

    w->frame_buffer = mmap(NULL, w->buffer_info.length, PROT_READ | PROT_WRITE, MAP_SHARED, w->webcam_fd,
                           w->buffer_info.m.offset);
    if (w->frame_buffer == MAP_FAILED) {
        res = WC_IO;
        perror("mmap");
        goto close_webcam;
    }

    return res;

    close_webcam:
    close(w->webcam_fd);

    error:
    free(*webcam);
    *webcam = NULL;

    return res;
}

static bool isRunning(ThreadArgs *args) {
    bool ret = 0;
    pthread_mutex_lock(&args->running_mutex);
    ret = args->running;
    pthread_mutex_unlock(&args->running_mutex);
    return ret;
}

static void stop(ThreadArgs *args) {
    pthread_mutex_lock(&args->running_mutex);
    args->running = 0;
    pthread_mutex_unlock(&args->running_mutex);
}

static void *webcam_handle_stream(void *a) {
    ThreadArgs *args = a;
    Webcam *webcam = args->webcam;
    args->frame_index = 0;
    List *frames = lst_create();

    if (-1 == ioctl(webcam->webcam_fd, VIDIOC_STREAMON, &webcam->buffer_info.type)) {
        perror("Start Capture");
        // TODO
        abort();
    }

    gettimeofday(&args->frame_req_time, NULL);
    if (ioctl(webcam->webcam_fd, VIDIOC_QBUF, &webcam->buffer_info) < 0) {
        perror("VIDIOC_QBUF");
        // TODO
        abort();
    }

    printf("started\n");
    while (isRunning(args)) {
        if (-1 == ioctl(webcam->webcam_fd, VIDIOC_DQBUF, &webcam->buffer_info)) {
            perror("Retrieving Frame");
            // TODO
            abort();
        }
        gettimeofday(&args->frame_ready_time, NULL);

        storage_store_frame(webcam->storage, args->train_id, args->frame_index++, webcam->frame_buffer,
                            webcam->buffer_info.length);

        FrameMeta *frame = xmalloc(sizeof(FrameMeta));
        memset(frame, 0, sizeof(FrameMeta));
        frame->start_time = tv2mks(args->frame_req_time);
        frame->end_time = tv2mks(args->frame_ready_time);
        lst_append(frames, frame);

        gettimeofday(&args->frame_req_time, NULL);
        if (ioctl(webcam->webcam_fd, VIDIOC_QBUF, &webcam->buffer_info) < 0) {
            perror("VIDIOC_QBUF");
            // TODO
            abort();
        }
    }

    if (ioctl(webcam->webcam_fd, VIDIOC_STREAMOFF, &webcam->buffer_info.type) < 0) {
        perror("VIDIOC_STREAMOFF");
        // TODO: can try to stream on later?
    }

    uint32_t frames_cnt = lst_size(frames);
    FrameMeta **frame_array = frames_cnt == 0 ? NULL : xcalloc(frames_cnt, sizeof(FrameMeta *));
    if (frames_cnt > 0) {
        lst_to_array(frames, (void **) frame_array);
        storage_store_stream_idx(webcam->storage, args->train_id, frame_array, frames_cnt);
    }

    for (int i = 0; i < frames_cnt; i++) {
        free(frame_array[i]);
    }
    free(frame_array);
    lst_free(frames);

    printf("exited\n");
    return NULL;
}

int8_t webcam_start_stream(Webcam *webcam, char *train_id) {
    if (LPX_SUCCESS != storage_prepare(webcam->storage, train_id)) {
        return WC_STRG;
    }
    ThreadArgs *thread = xmalloc(sizeof(ThreadArgs));
    memset(thread, 0, sizeof(ThreadArgs));
    thread->running = 1;
    thread->webcam = webcam;
    thread->train_id = train_id;
    pthread_mutex_init(&thread->running_mutex, NULL);
    webcam->thread = thread;
    // TODO: handle errors
    pthread_t tid;
    pthread_create(&tid, NULL, webcam_handle_stream, thread);
    webcam->tid = tid;
    return LPX_SUCCESS;
}

int8_t webcam_stop_stream(Webcam *webcam) {
    stop(webcam->thread);
    pthread_join(webcam->tid, NULL);
    free(webcam->thread);
    // TODO: handle errors
}

void webcam_close(Webcam *webcam) {
    munmap(webcam->frame_buffer, webcam->buffer_info.length);
    close(webcam->webcam_fd);
    free(webcam);
}