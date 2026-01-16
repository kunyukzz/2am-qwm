#ifndef TASKBAR_H
#define TASKBAR_H

#include "views.h"
#include "tray_status.h"

struct qwm_t;

typedef struct {
    xcb_window_t win;
    uint16_t width, height, y_pos, right_x;
    xcb_font_t font;
    xcb_gcontext_t gc;
    uint16_t char_width;
} taskbar_t;

void taskbar_init(struct qwm_t *qwm, taskbar_t *tb);

void taskbar_kill(struct qwm_t *qwm, taskbar_t *tb);

int32_t taskbar_update(struct qwm_t *qwm, taskbar_t *tb);

void taskbar_draw(struct qwm_t *qwm, taskbar_t *tb, tray_status_t *ts);

void taskbar_handle_expose(struct qwm_t *qwm, taskbar_t *tb,
                           xcb_expose_event_t *ev);

#endif // TASKBAR_H
