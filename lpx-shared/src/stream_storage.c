#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <unistd.h>
#include <dirent.h>
#include <zip.h>
#include "../include/stream_storage.h"
#include "../include/lpxstd.h"

// формат записи в файле индекса потока
#define FRAME_FORMAT "%" PRId64 ",%" PRId64 "\n"

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

static char *frame_path(char *train_dir, size_t frame_idx) {
    char frame_file[21 + 5 + 1]; // Любое число uint64_t влезет в 21 символов
    snprintf(frame_file, 16, "%ld.jpeg", frame_idx);
    char *frame_path = append_path(train_dir, frame_file);
    return frame_path;
}

int8_t storage_prepare(Storage *storage, char *train_id) {
    int8_t res = LPX_SUCCESS;

    char *td = train_dir(storage, train_id);

    if (access(td, F_OK) == 0) {
        res = STRG_EXISTS;
        goto cleanup;
    }

    if (mkdir(td, 0777) != 0) {
        res = LPX_IO;
        goto cleanup;
    }

    cleanup:
    free(td);

    return res;
}

int8_t
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
        res = LPX_IO;
        goto cleanup;
    }

    if (fwrite(buf, 1, size, frame_f) == -1) {
        res = LPX_IO;
        goto close_file;
    }

    close_file:
    fclose(frame_f);

    cleanup:
    free(td);
    free(fp);

    return res;
}

int8_t
storage_store_stream_idx(Storage *storage, char *train_id, FrameMeta **index, size_t frames_cnt) {
    int8_t res = LPX_SUCCESS;

    char *td = train_dir(storage, train_id);

    if (access(td, F_OK) != 0) {
        res = STRG_NOT_FOUND;
        goto cleanup;
    }

    char *idx_path = append_path(td, "index.csv");

    FILE *idx_f = fopen(idx_path, "w+");
    if (idx_f == NULL) {
        res = LPX_IO;
        goto cleanup;
    }

    for (int i = 0; i < frames_cnt; i++) {
        int r = fprintf(idx_f, FRAME_FORMAT, index[i]->start_time, index[i]->end_time);
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

int8_t storage_read_stream_idx(Storage *storage, char *train_id, FrameMeta ***index, size_t *frames_cnt) {
    int8_t res = LPX_SUCCESS;

    char *td = train_dir(storage, train_id);

    if (access(td, F_OK) != 0) {
        res = STRG_NOT_FOUND;
        goto free_td;
    }

    char *idx_path = append_path(td, "index.csv");
    if (access(idx_path, F_OK) != 0) {
        res = STRG_NOT_FOUND;
        goto free_td;
    }

    FILE *idx_f = fopen(idx_path, "r");
    if (idx_f == NULL) {
        res = LPX_IO;
        goto free_td;
    }

    List *frames = lst_create();

    char *buf = xcalloc(sizeof(char), 256);
    while (fgets(buf, 256, idx_f) != NULL) {
        FrameMeta *frame = xmalloc(sizeof(FrameMeta));
        int r = sscanf(buf, FRAME_FORMAT, &frame->start_time, &frame->end_time);
        if (r == EOF || r != 2) {
            res = STRG_BAD_INDEX;
            goto free_frames;
        }
        lst_append(frames, frame);
    }
    if (ferror(idx_f)) {
        res = LPX_IO;
        goto free_frames;
    }

    FrameMeta **frame_array = lst_size(frames) == 0 ? NULL : xcalloc(lst_size(frames), sizeof(FrameMeta *));

    if (lst_size(frames) > 0) {
        lst_to_array(frames, (const void **) frame_array);
    }

    *index = frame_array;
    *frames_cnt = lst_size(frames);

    free_frames:
    lst_free(frames);
    fclose(idx_f);
    free(buf);

    free_td:
    free(td);
    free(idx_path);

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
        res = LPX_IO;
        goto cleanup;
    }


    size_t size;
    if (file_size(frame_f, (off_t *) &size) != 0) {
        res = LPX_IO;
        goto close_file;
    }
    *buf = xmalloc(size);
    *len = size;

    if (fread(*buf, size, 1, frame_f) != 0) {
        res = LPX_IO;
        goto close_file;
    }

    close_file:
    fclose(frame_f);

    cleanup:
    free(td);
    free(fp);

    return res;
}

static int8_t storage_list_streams(Storage *storage, char ***train_ids, size_t *len) {
    return list_directory(storage->base_dir, train_ids, len);
}

static int8_t storage_read_frame_meta(Storage *storage, char *train_id, uint32_t idx, FrameMeta **frame_meta) {
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
        res = LPX_IO;
        goto cleanup;
    }


    char *buf = NULL;
    buf = xcalloc(sizeof(char), 256);
    FrameMeta *frame = xmalloc(sizeof(FrameMeta));
    while (fgets(buf, 256, idx_f) != NULL) {
        int r = sscanf(buf, FRAME_FORMAT, &frame->start_time, &frame->end_time);
        if (r == EOF || r != 2) {
            res = STRG_BAD_INDEX;
            goto close_file;
        }
        if (idx-- == 0) {
            break;
        }
    }
    if (ferror(idx_f)) {
        res = LPX_IO;
        goto close_file;
    }

    *frame_meta = frame;

    close_file:
    fclose(idx_f);
    free(buf);

    cleanup:
    free(td);
    free(idx_path);

    return res;
}

int8_t storage_find_stream(Storage *storage, uint64_t time, char **train_id) {
    char **streams;
    size_t streams_len;
    int8_t res = list_directory(storage->base_dir, &streams, &streams_len);
    if (res != LPX_SUCCESS) {
        return LPX_IO;
    }

    ssize_t found_train_idx = -1;
    for (int i = 0; i < streams_len; i++) {
        FrameMeta **index;
        size_t index_size;
        res = storage_read_stream_idx(storage, streams[i], &index, &index_size);
        if (res != LPX_SUCCESS) {
            continue;
        }
        ssize_t idx = stream_find_frame_abs(index, index_size, time);
        free_array((void **) index, index_size);
        if (idx != -1) {
            found_train_idx = i;
        }
    }
    if (found_train_idx != -1) {
        *train_id = xcalloc(strlen(streams[found_train_idx]) + 1, sizeof(char));
        strcpy(*train_id, streams[found_train_idx]);
    }

    free_array((void **) streams, streams_len);

    return LPX_SUCCESS;
}

int8_t storage_open_stream(Storage *storage, char *train_id, size_t offset_idx, VideoStreamBytesStream **stream) {
    char *td = train_dir(storage, train_id);
    FrameMeta **index = NULL;
    size_t index_size = 0;
    int8_t res = storage_read_stream_idx(storage, train_id, &index, &index_size);
    if (res != LPX_SUCCESS) {
        res = LPX_IO;
        goto free_index;
    }

    size_t files_size = offset_idx < index_size ? index_size - offset_idx : 0;
    char **files = xcalloc(files_size, sizeof(char *));
    for (size_t i = 0; i < files_size; i++) {
        files[i] = frame_path(td, i + offset_idx);
    }

    *stream = stream_open(files, files_size);

    free_index:
    free_array((void **) index, index_size);

    free(td);

    return res;
}

int8_t
storage_open_stream_frames(Storage *storage, char *train_id, List *frame_indexes, VideoStreamBytesStream **stream) {
    char *td = train_dir(storage, train_id);
    FrameMeta **index = NULL;
    size_t index_size = 0;
    int8_t res = storage_read_stream_idx(storage, train_id, &index, &index_size);
    if (res != LPX_SUCCESS) {
        res = LPX_IO;
        goto free_index;
    }

    size_t files_size = lst_size(frame_indexes);
    char **files = xcalloc(files_size, sizeof(char *));
    ListIter *iter = lst_iterator(frame_indexes);
    for (int i = 0; lst_iter_advance(iter); i++) {
        files[i] = frame_path(td, *((size_t *) lst_iter_peak(iter)));
    }

    *stream = stream_open(files, files_size);

    lst_iter_free(iter);

    free_index:
    free_array((void **) index, index_size);

    free(td);

    return res;
}

int8_t storage_delete_stream(Storage *storage, char *train_id) {
    int8_t res = 0;

    char *td = train_dir(storage, train_id);
    char **files;
    size_t files_len;
    res = list_directory(td, &files, &files_len);
    if (res != LPX_SUCCESS) {
        res = LPX_IO;
        goto free_td;
    }

    for (int i = 0; i < files_len; i++) {
        char *path = append_path(td, files[i]);
        int r = unlink(path);
        free(path);
        if (r != 0) {
            res = LPX_IO;
            goto free_files;
        }
    }
    rmdir(td);

    free_files:
    free_array((void **) files, files_len);

    free_td:
    free(td);

    return res;
}

int8_t storage_clear(Storage *storage) {
    int8_t res = 0;
    char **streams;
    size_t streams_len;
    res = list_directory(storage->base_dir, &streams, &streams_len);
    if (res != LPX_SUCCESS) {
        return LPX_IO;
    }

    for (int i = 0; i < streams_len; i++) {
        res = storage_delete_stream(storage, streams[i]);
        if (res != LPX_SUCCESS) {
            break;
        }
    }

    free_array((void **) streams, streams_len);

    return res;
}

void storage_close(struct Storage *storage) {
    free(storage->base_dir);
    free(storage);
}
