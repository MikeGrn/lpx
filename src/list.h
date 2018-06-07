#ifndef LPX_LIST_H
#define LPX_LIST_H

#include <stdint.h>

typedef struct List List;

List* lst_create();

void lst_append(List *lst, void *data);

uint32_t lst_size(List *lst);

void* lst_to_array(List *lst, void **arr);

void lst_free(List *lst);

#endif //LPX_LIST_H
