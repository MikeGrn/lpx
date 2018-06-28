#ifndef LPX_STREAM_STORAGE_H
#define LPX_STREAM_STORAGE_H

#include <stdint.h>
#include <stdio.h>
#include "stream.h"

// Error codes
#define STRG_ACCESS    2
#define STRG_EXISTS    3
#define STRG_NOT_FOUND 4
#define STRG_BAD_INDEX 5

typedef struct Storage Storage;

int8_t storage_open(char *base_dir, Storage **storage);

int8_t storage_prepare(Storage *storage, char *train_id);

int8_t storage_store_frame(Storage *storage, char *train_id, uint32_t frame_idx, const uint8_t *buf, size_t size);

int8_t storage_store_stream_idx(Storage *storage, char *train_id, FrameMeta **index, size_t frames_cnt);

int8_t storage_read_stream_idx(Storage *storage, char *train_id, FrameMeta ***index, size_t *frames_cnt);

int8_t storage_read_frame(Storage *storage, char *train_id, uint32_t frame_idx, uint8_t **buf, size_t *len);

int8_t storage_find_stream(Storage *storage, int64_t time, char **train_id);

int8_t storage_open_stream(Storage *storage, char *train_id, VideoStreamBytesStream **stream);

void storage_close(Storage *storage);

#endif //LPX_STREAM_STORAGE_H
