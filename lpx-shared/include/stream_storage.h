#ifndef LPX_STREAM_STORAGE_H
#define LPX_STREAM_STORAGE_H

#include <stdint.h>
#include <stdio.h>
#include "stream.h"
#include "list.h"

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

int8_t storage_find_stream(Storage *storage, uint64_t time, char **train_id);

/**
 * Возвращает поток байт содиржащих все фремы заданного стрима начиная с заданного оффсета
 */
int8_t storage_open_stream(Storage *storage, char *train_id, size_t offset_idx, VideoStreamBytesStream **stream);

/**
 * Возвращает поток байт содержащих фреймы по указанным индексам в заданном стриме
 */
int8_t storage_open_stream_frames(Storage *storage, char *train_id, List *frame_indexes, VideoStreamBytesStream **stream);

int8_t storage_delete_stream(Storage *storage, char *train_id);

/**
 * Удаляет все стримы с диска
 */
int8_t storage_clear(Storage *storage);

void storage_close(Storage *storage);

#endif //LPX_STREAM_STORAGE_H
