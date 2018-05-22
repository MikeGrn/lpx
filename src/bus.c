#include "bus.h"
#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <memory.h>
#include "libusb_debug_print.h"
#include "lpxstd.h"

const uint16_t MARKER = 0xA55A;

static const int BUS_VENDOR_ID = 0x0547;

static const int BUS_PRODUCT_ID = 0x101F;

static struct timeval NO_TIMEOUT = {0};


static libusb_context *ctx = NULL; // libusb session

static libusb_device *dev = NULL;

static libusb_device_handle *handle = NULL;

static const struct libusb_pollfd **usbPollFd;

static struct pollfd *fds;

static struct libusb_transfer *transfer;

static unsigned char buffer[8];

static enum BusState busState = NOT_INITIALIZED;

static int64_t trainId = 0;


static void find_device(libusb_device *const *devs, ssize_t cnt);

static void libusb_interrupt_cb(struct libusb_transfer *_transfer);

static int8_t bus_send_request(unsigned char *req, uint8_t reqlen);

static void poll_fd_added(int fd, short events, void *user_data) {
    printf("libusb poll fd added: %d, %d\n", fd, events);
}

static void poll_fd_removed(int fd, void *user_data) {
    printf("libusb poll fd removed: %d\n", fd);
}

int8_t bus_init() {
    libusb_device **devs; //pointer to pointer of device, used to retrieve a list of devices
    int r; //for return values
    ssize_t cnt; //holding number of devices in list
    r = libusb_init(&ctx); //initialize a library session
    if (r < 0) {
        fprintf(stderr, "Init Error %d\n", r);
        return -1;
    }
    libusb_set_pollfd_notifiers(ctx, poll_fd_added, poll_fd_removed, NULL);
    libusb_set_debug(ctx, 4); //set verbosity level to 3, as suggested in the documentation
    // TODO: handle libusb event timeouts
    if (0 == libusb_pollfds_handle_timeouts(ctx)) {
        printf("Should handle libusb event timeouts\n");
        return -1;
    }
    cnt = libusb_get_device_list(ctx, &devs); //get the list of devices
    if (cnt < 0) {
        fprintf(stderr, "Get device error\n");
        return -1;
    }

    find_device(devs, cnt);
    if (dev) {
        printf("Device found open it\n");
        r = libusb_open(dev, &handle);
        if (LIBUSB_SUCCESS != r) {
            return -1;
        }
        r = libusb_kernel_driver_active(handle, 0);
        printf("Kernel driver active: %d\n", r);
        r = libusb_claim_interface(handle, 0);
        if (LIBUSB_SUCCESS != r) {
            perror("claim usb interface");
            printf("Could not claim interface\n");
            return -1;
        }
        transfer = libusb_alloc_transfer(0);
        if (!transfer) {
            printf("could not allocate transfer\n");
            return -1;
        }
        libusb_fill_interrupt_transfer(transfer, handle, 0x88, buffer, sizeof(buffer), libusb_interrupt_cb, NULL, 0);
        r = libusb_submit_transfer(transfer);
        printf("transfer submitted\n");
        if (LIBUSB_SUCCESS != r) {
            return -1;
        }
        busState = UNKNOWN;
        //print_device(dev);
    }

    libusb_free_device_list(devs, 1); //free the list, unref the devices in it

    if (dev) {
        return 0;
    } else {
        return -1;
    }
}

static void find_device(libusb_device *const *devs, ssize_t cnt) {
    struct libusb_device_descriptor desc;
    for (int i = 0; i < cnt; i++) {
        int ret = libusb_get_device_descriptor(devs[i], &desc);
        if (ret < 0) {
            fprintf(stderr, "failed to get device descriptor");
            continue;
        }
        if (desc.idVendor == BUS_VENDOR_ID && desc.idProduct == BUS_PRODUCT_ID) {
            dev = devs[i];
            break;
        }
    }
}

struct pollfd *bus_fds(uint8_t *len) {
    usbPollFd = libusb_get_pollfds(ctx);

    uint8_t fdsCnt = 0;
    for (; usbPollFd[fdsCnt] != NULL; fdsCnt++) {}

    fds = calloc(fdsCnt, sizeof(struct pollfd));
    for (int i = 0; i < fdsCnt; i++) {
        fds[i].fd = usbPollFd[i]->fd;
        fds[i].events = usbPollFd[i]->events;
    }

    *len = fdsCnt;
    return fds;
}

enum BusState bus_state() {
    return busState;
}

static void libusb_interrupt_cb(struct libusb_transfer *_transfer) {
    printf("status: %d\n", _transfer->status);
    if (LIBUSB_TRANSFER_CANCELLED == _transfer->status) {
        return;
    }
    printf("Interrupt len: %d", _transfer->actual_length);
    unsigned char *buffer = _transfer->buffer;
    printf(", data: [%d, %d, %d, %d, %d, %d]\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
    printf("\n");
    unsigned char intStatus = _transfer->buffer[4];
    if (1 == intStatus) {
        // сигнал дальнего оповещения
        assert(UNKNOWN == busState || WAIT_TRAIN == busState && "Unexpected bus state");
        busState = TRAIN;
        trainId = time(NULL);
    } else if (0 == intStatus) {
        // поезд завершился
        assert(UNKNOWN == busState || TRAIN == busState && "Unexpected bus state");
        busState = WAIT_TRAIN;
    } else if (3 == intStatus) {
        // прошло первое колесо
        assert(UNKNOWN == busState || TRAIN == busState && "Unexpected bus state");
    } else {
        fprintf(stderr, "Unexpected interrupt status %d", intStatus);
    }
    int sret = libusb_submit_transfer(transfer);
    printf("sret: %d\n", sret);
}

int64_t bus_trainId() {
    return trainId;
}

int8_t bus_read_response(void *res, int8_t reslen) {
    int len = 0;
    int r = libusb_bulk_transfer(handle, 0x86, res, reslen, &len, 0);
    if (LIBUSB_SUCCESS != r || reslen != len) {
        return -1;
    }
    printArray("Rx", res, reslen);
    return 0;
}

/**
 * Работает в блокируещем режиме
 */
int8_t bus_request_state() {
    uint8_t stateSize = sizeof(struct TMsbState);
    unsigned char outbuf[10];
    outbuf[0] = 0x23;
    outbuf[1] = 0;
    outbuf[2] = (unsigned char) (stateSize / 2);
    struct TMsbState state;
    int r = bus_send_request((unsigned char[3]) {0x23, 0, sizeof(state) / 2}, 3);
    if (LIBUSB_SUCCESS != r) {
        return -1;
    }
    r = bus_read_response(&state, sizeof(state));
    if (LIBUSB_SUCCESS != r) {
        return -1;
    }
    assert(MARKER == state.marker && "Unexpected marker");
    int len = 0;
    uint32_t trainSize = sizeof(struct TMsbTrainHdr) / sizeof(uint32_t);
    outbuf[0] = 0x24;
    outbuf[1] = 0;

    outbuf[2] = (unsigned char) (state.lastTrain & 0xFFu);
    outbuf[3] = (unsigned char) ((state.lastTrain >> 8u) & 0xFFu);
    outbuf[4] = (unsigned char) ((state.lastTrain >> 16u) & 0xFFu);
    outbuf[5] = (unsigned char) (state.lastTrain >> 24u);

    outbuf[6] = (unsigned char) (trainSize & 0xFFu);
    outbuf[7] = (unsigned char) ((trainSize >> 8u) & 0xFFu);
    outbuf[8] = (unsigned char) ((trainSize >> 16u) & 0xFFu);
    outbuf[9] = (unsigned char) (trainSize >> 24u);

    r = libusb_bulk_transfer(handle, 0x2, (unsigned char *) &outbuf, sizeof(outbuf), &len, 0);
    printf("1 bulk transfer len: %d, r: %d\n", len, r);
    unsigned char trainBuf[trainSize * sizeof(uint32_t)];
    memset(trainBuf, 0, sizeof(trainBuf));
    int inlen = 0;
    printf("sending read bulk transfer\n");
    r = libusb_bulk_transfer(handle, 0x86, trainBuf, sizeof(trainBuf), &inlen, 0);
    printf("res: %d, inlen: %d\n", r, inlen);
    printArray("train data: ", trainBuf, inlen);
    struct TMsbTrainHdr *hdr = (struct TMsbTrainHdr *) &trainBuf;
    printf("hdr.marker: %d (%2X)\n", hdr->marker, hdr->marker);
    printf("hdr.numberOfWheels: %d\n", hdr->numberOfWheels);
    printf("hdr.timeStart: %d \n", hdr->timeStart);
    printf("hdr.timeStop: %d \n", hdr->timeStop);
    printf("hdr.timeFirstWheel: %d \n", hdr->timeFirstWheel);
    printf("hdr.trFirstSignalTime: %d \n", hdr->trFirstSignalTime);
    outbuf[2] = hdr->thisTrainData & 0xFF;
    outbuf[3] = (hdr->thisTrainData >> 8) & 0xFF;
    outbuf[4] = (hdr->thisTrainData >> 16) & 0xFF;
    outbuf[5] = (hdr->thisTrainData >> 24);

    int wheelsSize = sizeof(struct TMsbWheelTime) * hdr->numberOfWheels / sizeof(uint32_t);
    outbuf[6] = wheelsSize & 0xFF;
    outbuf[7] = (wheelsSize >> 8) & 0xFF;
    outbuf[8] = (wheelsSize >> 16) & 0xFF;
    outbuf[9] = (wheelsSize >> 24);

    r = libusb_bulk_transfer(handle, 0x2, (unsigned char *) &outbuf, sizeof(outbuf), &len, 0);
    unsigned char wheelsBuf[wheelsSize * sizeof(uint32_t)];
    inlen = 0;
    r = libusb_bulk_transfer(handle, 0x86, wheelsBuf, sizeof(wheelsBuf), &inlen, 0);
    printArray("wheels data: ", wheelsBuf, inlen);
    uint32_t *time = (uint32_t *) &wheelsBuf;
    uint32_t baseTimeOffsetMks = s2mks(hdr->timeFirstWheel - hdr->timeStart);
    // todo: free
    uint32_t *wheelTimes = malloc(sizeof(uint32_t) * hdr->numberOfWheels);
    for (int i = 0; i < hdr->numberOfWheels * 6; i += 6) {
        uint64_t sum = 0;
        uint8_t items = 0;
        for (int j = 0; j < 6; j++) {
            printf("%d ", *time);
            if (*time > 0) {
                items++;
            }
            sum += *time;
            time++;
        }
        wheelTimes[i / 6] = (uint32_t) (baseTimeOffsetMks + sum / items);
        printf("%d \n", wheelTimes[i / 6]);
    }

    printf("\n");
    return 0;
}

static int8_t bus_send_request(unsigned char *req, uint8_t reqlen) {
    int len = 0;
    int r = libusb_bulk_transfer(handle, 0x2, req, reqlen, &len, 0);
    if (LIBUSB_SUCCESS != r || len != reqlen) {
        return -1;
    }
    return 0;
}

int8_t bus_handle_events() {
    return (int8_t) libusb_handle_events_timeout(ctx, &NO_TIMEOUT);
}

void bus_close() {
    if (transfer) {
        libusb_cancel_transfer(transfer);
        libusb_handle_events(ctx);
        libusb_free_transfer(transfer);
    }
    if (fds) {
        free(fds);
    }
    if (usbPollFd) {
        libusb_free_pollfds(usbPollFd);
    }
    if (handle) {
        libusb_release_interface(handle, 0);
        libusb_close(handle);
    }
    libusb_exit(ctx);
    busState = CLOSED;
}
