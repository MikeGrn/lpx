#ifndef LPX_WEBCAM2_H
#define LPX_WEBCAM2_H

#include <stdint.h>
#include "stream_storage.h"

// Error codes
#define WC_IO     1
#define WC_CAP    2
#define WC_STRG   3
#define WC_THREAD 4

typedef struct Webcam Webcam;

typedef void (*error_callback)(void *user_data, int errcode);

int8_t webcam_init(Storage *storage, char *device, Webcam **webcam, void* user_data, error_callback callback);

int8_t webcam_start_stream(Webcam *webcam, char *train_id);

int8_t webcam_stop_stream(Webcam *webcam);

bool webcam_streaming(Webcam *pWebcam);

void webcam_close(Webcam *webcam);

#endif //LPX_WEBCAM2_H
