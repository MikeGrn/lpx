#include <stdio.h>
#include <libusb-1.0/libusb.h>
#include <string.h>
#include <poll.h>
#include <signal.h>

static struct libusb_transfer *transfer;

static void libusb_cb(struct libusb_transfer *_transfer) {
    printf("status: %d\n", _transfer->status);
    if (_transfer->status == LIBUSB_TRANSFER_CANCELLED) {
        return;
    }
    printf("Interrupt len: %d", _transfer->actual_length);
    unsigned char* buffer = _transfer->buffer;
    printf(", data: [%d, %d, %d, %d, %d, %d]\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
    printf("\n");
    int sret = libusb_submit_transfer(transfer);
    printf("sret: %d\n", sret);
}

volatile int stop = 0;

static void handler(int signum) {
    printf("signal: %d\n", signum);
    stop = 1;
}

int main() {
    signal(SIGINT, handler);
    libusb_device **devs; //pointer to pointer of device, used to retrieve a list of devices
    libusb_context *ctx = NULL; //a libusb session
    int r; //for return values
    ssize_t cnt; //holding number of devices in list
    r = libusb_init(&ctx); //initialize a library session
    if (r < 0) {
        printf("Init Error %d\n", r);
        return 1;
    }
    libusb_set_debug(ctx, 3); //set verbosity level to 3, as suggested in the documentation
    cnt = libusb_get_device_list(ctx, &devs); //get the list of devices
    if (cnt < 0) {
        printf("Get device error\n");
    }
    printf("%zi devices in list\n", cnt);
    ssize_t i; //for iterating through the list
    //print_device(devs[4]);

    struct libusb_device_descriptor desc;
    libusb_device *dev = NULL;
    for (i = 0; i < cnt; i++) {
        int ret = libusb_get_device_descriptor(devs[i], &desc);
        if (ret < 0) {
            fprintf(stderr, "failed to get device descriptor");
            continue;
        }
        if (desc.idVendor == 0x0547 && desc.idProduct == 0x101F) {
            dev = devs[i];
            break;
        }
    }
    libusb_free_device_list(devs, 1); //free the list, unref the devices in it
    if (dev) {
        libusb_device_handle *handle = NULL;
        int ret = libusb_open(dev, &handle);
        if (LIBUSB_SUCCESS == ret) {
            printf("Device opened\n");

            transfer = libusb_alloc_transfer(0);
            unsigned char buffer[8]; // TODO: move to heap
            libusb_fill_interrupt_transfer(transfer, handle, 0x88, buffer, sizeof(buffer), libusb_cb, NULL, 0);
            libusb_submit_transfer(transfer);
            const struct libusb_pollfd **usbPollFd = libusb_get_pollfds(ctx);
            struct pollfd fds[4];
            for (int i = 0; usbPollFd[i] != NULL; i++) {
                // TODO: handle array index out of bounds
                fds[i].fd = usbPollFd[i]->fd;
                fds[i].events = usbPollFd[i]->events;
            }
            fds[3].fd = 0;
            fds[3].events = POLLIN;
            while (!stop) {
                printf("polling\n");
                int pret = poll(fds, 4, -1);
                printf("pret: %d\n", pret);
                if (pret > 0) {
                    printf("revents0: %d\n", fds[0].revents);
                    printf("revents1: %d\n", fds[1].revents);
                    printf("revents2: %d\n", fds[2].revents);
                    printf("revents3: %d\n", fds[3].revents);
                    if (fds[0].revents & POLLIN || fds[1].revents & POLLIN || fds[2].revents & POLLOUT) {
                        printf("handle events\n");
                        struct timeval tv = {0};
                        int heret = libusb_handle_events_timeout(ctx, &tv);
                        printf("heret: %d\n", heret);
                    }
                    if (fds[3].revents & POLLIN) {
                        break;
                    }
                }
            }
            libusb_cancel_transfer(transfer);
            libusb_handle_events(ctx);
            libusb_free_transfer(transfer);
        }

        if (handle) {
            libusb_close(handle);
        }
    } else {
        printf("Device not found\n");
    }
    libusb_exit(ctx); //close the session
    return 0;
}