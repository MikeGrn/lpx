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

typedef struct CaptureSession {
    Storage *storage;
    char *train_id;
    uint32_t frame_index; // порядковый номер следующего фрейма
    struct timeval frame_req_time; // системное (астрономическое) время запроса фрейма
    void *user_data; // пользовательские данные, передаваемые в каллбэк
    error_callback ecb;
    List *frames;
    bool stopping;
    pthread_mutex_t mutex;
} CaptureSession;


typedef struct Camera {
    Storage *storage;
    Raspiraw *raspiraw;
    void *user_data; // пользовательские данные, передаваемые в каллбэк
    error_callback ecb;
    CaptureSession *capture_session;
} Camera;

static struct timeval current_time() {
    static struct timeval cur_time;
    int r = gettimeofday(&cur_time, NULL);
    assert(!r); // получение даты в непустую структуру никогда не должно приводить к ошибке
    return cur_time;
}

static void camera_handle_frame(uint8_t *buffer, size_t buffer_len, void *cs) {
    CaptureSession *capture_session = cs;

    bool stopping;
    int r = pthread_mutex_lock(&capture_session->mutex);
    assert(r == 0 && "Could not not lock running_mutex");
    stopping = capture_session->stopping;
    r = pthread_mutex_unlock(&capture_session->mutex);
    assert(r == 0 && "Could not not unlock running_mutex");

    if (stopping) {
        return;
    }

    r = storage_store_frame(capture_session->storage, capture_session->train_id, capture_session->frame_index++,
                                buffer, buffer_len);

    if (LPX_SUCCESS != r) {
        if (errno != 0) {
            perror("Frame writing");
        }
        capture_session->ecb(capture_session->user_data, r);
        fprintf(stderr, "Stream storage failed, errcode: %d\n", r);
        return;
    }

    struct timeval cur_time = current_time();

    FrameMeta *frame = xmalloc(sizeof(FrameMeta));
    memset(frame, 0, sizeof(FrameMeta));
    frame->start_time = tv2mks(capture_session->frame_req_time);
    frame->end_time = tv2mks(cur_time);
    lst_append(capture_session->frames, frame);

    capture_session->frame_req_time = cur_time;
}

int8_t camera_init(Storage *storage, Camera **camera, void *user_data, error_callback ecb) {
    int8_t res = LPX_SUCCESS;
    *camera = xmalloc(sizeof(Camera));
    memset(*camera, 0, sizeof(Camera));
    Raspiraw *raspiraw;
    if (LPX_SUCCESS != raspiraw_init(&raspiraw, &camera_handle_frame)) {
        res = CAM_CAP;
        goto error;
    }
    Camera *c = *camera;
    c->storage = storage;
    c->ecb = ecb;
    c->user_data = user_data;
    c->raspiraw = raspiraw;

    return res;

    // завершение по ошибке
    error:
    free(*camera);
    *camera = NULL;

    return res;
}

int8_t camera_start_stream(Camera *camera, char *train_id) {

    List *frames = lst_create();
    CaptureSession *cs = xmalloc(sizeof(CaptureSession));
    memset(cs, 0, sizeof(CaptureSession));
    cs->frame_req_time = current_time();
    size_t train_id_size = strnlen(train_id, MAX_INT_LEN);
    cs->train_id = xcalloc(train_id_size + 1, sizeof(char));
    strncpy(cs->train_id, train_id, train_id_size);
    cs->train_id = train_id;
    cs->storage = camera->storage;
    cs->frames = frames;
    cs->ecb = camera->ecb;
    cs->frame_index = 0;
    cs->user_data = camera->user_data;
    cs->stopping = false;

    int8_t res;
    if (LPX_SUCCESS != storage_prepare(camera->storage, train_id)) {
        res = CAM_STRG;
        goto error;
    }

    int r = pthread_mutex_init(&cs->mutex, NULL);
    if (0 != r) {
        res = CAM_THREAD;
        fprintf(stderr, "Could not initialize capture session mutex, errcode: %d\n", r);
        goto error;
    }

    res = raspiraw_start(camera->raspiraw, cs);

    camera->capture_session = cs;

    return res;

    error:
    free(cs->train_id);
    lst_free(cs->frames);
    free(cs);
    return res;
}

static int8_t camera_write_frame_index(CaptureSession *capture_session) {
    int8_t res = LPX_SUCCESS;

    size_t frames_cnt = lst_size(capture_session->frames);
    FrameMeta **frame_array = frames_cnt == 0 ? NULL : xcalloc(frames_cnt, sizeof(FrameMeta *));
    if (frames_cnt > 0) {
        lst_to_array(capture_session->frames, (const void **) frame_array);
        int8_t r = storage_store_stream_idx(capture_session->storage, capture_session->train_id, frame_array, frames_cnt);
        if (LPX_SUCCESS != r) {
            if (errno != 0) {
                res = CAM_STRG;
                perror("Stream index writing");
            }
            capture_session->ecb(capture_session->user_data, r);
            fprintf(stderr, "Stream storage failed, errcode: %d\n", r);
        }
    }

    free_array((void **) frame_array, frames_cnt);
    lst_free(capture_session->frames);
    free(capture_session);
    
    return res;
}

int8_t camera_stop_stream(Camera *camera) {
    CaptureSession *cs = camera->capture_session;
    if (cs == NULL || cs->stopping) {
        printf("not streaming ignore stop request\n");
        return LPX_SUCCESS;
    }

    int r = pthread_mutex_lock(&cs->mutex);
    assert(r == 0 && "Could not not lock running_mutex");
    cs->stopping = true;
    r = pthread_mutex_unlock(&cs->mutex);
    assert(r == 0 && "Could not not unlock running_mutex");

    raspiraw_stop(camera->raspiraw);

    int8_t write_res = camera_write_frame_index(cs);
    if (LPX_SUCCESS != write_res) {
        camera->ecb(camera->user_data, write_res);
    }
    
    return LPX_SUCCESS;
}

bool camera_streaming(Camera *camera) {
    return camera->capture_session != NULL;
}

void camera_close(Camera *camera) {
    camera_stop_stream(camera);
    raspiraw_close(camera->raspiraw);
    free(camera);
}