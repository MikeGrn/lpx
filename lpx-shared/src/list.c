#include <stdint.h>
#include <malloc.h>
#include <string.h>
#include <stdbool.h>
#include "../include/list.h"
#include "../include/lpxstd.h"


typedef struct ListItem {
    struct ListItem *next;
    const void *data;
} ListItem;

typedef struct List {
    ListItem *first;
    ListItem *last;
    uint32_t size;
} List;

typedef struct ListIter {
    ListItem *first;
    ListItem *item;
} ListIter;

List *lst_create() {
    List *lst = xmalloc(sizeof(List));
    memset(lst, 0, sizeof(List));
    return lst;
}

void lst_append(List *lst, const void *data) {
    ListItem *item = xmalloc(sizeof(struct ListItem));
    memset(item, 0, sizeof(ListItem));
    item->data = data;
    if (lst->last == NULL) {
        lst->first = item;
        lst->last = item;
    } else {
        lst->last->next = item;
        lst->last = item;
    }
    lst->size++;
}

const void *lst_last(List *lst) {
    if (lst->last) {
        return lst->last->data;
    } else {
        return NULL;
    }
}

size_t lst_size(List *lst) {
    return lst->size;
}

ListIter *lst_iterator(List *lst) {
    ListIter *iter = xmalloc(sizeof(ListIter));
    iter->first = xmalloc(sizeof(ListItem));
    iter->item = iter->first;
    iter->item->next = lst->first;
    return iter;
}

bool lst_iter_has_next(ListIter *iter) {
    return iter->item->next != NULL;
}

bool lst_iter_advance(ListIter *iter) {
    if (iter->item == NULL) {
        return false;
    }
    iter->item = iter->item->next;
    return iter->item != NULL;
}

const void *lst_iter_peak(ListIter *iter) {
    if (iter->item == NULL) {
        return NULL;
    }
    return iter->item->data;
}

void lst_iter_free(ListIter *iter) {
    free(iter->first);
    free(iter);
}

const void* lst_to_array(List *lst, const void **arr) {
    ListItem *item = lst->first;
    int idx = 0;
    while (item != NULL) {
        arr[idx++] = item->data;
        item = item->next;
    }

    return arr;
}

void lst_free(List *lst) {
    ListItem *item = lst->first;
    while (item != NULL) {
        ListItem *next = item->next;
        free(item);
        item = next;
    }
    free(lst);
}

void lst_deep_free(List *lst) {
    ListItem *item = lst->first;
    while (item != NULL) {
        ListItem *next = item->next;
        free((void *) item->data);
        free(item);
        item = next;
    }
    free(lst);
}
