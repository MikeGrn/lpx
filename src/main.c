#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <signal.h>
#include <assert.h>
#include <stdlib.h>
#include "lpxstd.h"
#include "bus.h"
#include "webcam.h"
#include <curl/curl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>

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
                    // TODO: освободить офсеты
                    uint64_t *wheelOffsets = NULL;
                    uint32_t timeOffsetsLen = 0;
                    r = bus_last_train_wheel_time_offsets(&wheelOffsets, &timeOffsetsLen);
                    assert(0 == r);

                    struct FrameMeta *frames = webcam_last_stream_index();
                    printf("Frames index:\n");
                    for (int i = 0; frames[i].size != 0; i++) {
                        printf("%d,%d,%" PRId64 ",%" PRId64 "\n", frames[i].offset, frames[i].size, frames[i].startTime, frames[i].endTime);
                    }
                    int64_t streamBase = frames->startTime;
                    printf("timeOffsetsLen: %d\n", timeOffsetsLen);
                    int f = 0;
                    for (int i = 0; i < timeOffsetsLen; i++) {
                        printf("%ld ", wheelOffsets[i]);

                        // todo: сделать нормальную границу
                        while ((frames[f + 1]).size != 0) {
                            int64_t frameOffset = frames[f].startTime - streamBase;
                            int64_t nextFrameOffset = frames[f + 1].startTime - streamBase;
                            if (labs(nextFrameOffset - wheelOffsets[i]) > labs(frameOffset - wheelOffsets[i])) {
                                printf("frame found idx: %d, ws: %ld, fs: %ld\n", f, wheelOffsets[i], (frames[f + 1].startTime - streamBase)); // на втором фрейме видно огоньки
                                int64_t trainId = bus_trainId();
                                unsigned char *frame = webcam_get_frame(trainId, frames[f]);
                                char nbuf[256];
                                sprintf(nbuf, "/home/azhidkov/tmp/lpx-out/%" PRId64 "-%d.jpeg", trainId, i);
                                FILE *ft = fopen(nbuf, "wb");
                                fwrite(frame, 1, frames[f].size, ft);
                                fclose(ft);

                                CURL *curl;
                                CURLcode res;

                                struct curl_httppost *formpost = NULL;
                                struct curl_httppost *lastptr = NULL;
                                struct curl_slist *headerlist = NULL;

                                curl_global_init(CURL_GLOBAL_ALL);

                                /* Fill in the file upload field */
                                curl_formadd(&formpost,
                                             &lastptr,
                                             CURLFORM_COPYNAME, "file",
                                             CURLFORM_FILE, nbuf,
                                             CURLFORM_END);

                                /* Fill in the filename field */
                                curl_formadd(&formpost,
                                             &lastptr,
                                             CURLFORM_COPYNAME, "filename",
                                             CURLFORM_COPYCONTENTS, nbuf,
                                             CURLFORM_END);


                                curl = curl_easy_init();
                                /* initialize custom header list (stating that Expect: 100-continue is not
                                   wanted */
                                if(curl) {
                                    /* what URL that receives this POST */
                                    curl_easy_setopt(curl, CURLOPT_URL, "http://192.168.1.236:8000/upload");
                                    curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);

                                    /* Perform the request, res will get the return code */
                                    res = curl_easy_perform(curl);
                                    /* Check for errors */
                                    if(res != CURLE_OK)
                                        fprintf(stderr, "curl_easy_perform() failed: %s\n",
                                                curl_easy_strerror(res));

                                    /* always cleanup */
                                    curl_easy_cleanup(curl);

                                    /* then cleanup the formpost chain */
                                    curl_formfree(formpost);
                                    /* free slist */
                                    curl_slist_free_all(headerlist);
                                }
                                break;
                            }
                            f++;
                        }
                    }
                    printf("\n");

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