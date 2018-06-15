#include <stdint.h>
#include <malloc.h>
#include <string.h>
#include "../include/list.h"
#include "../include/lpxstd.h"


typedef struct ListItem {
    struct ListItem *next;
    void *data;
} ListItem;

typedef struct List {
    ListItem *first;
    ListItem *last;
    uint32_t size;
} List;

List *lst_create() {
    List *lst = xmalloc(sizeof(List));
    memset(lst, 0, sizeof(List));
    return lst;
}

void lst_append(List *lst, void *data) {
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

uint32_t lst_size(List *lst) {
    return lst->size;
}

void* lst_to_array(List *lst, void **arr) {
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