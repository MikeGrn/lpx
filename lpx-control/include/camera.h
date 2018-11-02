#ifndef LPX_WEBCAM2_H
#define LPX_WEBCAM2_H

#include <stdint.h>
#include "stream_storage.h"

// Коды ошибок
#define CAM_THREAD 2 // ошибки управления потоком
#define CAM_CAP    3 // ошибки конфигурации вебкамеры
#define CAM_STRG   4 // ошибки хранилища потоков

typedef struct Camera Camera;

typedef void (*error_callback)(void *user_data, int errcode);

int8_t camera_init(Storage *storage, char *device, Camera **camera, void *user_data, error_callback callback);

int8_t camera_start_stream(Camera *camera, char *train_id);

int8_t camera_stop_stream(Camera *camera);

bool camera_streaming(Camera *camera);

void camera_close(Camera *camera);

#endif //LPX_WEBCAM_H
