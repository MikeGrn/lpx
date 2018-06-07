#ifndef LPX_WEBCAM2_H
#define LPX_WEBCAM2_H

#include <stdint.h>
#include "stream_storage.h"

#define WC_IO   1
#define WC_CAP  2
#define WC_STRG 3

typedef struct Webcam Webcam;

int8_t webcam_init(Storage *storage, char *device, Webcam **webcam);

int8_t webcam_start_stream(Webcam *webcam, char *train_id);

int8_t webcam_stop_stream(Webcam *webcam);

void webcam_close(Webcam *webcam);

#endif //LPX_WEBCAM2_H
