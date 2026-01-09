#ifndef CLIENT_H
#define CLIENT_H

#include <xcb/xcb.h>

struct qwm_t;

typedef struct client_t {
    xcb_window_t win;
    uint32_t x, y, w, h;
    uint16_t workspace;
    struct client_t *next;
} client_t;

client_t *client_init(struct qwm_t *wm, xcb_window_t win);

void client_kill(struct qwm_t *wm, client_t *c);

void client_configure(struct qwm_t *wm, client_t *c, uint32_t x, uint32_t y,
                      uint32_t w, uint32_t h);

#endif // CLIENT_H
