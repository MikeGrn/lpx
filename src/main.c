#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <signal.h>
#include <assert.h>
#include "lpxstd.h"
#include "bus.h"

volatile int stop = 0;

static void handler(int signum) {
    printf("signal: %d\n", signum);
    stop = 1;
}

int main() {
    signal(SIGINT, handler);
    int r = bus_init();
    if (r < 0) {
        printf("Could not initialize bus");
        return -1;
    }

    uint8_t busFdsCnt;
    struct pollfd *busFds = bus_fds(&busFdsCnt);

    struct pollfd fds[busFdsCnt + 1];
    fds[0].fd = 0;
    fds[0].events = POLLIN;

    for (int i = 0; i < busFdsCnt; i++) {
        fds[i + 1] = busFds[i];
    }

    while (!stop) {
        r = poll(fds, ALEN(fds), -1);
        assert(r >= 0); // TODO
        printf("pret: %d\n", r);
        if (r > 0) {
            for (int i = 0; i < busFdsCnt; i++) {
                if (busFds[i].revents == busFds[i].events) {
                    r = bus_handle_events();
                    assert(r == 0); // TODO
                    break;
                }
            }
            if (fds[0].revents == POLLIN) {
                break;
            }
        }
    }

    bus_close();
    return 0;
}