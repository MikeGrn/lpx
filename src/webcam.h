#ifndef LPX_WEBCAM_H
#define LPX_WEBCAM_H

#include <poll.h>
#include <stdbool.h>
#include <stdint.h>

int8_t webcam_init(char *outDir);

struct pollfd webcam_fd();

int8_t webcam_start_stream(int64_t trainId);

int8_t webcam_handle_frame(int64_t trainId, bool last);

bool webcam_streaming();

void webcam_close();

#endif //LPX_WEBCAM_H
