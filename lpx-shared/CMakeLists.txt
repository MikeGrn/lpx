cmake_minimum_required (VERSION 2.6)
include_directories(include)

add_library(lpx src/list.c src/lpxstd.c src/stream.c src/stream_storage.c ../lpx-server/src/main.c)
