#ifndef LPX_BUS_H
#define LPX_BUS_H

#include <poll.h>
#include <stdint.h>

enum BusState {
    NOT_INITIALIZED,
    UNKNOWN,
    WAIT_TRAIN,
    TRAIN,
    CLOSED
};

int8_t bus_init();

struct pollfd* bus_fds(uint8_t *len);

enum BusState bus_state();

int64_t bus_trainId();

int8_t bus_handle_events();

void bus_close();

#endif //LPX_BUS_H
