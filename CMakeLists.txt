cmake_minimum_required (VERSION 2.6)
project (lpx)
add_executable(lpx src/main.c src/bus.c src/bus.h src/libusb_debug_print.c src/lpxstd.h src/webcam.c src/webcam.h src/lpxstd.c)
target_link_libraries(lpx usb-1.0)