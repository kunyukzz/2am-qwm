#ifndef QUIET_WM_H
#define QUIET_WM_H

#ifndef _POSIX_C_SOURCE
#    define _POSIX_C_SOURCE 200809L
#endif

#include "taskbar.h"
#include "client.h"

typedef struct qwm_t qwm_t;

#define WORKSPACE_COUNT 5

typedef struct {
    client_t *clients;
    client_t *focused;
} workspace_t;

typedef struct {
    uint16_t mod;
    xcb_keycode_t key;
    void (*func)(qwm_t *);
} keybind_t;

struct qwm_t {
    uint16_t w, h;

    xcb_connection_t *conn;
    xcb_window_t root;
    const xcb_setup_t *setup;
    xcb_screen_t *screen;

    taskbar_t *taskbar;
    client_t *clients;

    const keybind_t *keybinds;
    uint64_t keybind_count;

    workspace_t workspaces[WORKSPACE_COUNT];
    uint16_t current_ws;
};

qwm_t *qwm_init(void);

void qwm_run(qwm_t *qwm);

void qwm_kill(qwm_t *qwm);

#endif // QUIET_WM_H
