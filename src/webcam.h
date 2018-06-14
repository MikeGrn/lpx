#ifndef LPX_WEBCAM2_H
#define LPX_WEBCAM2_H

#include <stdint.h>
#include "stream_storage.h"

// Коды ошибок
#define WC_THREAD 2 // ошибки управления потоком
#define WC_CAP    3 // ошибки конфигурации вебкамеры
#define WC_STRG   4 // ошибки хранилища потоков

typedef struct Webcam Webcam;

typedef void (*error_callback)(void *user_data, int errcode);

int8_t webcam_init(Storage *storage, char *device, Webcam **webcam, void* user_data, error_callback callback);

int8_t webcam_start_stream(Webcam *webcam, char *train_id);

int8_t webcam_stop_stream(Webcam *webcam);

bool webcam_streaming(Webcam *pWebcam);

void webcam_close(Webcam *webcam);

#endif //LPX_WEBCAM_H
