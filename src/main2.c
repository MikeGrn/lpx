#include <stdio.h>
#include <inttypes.h>
#include <time.h>
#include "list.h"
#include "lpxstd.h"
#include "stream_storage.h"
#include "webcam2.h"
#include "unistd.h"

static void ec(Webcam *w, int errcode) {
    fprintf(stderr, "Webcam error, code: %d\n", errcode);
}

int main() {
    Storage *s;
    storage_open("/home/azhidkov/tmp/lpx-out", &s);

    Webcam *w;
    webcam_init(s, "/dev/video0", &w, ec);

    if (LPX_SUCCESS != webcam_start_stream(w, "1")) {
        printf("error\n");
        goto cleanup;
    }
    struct timespec tv = { .tv_sec = 0, .tv_nsec = 1000 };
    nanosleep(&tv, 0);
    webcam_stop_stream(w);

    if (LPX_SUCCESS != webcam_start_stream(w, "2")) {
        printf("error\n");
        goto cleanup;
    }
    sleep(1);
    webcam_stop_stream(w);

    cleanup:
    webcam_close(w);
    storage_close(s);
}
