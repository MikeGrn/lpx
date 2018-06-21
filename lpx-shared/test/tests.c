#include <CUnit/CUnit.h>
#include <lpxstd.h>
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

void test_stream_compress(void) {
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
    ADD_TEST(pSuite, test_stream_compress)

    /* Run tests using Basic interface */
    CU_basic_run_tests();

    int failures_count = CU_get_number_of_tests_failed();
    /* Clean up registry and return */

    CU_cleanup_registry();
    free(base_dir);
    return CU_get_error() || failures_count;
}