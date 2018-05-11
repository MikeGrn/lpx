#include "bus.h"
#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

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

static uint32_t trainId = 0;


static void find_device(libusb_device *const *devs, ssize_t cnt);

static void libusb_cb(struct libusb_transfer *_transfer);

int8_t bus_init() {
    libusb_device **devs; //pointer to pointer of device, used to retrieve a list of devices
    int r; //for return values
    ssize_t cnt; //holding number of devices in list
    r = libusb_init(&ctx); //initialize a library session
    if (r < 0) {
        fprintf(stderr, "Init Error %d\n", r);
        return -1;
    }
    cnt = libusb_get_device_list(ctx, &devs); //get the list of devices
    if (cnt < 0) {
        fprintf(stderr, "Get device error\n");
        return -1;
    }

    find_device(devs, cnt);
    if (dev) {
        libusb_open(dev, &handle);
        transfer = libusb_alloc_transfer(0);
        libusb_fill_interrupt_transfer(transfer, handle, 0x88, buffer, sizeof(buffer), libusb_cb, NULL, 0);
        libusb_submit_transfer(transfer);
        busState = UNKNOWN;
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

    fds = alloca(sizeof(struct pollfd) * fdsCnt);
    for (int i = 0; i < fdsCnt; i++) {
        fds[i].fd = usbPollFd[i]->fd;
        fds[i].events = usbPollFd[i]->events;
    }

    *len = fdsCnt;
    return fds;
}

static void libusb_cb(struct libusb_transfer *_transfer) {
    printf("status: %d\n", _transfer->status);
    if (_transfer->status == LIBUSB_TRANSFER_CANCELLED) {
        return;
    }
    printf("Interrupt len: %d", _transfer->actual_length);
    unsigned char *buffer = _transfer->buffer;
    printf(", data: [%d, %d, %d, %d, %d, %d]\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
    printf("\n");
    unsigned char intStatus = _transfer->buffer[4];
    if (intStatus == 1) {
        // сигнал дальнего оповещения
        assert(busState == UNKNOWN || busState == WAIT_TRAIN && "Unexpected bus state");
        busState = TRAIN;
        trainId++;
    } else if (intStatus == 0) {
        // поезд завершился
        assert(busState == UNKNOWN || busState == TRAIN && "Unexpected bus state");
        busState = WAIT_TRAIN;
    } else if (intStatus == 3) {
        // прошло первое колесо
        assert(busState == UNKNOWN || busState == TRAIN && "Unexpected bus state");
    } else {
        fprintf(stderr, "Unexpected interrupt status %d", intStatus);
    }
    int sret = libusb_submit_transfer(transfer);
    printf("sret: %d\n", sret);
}

uint32_t bus_trainId() {
    return trainId;
}

int8_t bus_handle_events() {
    libusb_handle_events_timeout(ctx, &NO_TIMEOUT);
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
        libusb_close(handle);
    }
    libusb_exit(ctx);
    busState = CLOSED;
}
