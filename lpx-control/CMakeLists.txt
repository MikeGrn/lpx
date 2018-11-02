cmake_minimum_required (VERSION 2.6)

include_directories("${PROJECT_SOURCE_DIR}/lpx-shared/include")

add_executable(lpx-control src/main.c src/camera.c src/train_sensor.c)
target_link_libraries(lpx-control pthread wiringPi lpx)
