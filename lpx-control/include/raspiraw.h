#ifndef LPX_RASPIRAW_H
#define LPX_RASPIRAW_H

#include "stream_storage.h"

typedef struct Raspiraw Raspiraw;

int raspiraw_init(Raspiraw **raspiraw, Storage *storage);

int raspiraw_start(Raspiraw *raspiraw);

int raspiraw_stop(Raspiraw *raspiraw);

void raspiraw_set_train_id(Raspiraw *raspiraw, char *train_id);

#endif //LPX_RASPIRAW_H
