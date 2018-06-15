#include <stdint.h>
#include <stdlib.h>
#include "../include/stream.h"

int32_t stream_find_frame(FrameMeta **index, uint32_t index_len, uint64_t time) {
    int64_t stream_base = index[0]->start_time;
    for (int i = 0; i < index_len - 1; i++) {
        int64_t frameOffset = index[i]->start_time - stream_base;
        int64_t nextFrameOffset = index[i + 1]->start_time - stream_base;
        if (labs(nextFrameOffset - time) > labs(frameOffset - time)) {
            return i;
        }
    }
    return -1;
}
