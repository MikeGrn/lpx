#include <CUnit/CUnit.h>
#include <lpxstd.h>
#include <archive_entry.h>
#include <assert.h>
#include <poll.h>
#include "../include/stream_storage.h"
#include "../src/stream_storage.c"
#include "CUnit/Basic.h"

#define ADD_TEST(P_SUITE, TEST) \
if ((NULL == CU_add_test(P_SUITE, #TEST , TEST ))) { \
    CU_cleanup_registry(); \
    return CU_get_error(); \
}

static char *base_dir;

int init_suite(void) { return 0; }

int clean_suite(void) { return 0; }

int stringcmp(const void *a, const void *b) {
    const char **ia = (const char **) a;
    const char **ib = (const char **) b;
    return strcmp(*ia, *ib);
}

void test_list_directory(void) {
    char **children;
    size_t len;
    list_directory(base_dir, &children, &len);
    qsort(children, len, sizeof(char *), stringcmp);
    CU_ASSERT_EQUAL(len, 2);
    CU_ASSERT_STRING_EQUAL(children[0], "subdir1");
    CU_ASSERT_STRING_EQUAL(children[1], "subdir2");
    for (int i = 0; i < len; i++) {
        free(children[i]);
    }
    free(children);
}

void test_read_first_frame_meta(void) {
    Storage *s;
    storage_open(base_dir, &s);
    FrameMeta *fm;
    storage_read_frame_meta(s, "subdir1", 0, &fm);
    CU_ASSERT_EQUAL(1529488204473095, fm->start_time);
    CU_ASSERT_EQUAL(1529488205138216, fm->end_time);
    free(fm);
    storage_close(s);
}

void test_read_last_frame_meta(void) {
    Storage *s;
    storage_open(base_dir, &s);
    FrameMeta *fm;
    storage_read_frame_meta(s, "subdir1", 29, &fm);
    CU_ASSERT_EQUAL(1529488207551183, fm->start_time);
    CU_ASSERT_EQUAL(1529488207690131, fm->end_time);
    free(fm);
    storage_close(s);
}

void test_find_first_stream(void) {
    Storage *s;
    storage_open(base_dir, &s);
    char *stream;
    storage_find_stream(s, 1529488204473096, &stream);
    CU_ASSERT_STRING_EQUAL("subdir1", stream);
    free(stream);
    storage_close(s);
}

void test_find_second_stream(void) {
    Storage *s;
    storage_open(base_dir, &s);
    char *stream;
    storage_find_stream(s, 1529489555016679, &stream);
    CU_ASSERT_STRING_EQUAL("subdir2", stream);
    free(stream);
    storage_close(s);
}

/*void test_stream_compress(void) {
    Storage *s;
    storage_open(base_dir, &s);
    char *fname = xmalloc(L_tmpnam);
    fname = tmpnam(fname);
    CU_ASSERT_PTR_NOT_NULL(fname);
    storage_compress(s, "subdir1", fname);
    int errorp = 0;
    zip_t *zip = zip_open(fname, ZIP_RDONLY, &errorp);
    int files = zip_get_num_files(zip);
    CU_ASSERT_EQUAL(files, 31);
    zip_discard(zip);
    free(fname);
    storage_close(s);
}*/


void test_stream_compress_fd(void) {
    Storage *s;
    storage_open(base_dir, &s);
    char file_name[] = "lpx-XXXXXX";
    FILE *f = tmpfile();
    CU_ASSERT_PTR_NOT_NULL(f);
    storage_compress_fd(s, "subdir1", f);
    int errorp = 0;
    zip_source_t *zs = zip_source_filep_create(f, 0, -1, NULL);
    zip_t *zip = zip_open_from_source(zs, ZIP_RDONLY, NULL);
    int files = zip_get_num_files(zip);
    CU_ASSERT_EQUAL(files, 31);
    zip_discard(zip);
    storage_close(s);
}

void test_stream_archive_file(void) {
    Storage *s;
    storage_open(base_dir, &s);

    StreamArchiveStream *stream = NULL;
    storage_open_stream(s, "subdir1", &stream);

    char **children;
    size_t children_len;
    list_directory(append_path(base_dir, "subdir1"), &children, &children_len);

    struct archive *archive = archive_write_new();
    if (archive == NULL ||
        archive_write_set_format_zip(archive) != ARCHIVE_OK ||
        archive_write_add_filter_none(archive) != ARCHIVE_OK ||
        archive_write_zip_set_compression_store(archive) != ARCHIVE_OK ||
        archive_write_set_bytes_per_block(archive, 0) != ARCHIVE_OK) {

        printf("%d : %s\n", archive_errno(archive), archive_error_string(archive));
    }

    FILE *out = fopen("/tmp/test2.zip", "wb");
    archive_write_open_FILE(archive, out);

    for (int i = 0; i < children_len; i++) {
        char *filename = append_path(append_path(base_dir, "subdir1"), children[i]);
        struct stat st;
        stat(filename, &st);
        struct archive_entry *entry = archive_entry_new();
        archive_entry_set_pathname(entry, children[i]);
        archive_entry_set_size(entry, st.st_size);
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, 0644);
        // буфер будет заполнен в  archive_write_cb, вызванном из archive_write_data
        int r = archive_write_header(archive, entry);
        int fd = open(filename, O_RDONLY);
        char buff[1024];
        ssize_t len = read(fd, buff, sizeof(buff));
        while (len > 0) {
            archive_write_data(archive, buff, (size_t) len);
            len = read(fd, buff, sizeof(buff));
        }
        close(fd);
        archive_entry_free(entry);
    }

    archive_write_close(archive); // Note 4
    archive_write_free(archive); // Note 5
/*    uint8_t *buf = xcalloc(10240, sizeof(uint8_t));
    ssize_t len = 1;
    while (len != -1) {
        ssize_t l = stream_write_block(stream, buf, 10240);
        if (l > 0) {
            len = fwrite(buf, sizeof(uint8_t), (size_t) l, out);
        } else {
            len = l;
        }
    }*/
    stream_close(stream);

    //free(buf);
    fclose(out);
    storage_close(s);
}

void test_stream_archive(void) {
    Storage *s;
    storage_open(base_dir, &s);

    StreamArchiveStream *stream = NULL;
    storage_open_stream(s, "subdir1", &stream);

    FILE *out = fopen("/tmp/test.zip", "wb");
    uint8_t *buf = xcalloc(10240, sizeof(uint8_t));
    int *pipe = NULL;
    pipe = stream_pipe(stream);
    int tlen = 0;
    while (tlen != -1) {
        tlen = 0;
        ssize_t l = stream_write_block(stream);
        if (l == -1) {
            break;
        }
        assert(l >= 0);
        ssize_t readed = 0;
        if (poll(&(struct pollfd){ .fd = pipe[0], .events = POLLIN }, 1, 0)==1) {
            readed = read(pipe[0], buf, 10240);
        }
        while (readed > 0) {
            tlen += readed;
            fwrite(buf, sizeof(uint8_t), (size_t) readed, out);
            if (poll(&(struct pollfd){ .fd = pipe[0], .events = POLLIN }, 1, 0)==1) {
                readed = read(pipe[0], buf, 10240);
            } else {
                readed = 0;
            }
        }
    }
    stream_finish(stream);
    ssize_t readed = read(pipe[0], buf, 10240);
    while (readed > 0) {
        tlen += readed;
        fwrite(buf, sizeof(uint8_t), (size_t) readed, out);
        if (poll(&(struct pollfd){ .fd = pipe[0], .events = POLLIN }, 1, 0)==1) {
            readed = read(pipe[0], buf, 10240);
        } else {
            readed = 0;
        }
    }
    stream_close(stream);

    free(buf);
    fclose(out);
    storage_close(s);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Base dir arg missed");
        return -1;
    }
    base_dir = append_path(argv[1], "lpx-shared/test/test_dir");

    CU_pSuite pSuite = NULL;

    /* Initialize CUnit test registry */
    if (CUE_SUCCESS != CU_initialize_registry())
        return CU_get_error();

    /* Add suite to registry */
    pSuite = CU_add_suite("Basic_Test_Suite", init_suite, clean_suite);
    if (NULL == pSuite) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    /* add test to suite */
    ADD_TEST(pSuite, test_list_directory)
    ADD_TEST(pSuite, test_read_first_frame_meta)
    ADD_TEST(pSuite, test_read_last_frame_meta)
    ADD_TEST(pSuite, test_find_first_stream)
    ADD_TEST(pSuite, test_find_second_stream)
    //ADD_TEST(pSuite, test_stream_compress)
    ADD_TEST(pSuite, test_stream_archive);
    ADD_TEST(pSuite, test_stream_archive_file);

    /* Run tests using Basic interface */
    CU_basic_run_tests();

    int failures_count = CU_get_number_of_tests_failed();
    /* Clean up registry and return */

    CU_cleanup_registry();
    free(base_dir);
    return CU_get_error() || failures_count;
}