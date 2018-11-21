cmake_minimum_required (VERSION 2.6)

include_directories(. /mnt/lpx/opt/vc/include /opt/vc/include)
link_directories(/opt/vc/lib)
include_directories("${PROJECT_SOURCE_DIR}/lpx-shared/include")

add_executable(lpx-control src/main.c src/camera.c src/train_sensor.c src/raspiraw.c)
target_link_libraries(lpx-control pthread wiringPi lpx mmal_core mmal_util mmal_vc_client vcos bcm_host)
