#ifndef __LIST_H__
#define __LIST_H__

#include <inttypes.h>

typedef struct tBox {
    struct tBox *next;
    void * content;
} box_t;

typedef struct {
    box_t *first;
    box_t *act;    
} list_t;


uint8_t list_init(list_t *list);
void list_destoy(list_t *list);

uint8_t list_insert_first(list_t *list, void * content);
uint8_t list_remove_first(list_t *list);

void * list_get(list_t *list);

uint8_t list_next(list_t *list);
uint8_t list_first(list_t *list);

uint8_t list_is_init(list_t *list);

#endif //__LIST_H__
