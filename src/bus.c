#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <memory.h>
#include <stdbool.h>
#include <pthread.h>
#include <assert.h>
#include <zconf.h>
#include <poll.h>
#include <sys/eventfd.h>
#include "bus.h"
#include "lpxstd.h"

static const int BUS_VENDOR_ID = 0x0547;

static const int BUS_PRODUCT_ID = 0x101F;

const uint16_t STATE_MARKER = 0xA55A;

const uint16_t TRAIN_MARKER = 0xA5A5;

typedef struct Bus {
    libusb_context *ctx;
    libusb_device *dev;
    libusb_device_handle *handle;
    void *user_data; // пользовательские данные, передаваемые в коллбэк
    interrupt_callback icb;
    bool running;
    pthread_mutex_t running_mutex;
    pthread_t tid;
} Bus;

static bool isRunning(Bus *bus) {
    bool ret = 0;
    int r = pthread_mutex_lock(&bus->running_mutex);
    assert(r == 0 && "Could not not lock running_mutex");
    ret = bus->running;
    r = pthread_mutex_unlock(&bus->running_mutex);
    assert(r == 0 && "Could not not unlock running_mutex");
    return ret;
}

static void *handle_device(void *b) {
    printf("Bus thread started\n");
    Bus *bus = b;
    unsigned char buffer[8];
    int len = 0;
    while (isRunning(bus)) {
        // Этот вызов блокирует завершение программы на время таймаута
        // Но т.к. использование асинхронного АПИ сильно усложняет логику, а в штатном режиме программа выходить не должна,
        // оставляю эту задержку
        int ret = libusb_interrupt_transfer(bus->handle, 0x88, buffer, sizeof(buffer), &len, 1000);
        if (LIBUSB_ERROR_TIMEOUT == ret) {
            continue;
        } else if (LIBUSB_SUCCESS != ret) {
            bus->icb(bus->user_data, BUS_IFACE, -1);
            break;
        }
        unsigned char intStatus = buffer[4];
        if (bus->icb) {
            bus->icb(bus->user_data, LPX_SUCCESS, intStatus);
        }
    }
    return NULL;
}

static libusb_device *find_device(libusb_device **devs, ssize_t cnt) {
    struct libusb_device_descriptor desc;
    for (int i = 0; i < cnt; i++) {
        int ret = libusb_get_device_descriptor(devs[i], &desc);
        if (ret < 0) {
            fprintf(stderr, "failed to get device descriptor");
            continue;
        }
        if (desc.idVendor == BUS_VENDOR_ID && desc.idProduct == BUS_PRODUCT_ID) {
            return devs[i];
        }
    }
    return NULL;
}


int32_t bus_init(Bus **bus, void *user_data, interrupt_callback icb) {
    int8_t res = LPX_SUCCESS;
    *bus = xmalloc(sizeof(Bus));
    memset(*bus, 0, sizeof(Bus));
    Bus *b = *bus;
    b->user_data = user_data;
    b->running = 1;
    b->icb = icb;
    libusb_device **devs;
    int r;
    r = libusb_init(&b->ctx);
    if (r < 0) {
        res = BUS_IFACE;
        fprintf(stderr, "Init Error %d\n", r);
        goto free_bus;
    }
    //libusb_set_debug(b->ctx, 4);
    ssize_t cnt = libusb_get_device_list(b->ctx, &devs);
    if (cnt < 0) {
        res = BUS_IFACE;
        fprintf(stderr, "Get device error\n");
        goto close_libusb;
    }

    b->dev = find_device(devs, cnt);
    libusb_free_device_list(devs, 1);
    if (b->dev == NULL) {
        res = BUS_IFACE;
        fprintf(stderr, "Bus device not found\n");
        goto close_libusb;
    }
    printf("Device found open it\n");
    r = libusb_open(b->dev, &b->handle);
    if (LIBUSB_SUCCESS != r) {
        res = BUS_IFACE;
        fprintf(stderr, "Could not open device\n");
        goto close_libusb;
    }
    r = libusb_claim_interface(b->handle, 0);
    if (LIBUSB_SUCCESS != r) {
        res = BUS_IFACE;
        perror("claim usb interface");
        goto close_handle;
    }

    r = pthread_mutex_init(&b->running_mutex, NULL);
    if (0 != r) {
        res = BUS_THREAD;
        fprintf(stderr, "Could not initialize bus running_mutex, errcode: %d\n", r);
        goto close_handle;
    }

    pthread_t tid;
    r = pthread_create(&tid, NULL, handle_device, b);
    if (0 != r) {
        res = BUS_THREAD;
        fprintf(stderr, "Could not create bus thread, errcode: %d\n", r);
        goto destroy_mutex;
    }
    b->tid = tid;

    return res;

    // завершение по ошибке
    destroy_mutex:
    pthread_mutex_destroy(&b->running_mutex);

    close_handle:
    libusb_close(b->handle);

    close_libusb:
    libusb_exit(b->ctx);

    free_bus:
    free(*bus);

    return res;
}

static int8_t bus_read_response(Bus *bus, void *const buf, uint32_t bufLen, uint16_t marker) {
    uint32_t respLen = bufLen;
    unsigned char resp[respLen];
    memset(resp, 0, respLen);
    int len = 0;
    int r = libusb_bulk_transfer(bus->handle, 0x86, resp, respLen, &len, 0);
    if (LIBUSB_SUCCESS != r || len != respLen) {
        return BUS_IFACE;
    }
    if (marker != 0 && *((uint16_t*) resp) != marker) {
        fprintf(stderr, "Marker %d received, while %d expected", *((uint16_t*)resp), marker);
        return BUS_PROTOCOL;
    }

    memcpy(buf, &resp, bufLen);
    return LPX_SUCCESS;
}

static int32_t bus_read_fpga(Bus *bus, struct TMsbState *const state) {
    unsigned char req[3] = {0};
    req[0] = 0x23;
    req[1] = 0;
    req[2] = sizeof(struct TMsbState) / 2;
    int len = 0;
    int32_t r = libusb_bulk_transfer(bus->handle, 0x2, req, sizeof(req), &len, 0);
    if (LIBUSB_SUCCESS != r || len != sizeof(req)) {
        return BUS_IFACE;
    }
    r = bus_read_response(bus, state, sizeof(struct TMsbState), STATE_MARKER);
    return r;
}

static int32_t bus_read_train_data(Bus *bus, void *const buf, uint32_t bufLen, int32_t addr, uint16_t marker) {
    unsigned char req[10] = {0};
    req[0] = 0x24;
    memcpy(&(req[2]), &addr, sizeof(uint32_t));
    uint32_t trainSizeDWords = bufLen / sizeof(uint32_t);
    memcpy(&(req[6]), &trainSizeDWords, sizeof(uint32_t));

    int len;
    int32_t r = libusb_bulk_transfer(bus->handle, 0x2, (unsigned char *) &req, sizeof(req), &len, 0);
    if (LIBUSB_SUCCESS != r || len != sizeof(req)) {
        return BUS_IFACE;
    }
    r = bus_read_response(bus, buf, bufLen, marker);
    return r;
}

int32_t bus_last_train_wheel_time_offsets(Bus *bus, uint64_t **timeOffsets, uint32_t *len) {
    struct TMsbState state = {0};
    int32_t r = bus_read_fpga(bus, &state);
    if (r != LIBUSB_SUCCESS) {
        return r;
    } else if (UINT32_MAX == state.lastTrain) {
        fprintf(stderr, "No last train address");
        return BUS_DEVICE;
    }

    struct TMsbTrainHdr hdr = {0};
    r = bus_read_train_data(bus, &hdr, sizeof(hdr), state.lastTrain, TRAIN_MARKER);
    if (r != LIBUSB_SUCCESS) {
        return r;
    }

    struct TMsbWheelTime wheelTimes[hdr.numberOfWheels];
    memset(&wheelTimes, 0, sizeof(struct TMsbWheelTime) * hdr.numberOfWheels);
    r = bus_read_train_data(bus, &wheelTimes, sizeof(struct TMsbWheelTime) * hdr.numberOfWheels, hdr.thisTrainData, 0);
    if (r != LIBUSB_SUCCESS) {
        return r;
    }

    uint64_t baseTimeOffsetMks = hdr.trFirstWheelTime;

    uint64_t *wheelOffsets = xmalloc(sizeof(uint64_t) * hdr.numberOfWheels);
    for (int i = 0; i < hdr.numberOfWheels; i++) {
        wheelOffsets[i] = baseTimeOffsetMks + wheelTimes[i].rightStart;
    }
    *timeOffsets = wheelOffsets;
    *len = hdr.numberOfWheels;

    return LPX_SUCCESS;
}

static void stop(Bus *bus) {
    int r = pthread_mutex_lock(&bus->running_mutex);
    assert(r == 0 && "Could not not lock running_mutex");
    bus->running = 0;
    r = pthread_mutex_unlock(&bus->running_mutex);
    printf("Stop flag is set\n");
    assert(r == 0 && "Could not not unlock running_mutex");
}

void bus_close(Bus *bus) {
    stop(bus);
    pthread_join(bus->tid, NULL);
    if (bus->handle) {
        libusb_release_interface(bus->handle, 0);
        libusb_close(bus->handle);
    }
    libusb_exit(bus->ctx);
    free(bus);
}