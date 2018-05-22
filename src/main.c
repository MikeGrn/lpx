#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <signal.h>
#include <assert.h>
#include <stdlib.h>
#include "lpxstd.h"
#include "bus.h"
#include "webcam.h"

volatile int stop = 0;

static const int STDIN = 0;
static const int WEBCAM = 1;
static const int BUS = 2;

static void handler(int signum) {
    printf("signal: %d\n", signum);
    stop = 1;
}

int main() {
    signal(SIGINT, handler);
    int r = bus_init();
    if (r < 0) {
        printf("Could not initialize bus\n");
        return -1;
    }
    r = webcam_init("/home/azhidkov/tmp/lpx-out"); // TODO: mkdir
    if (r < 0) {
        printf("Could not initialize webcam\n");
        return -1;
    }

    uint8_t busFdsCnt;
    struct pollfd *busFds = bus_fds(&busFdsCnt);

    struct pollfd fds[busFdsCnt + 2];
    fds[STDIN].fd = 0;
    fds[STDIN].events = POLLIN;

    struct pollfd webcamFd = webcam_fd();
    fds[WEBCAM].fd = webcamFd.fd;
    fds[WEBCAM].events = webcamFd.events;

    for (int i = 0; i < busFdsCnt; i++) {
        fds[BUS + i] = busFds[i];
    }

    while (!stop) {
        if (webcam_streaming()) {
            fds[WEBCAM].fd = abs(fds[WEBCAM].fd);
        } else {
            fds[WEBCAM].fd = -abs(fds[WEBCAM].fd);
        }
        r = poll(fds, ALEN(fds), -1);
        assert(r >= 0); // TODO
        if (r <= 0) {
            printf("pret: %d\n", r);
        }
        if (r > 0) {
            for (int i = BUS; i < BUS + busFdsCnt; i++) {
                if (fds[i].revents == fds[i].events) {
                    // todo: засирает busFds - надо разобраться почему (может из-за бага с alloca-ом busFds?)
                    enum BusState prevState = bus_state();
                    r = bus_handle_events();
                    assert(0 == r); // TODO
                    enum BusState curState = bus_state();
                    if (prevState != curState && TRAIN == curState) {
                        webcam_start_stream(bus_trainId());
                    }
                    break;
                } else if (fds[i].revents == POLLERR) {
                    printf("Disable %d(%d) libusb poll fd\n", fds[i].fd, fds[i].events);
                    perror("libusb poll error");
                    fds[i].fd = -fds[i].fd;
                    break;
                }
            }
            if (POLLIN == fds[WEBCAM].revents) {
                webcam_handle_frame(bus_trainId(), WAIT_TRAIN == bus_state());
                // TODO: вынести обработку поезда в отдельный поток
                if (WAIT_TRAIN == bus_state()) {
                    r = bus_last_train_wheel_time_offsets();
                    assert(0 == r);
                }
                // TODO: удалить стрим
            }
            if (POLLERR == fds[WEBCAM].revents) {
                perror("webcam error");
            }
            if (POLLIN == fds[STDIN].revents) {
                // выходим по любому инпуту на stdin
                break;
            }
        }
    }

    bus_close();
    webcam_close();
    return 0;
}