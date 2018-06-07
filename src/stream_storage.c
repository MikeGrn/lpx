#include <errno.h>
#include <zconf.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sys/mman.h>
#include "stream_storage.h"
#include "lpxstd.h"
#include "list.h"

#define FRAME_FORMAT "%d,%d,%" PRId64 ",%" PRId64 "\n"

typedef struct Storage {
    char *base_dir;
} Storage;

int8_t storage_open(char *base_dir, Storage **storage) {
    if (access(base_dir, W_OK) != 0) {
        return STRG_ACCESS;
    }
    Storage *res = xmalloc(sizeof(Storage));
    char *bd = xcalloc(sizeof(char), strlen(base_dir) + 1);
    strncpy(bd, base_dir, strlen(base_dir));
    res->base_dir = bd;
    *storage = res;
    return LPX_SUCCESS;
}

static char *train_dir(Storage *storage, char *train_id) {
    return append_path(storage->base_dir, train_id);
}

static char *frame_path(char *train_dir, int frame_idx) {
    char frame_file[10 + 5 + 1]; // Любое число uint32_t влезет в 10 символов
    snprintf(frame_file, 16, "%d.jpeg", frame_idx);
    char *frame_path = append_path(train_dir, frame_file);
    return frame_path;
}

int8_t storage_prepare(Storage *storage, char *train_id) {
    int8_t res = LPX_SUCCESS;

    char *td = train_dir(storage, train_id);

    if (access(td, F_OK) != 0) {
        res = STRG_EXISTS;
        goto cleanup;
    }

    if (mkdir(td, 0700) != 0) {
        res = STRG_IO;
        goto cleanup;
    }

    cleanup:
    free(td);

    return res;
}

ssize_t
storage_store_frame(Storage *storage, char *train_id, uint32_t frame_idx, const uint8_t *buf, size_t size) {
    int8_t res = LPX_SUCCESS;

    char *td = train_dir(storage, train_id);

    if (access(td, F_OK) != 0) {
        res = STRG_NOT_FOUND;
        goto cleanup;
    }

    char *fp = frame_path(td, frame_idx);

    FILE *frame_f = fopen(fp, "w+");
    if (frame_f == NULL) {
        res = STRG_IO;
        goto cleanup;
    }

    if (fwrite(buf, 1, size, frame_f) == -1) {
        res = STRG_IO;
        goto close_file;
    }

    close_file:
    fclose(frame_f);

    cleanup:
    free(td);
    free(fp);

    return res;
}

ssize_t
storage_store_stream_idx(Storage *storage, char *train_id, FrameMeta *index, uint32_t frames_cnt) {
    int8_t res = LPX_SUCCESS;

    char *td = train_dir(storage, train_id);

    if (access(td, F_OK) != 0) {
        res = STRG_NOT_FOUND;
        goto cleanup;
    }

    char *idx_path = append_path(td, "index.csv");

    FILE *idx_f = fopen(idx_path, "w+");
    if (idx_f == NULL) {
        res = STRG_IO;
        goto cleanup;
    }

    for (int i = 0; i < frames_cnt; i++) {
        int r = fprintf(idx_f, FRAME_FORMAT, index[i].offset, index[i].size,
                        index[i].start_time, index[i].end_time);
        if (r < 0) {
            goto close_file;
        }
    }

    close_file:
    fclose(idx_f);

    cleanup:
    free(td);
    free(idx_path);

    return res;
}

int8_t
storage_read_stream_idx(Storage *storage, char *train_id, FrameMeta ***index, uint32_t *frames_cnt) {
    int8_t res = LPX_SUCCESS;

    char *td = train_dir(storage, train_id);

    if (access(td, F_OK) != 0) {
        res = STRG_NOT_FOUND;
        goto cleanup;
    }

    char *idx_path = append_path(td, "index.csv");
    if (access(idx_path, F_OK) != 0) {
        res = STRG_NOT_FOUND;
        goto cleanup;
    }

    FILE *idx_f = fopen(idx_path, "r");
    if (idx_f == NULL) {
        res = STRG_IO;
        goto cleanup;
    }

    List *frames = lst_create();

    char *buf = xcalloc(sizeof(char), 256);
    while (fgets(buf, 256, idx_f) != NULL) {
        FrameMeta *frame = xmalloc(sizeof(FrameMeta));
        int r = sscanf(buf, FRAME_FORMAT, &frame->offset, &frame->size, &frame->start_time, &frame->end_time);
        if (r == EOF || r != 4) {
            res = STRG_BAD_INDEX;
            goto close_file;
        }
        lst_append(frames, frame);
    }
    if (ferror(idx_f)) {
        res = STRG_IO;
        goto close_file;
    }

    FrameMeta **frame_array = lst_size(frames) == 0 ? NULL : xcalloc(lst_size(frames), sizeof(FrameMeta *));

    if (lst_size(frames) > 0) {
        lst_to_array(frames, (void **) frame_array);
    }

    *index = frame_array;
    *frames_cnt = lst_size(frames);

    close_file:
    fclose(idx_f);

    cleanup:
    free(td);
    free(idx_path);
    free(buf);
    lst_free(frames);

    return res;
}

int8_t storage_read_frame(Storage *storage, char *train_id, uint32_t frame_idx, uint8_t **buf, size_t *len) {
    int8_t res = LPX_SUCCESS;

    char *td = train_dir(storage, train_id);
    if (access(td, F_OK) != 0) {
        res = STRG_NOT_FOUND;
        goto cleanup;
    }

    char *fp = frame_path(td, frame_idx);
    if (access(fp, F_OK) != 0) {
        res = STRG_NOT_FOUND;
        goto cleanup;
    }

    FILE *frame_f = fopen(fp, "r");
    if (frame_f == NULL) {
        res = STRG_IO;
        goto cleanup;
    }


    size_t size;
    if (file_size(frame_f, (off_t *) &size) != 0) {
        res = STRG_IO;
        goto close_file;
    }
    *buf = xmalloc(size);
    *len = size;

    if (fread(*buf, size, 1, frame_f) != 0) {
        res = STRG_IO;
        goto close_file;
    }

    close_file:
    fclose(frame_f);

    cleanup:
    free(td);
    free(fp);

    return res;
}

void storage_close(struct Storage *storage) {
    free(storage->base_dir);
    free(storage);
}
