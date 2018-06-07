#ifndef LPX_WEBCAM_H
#define LPX_WEBCAM_H

#include <poll.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct FrameMeta {
    int64_t start_time;
    int64_t end_time;
    uint32_t offset;
    uint32_t size;
} FrameMeta;

int8_t webcam_init(char *outDir);

struct pollfd webcam_fd();

int8_t webcam_start_stream(int64_t trainId);

int8_t webcam_handle_frame(int64_t trainId, bool last);

bool webcam_streaming();

struct FrameMeta *webcam_last_stream_index();

unsigned char *webcam_get_frame(int64_t trainId, FrameMeta frame);

void webcam_close();

#endif //LPX_WEBCAM_H
