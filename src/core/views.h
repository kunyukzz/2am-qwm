#ifndef VIEWS_H
#define VIEWS_H

#include "client.h"

struct qwm_t;

#define WORKSPACE_COUNT 5

typedef enum {
    LAYOUT_MONOCLE,
    LAYOUT_FLOAT,
    LAYOUT_TILE,
} layout_type_t;

typedef struct {
    client_t *clients;
    client_t *focused;
    layout_type_t type;
    uint8_t vertical;
} workspace_t;

void layout_apply(struct qwm_t *wm, uint16_t ws);

#endif // VIEWS_H
