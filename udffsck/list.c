#include <stdlib.h>
#include "utils.h"
#include "list.h"

#define dbg(...) dummy(__VA_ARGS__)
void dummy(const char *format, ...) {
    return;
}

uint8_t list_init(list_t *list) {
    uint8_t status = 0;
    list->act = NULL;
    list->first = NULL;
    dbg("list->act %p, address: %p\n", list->act, &list->act);
    return 0;
}

void list_destoy(list_t *list) {
    list_first(list);
    while(list_is_init(list) != 0)
        list_remove_first(list);
}

uint8_t list_insert_first(list_t *list, void * content) {
    dbg("list->act %p, address: %p\n", list->act, &list->act);
    box_t *oldact = list->act;
    dbg("list->act %p, address: %p\n", list->act, &list->act);
    list->act = calloc(1, sizeof(box_t));
    if(list->act == NULL)
        return -1;
    list->act->content = content;
    list->act->next = oldact;
    list->first = list->act;
    dbg("act: %p, act->next: %p, content: %p\n", list->act, list->act->next, list->act->content);
    return 0;
}



uint8_t list_remove_first(list_t *list) {
    if(list->first == NULL)
        return -1;
    box_t *oldfirst = list->first;
    if(list->first->next != NULL) {
        if(list->first == list->act)
            list->act = NULL;
        list->first = list->first->next;
        dbg("oldfirst: %p, newfirst: %p, act: %p\n", oldfirst, list->first, list->act);
    }
    else {
        dbg("last remove\n");
        list->first = NULL;
        list->act = NULL;
    }
    dbg("free: %p\n", oldfirst);
    free(oldfirst->content);
    free(oldfirst);
    return 0;
}

void * list_get(list_t *list) {
    dbg("Act: %p\n", list->act);
    if(list->act == NULL) { 
        dbg("NULL\n");
        return NULL;
    }
    return list->act->content;
}

uint8_t list_next(list_t *list) {
    if(list->act == NULL)
        return -1;
    list->act = list->act->next;
    dbg("Act: %p\n", list->act);
    return 0;
}

uint8_t list_first(list_t *list) {
    if(list_is_init(list) == 0)
        return -1;
    list->act = list->first;
    return 0;
}

uint8_t list_is_init(list_t *list) {
    return list->first != NULL;
}   

