#ifndef QUIET_WM_H
#define QUIET_WM_H

#ifndef _POSIX_C_SOURCE
#    define _POSIX_C_SOURCE 200809L
#endif

#include "taskbar.h"
#include "client.h"
#include "views.h"
#include "tray_status.h"

typedef struct qwm_t qwm_t;

typedef struct {
    uint16_t mod;
    xcb_keycode_t key;
    void (*func)(qwm_t *);
} keybind_t;

// NOTE: some of these may be not using it
typedef struct {
    xcb_atom_t wm_protocols;
    xcb_atom_t wm_delete_window;
    xcb_atom_t wm_take_focus;
    xcb_atom_t net_wm_name;
    xcb_atom_t net_supported;
    xcb_atom_t net_active_window;
} atom_t;

struct qwm_t {
    uint16_t w, h;

    xcb_connection_t *conn;
    xcb_window_t root;
    const xcb_setup_t *setup;
    xcb_screen_t *screen;

    atom_t atom;

    taskbar_t *taskbar;
    tray_status_t tray;

    const keybind_t *keybinds;
    uint64_t keybind_count;

    workspace_t workspaces[WORKSPACE_COUNT];
    uint16_t current_ws;
};

qwm_t *qwm_init(void);

void qwm_run(qwm_t *qwm);

void qwm_kill(qwm_t *qwm);

#endif // QUIET_WM_H
