#ifndef LPX_WEBCAM_H
#define LPX_WEBCAM_H

#include <poll.h>
#include <stdbool.h>
#include <stdint.h>

int webcam_init();

struct pollfd* webcam_fds(uint8_t *len);

int webcam_handle_frame(int32_t trainId, bool last);

int webcam_close();

#endif //LPX_WEBCAM_H
