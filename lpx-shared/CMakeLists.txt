cmake_minimum_required (VERSION 2.6)

include_directories(include)

add_library(lpx src/list.c src/lpxstd.c src/stream.c src/stream_storage.c ../lpx-server/src/main.c)
add_executable(lpx-shared-test test/tests.c include/stream_storage.h)
target_link_libraries(lpx)
target_link_libraries(lpx-shared-test lpx cunit)

add_test(test_all lpx-shared-test ${PROJECT_BINARY_DIR})
