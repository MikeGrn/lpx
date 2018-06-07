#ifndef LPX_WEBCAM2_H
#define LPX_WEBCAM2_H

#include <stdint.h>
#include "stream_storage.h"

int8_t webcam_init(Storage *storage);

int8_t webcam_start_stream(char *train_id);


void webcam_close();

#endif //LPX_WEBCAM2_H
