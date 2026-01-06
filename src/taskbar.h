#ifndef TASKBAR_H
#define TASKBAR_H

#include <xcb/xcb.h>

struct qwm_t;

typedef struct {
    xcb_window_t win;
    uint16_t width, height, y_pos;
    xcb_font_t font;
    xcb_gcontext_t gc;
} taskbar_t;

taskbar_t *taskbar_init(struct qwm_t *qwm);

void taskbar_kill(struct qwm_t *qwm, taskbar_t *tb);

void taskbar_draw(struct qwm_t *qwm, taskbar_t *tb);

void taskbar_handle_expose(struct qwm_t *qwm, taskbar_t *tb,
                           xcb_expose_event_t *ev);

void taskbar_draw_text(struct qwm_t *qwm, taskbar_t *tb, uint16_t x,
                       const char *text);

#endif // TASKBAR_H

