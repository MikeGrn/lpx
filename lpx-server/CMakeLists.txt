cmake_minimum_required (VERSION 2.6)

include_directories("${PROJECT_SOURCE_DIR}/lpx-shared/include")

add_executable(lpx-server src/main.c ../lpx-shared/src/bmp.c ../lpx-shared/include/bmp.h)
target_link_libraries(lpx-server lpx microhttpd)
