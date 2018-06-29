#ifndef LPX_LIST_H
#define LPX_LIST_H

#include <stdint.h>
#include <stdbool.h>

typedef struct List List;

typedef struct ListIter ListIter;

List* lst_create();

void lst_append(List *lst, const void *data);

const void *lst_last(List *lst);

size_t lst_size(List *lst);

const void* lst_to_array(List *lst, const void **arr);

ListIter *lst_iterator(List *lst);

bool lst_iter_has_next(ListIter *iter);

bool lst_iter_advance(ListIter *iter);

const void *lst_iter_peak(ListIter *iter);

void lst_iter_free(ListIter *iter);

void lst_free(List *lst);

/**
 * Кроме структур списка освобождает и хранимые данные
 */
void lst_deep_free(List *lst);

#endif //LPX_LIST_H
