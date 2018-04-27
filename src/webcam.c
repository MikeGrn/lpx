#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <errno.h>
#include <linux/videodev2.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <time.h>

uint8_t *buffer;

int main(int argc, char *argv[]) {
    int fd;
    if ((fd = open("/dev/video0", O_RDWR)) < 0) {
        perror("open");
        exit(1);
    }

    struct v4l2_capability cap = {};
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        perror("VIDIOC_QUERYCAP");
        exit(1);
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "The device does not handle signle-planar video capture.\n");
        exit(1);
    }

    struct v4l2_format format;
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    format.fmt.pix.width = 640;
    format.fmt.pix.height = 480;
    format.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(fd, VIDIOC_S_FMT, &format) < 0) {
        perror("VIDIOC_S_FMT");
        exit(1);
    }

    struct v4l2_requestbuffers bufrequest = {0};
    bufrequest.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    bufrequest.memory = V4L2_MEMORY_MMAP;
    bufrequest.count = 1;
    if (ioctl(fd, VIDIOC_REQBUFS, &bufrequest) < 0) {
        perror("VIDIOC_REQBUFS");
        exit(1);
    }

    struct v4l2_buffer bufferinfo = {0};

    bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    bufferinfo.memory = V4L2_MEMORY_MMAP;
    bufferinfo.index = 0;
    if (ioctl(fd, VIDIOC_QUERYBUF, &bufferinfo) < 0) {
        perror("VIDIOC_QUERYBUF");
        exit(1);
    }


    buffer = mmap(NULL, bufferinfo.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, bufferinfo.m.offset);
    if (buffer == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    printf("Length: %d\nAddress: %p\n", bufferinfo.length, buffer);
    printf("Image Length: %d\n", bufferinfo.bytesused);

    memset(&bufferinfo, 0, sizeof(bufferinfo));
    bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    bufferinfo.memory = V4L2_MEMORY_MMAP;
    bufferinfo.index = 0;

    if (-1 == ioctl(fd, VIDIOC_STREAMON, &bufferinfo.type)) {
        perror("Start Capture");
        return 1;
    }

    int jpgfile;
    if ((jpgfile = open("/home/azhidkov/tmp/myimage.mjpeg", O_WRONLY | O_CREAT, 0660)) < 0) {
        perror("open");
        exit(1);
    }

    long deadline = time(NULL) + 10;
    int frame = 0;
    while (time(NULL) < deadline) {

        struct timeval stop, start;
        gettimeofday(&start, NULL);
        if (ioctl(fd, VIDIOC_QBUF, &bufferinfo) < 0) {
            perror("VIDIOC_QBUF");
            exit(1);
        }
        gettimeofday(&stop, NULL);
        printf("q time: %ld\n", stop.tv_usec - start.tv_usec);

        fd_set fds;
        struct timeval tv;
        int r;

        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        /* Timeout. */
        tv.tv_sec = 2;
        tv.tv_usec = 0;

        gettimeofday(&start, NULL);
        r = select(fd + 1, &fds, NULL, NULL, &tv);
        gettimeofday(&stop, NULL);
        printf("select time: %ld\n", stop.tv_usec - start.tv_usec);

        if (-1 == r) {
            if (EINTR == errno)
                continue;
            exit(EXIT_FAILURE);
        }

        if (0 == r) {
            fprintf(stderr, "select timeout\\n");
            exit(EXIT_FAILURE);
        }


        gettimeofday(&start, NULL);
        if (-1 == ioctl(fd, VIDIOC_DQBUF, &bufferinfo)) {
            perror("Retrieving Frame");
            return 1;
        }
        gettimeofday(&stop, NULL);
        printf("qd time: %ld\n", stop.tv_usec - start.tv_usec);
        printf("bi.seq: %d, .offset: %d, .bytesused: %d, .ts: %ld, .tc.frames: %d, .tc.seconds: %d\n", bufferinfo.sequence, bufferinfo.m.offset, bufferinfo.bytesused, bufferinfo.timestamp.tv_sec, bufferinfo.timecode.frames, bufferinfo.timecode.seconds);

        gettimeofday(&start, NULL);
        write(jpgfile, buffer, bufferinfo.length);
        gettimeofday(&stop, NULL);
        printf("write time: %ld\n", stop.tv_usec - start.tv_usec);
        frame++;
        printf("frame %d written, time: %ld\n", frame, time(NULL));
    }

    if (ioctl(fd, VIDIOC_STREAMOFF, &bufferinfo.type) < 0) {
        perror("VIDIOC_STREAMOFF");
        exit(1);
    }

    close(fd);


    close(jpgfile);

    printf("done\n");
    return EXIT_SUCCESS;
}
