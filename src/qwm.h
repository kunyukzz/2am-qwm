#ifndef QUIET_WM_H
#define QUIET_WM_H

#ifndef _POSIX_C_SOURCE
#    define _POSIX_C_SOURCE 200809L
#endif

#include <xcb/xcb.h>

typedef struct {
    uint16_t mod;
    xcb_keycode_t key;
    void (*func)(void);
} keybind_t;

typedef struct {
    xcb_window_t win;
    uint16_t height;
} taskbar_t;

typedef struct {
    uint16_t w, h;

    xcb_connection_t *conn;
    xcb_window_t root;
    const xcb_setup_t *setup;
    xcb_screen_t *screen;

    taskbar_t taskbar;

    const keybind_t *keybinds;
    uint64_t keybind_count;
} qwm_t;

qwm_t *qwm_init(void);

void qwm_kill(qwm_t *qwm);

#endif // QUIET_WM_H
