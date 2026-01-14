#ifndef LAUNCHER_H
#define LAUNCHER_H

#include <xcb/xcb.h>

struct qwm_t;

#define MAX_INPUT 64
#define MAX_MATCH 5

typedef struct {
    char name[64];
    char path[128];
} cmd_entry_t;

typedef struct {
    xcb_gcontext_t sel_text_gc;
    xcb_gcontext_t text_gc;

    xcb_window_t win;
    int16_t x, y, w, h;
    uint8_t opened;
    char scanned_path[4096];

    char input[MAX_INPUT];
    uint32_t input_len;

    cmd_entry_t *cmds;
    uint32_t cmd_count;
    uint32_t cmd_cap;

    uint16_t match_indices[MAX_MATCH];
    uint16_t match_count;
    uint16_t sel;
} launcher_t;

void launcher_init(launcher_t *l);

void launcher_kill(launcher_t *l);

void launcher_open(struct qwm_t *qwm, launcher_t *l);

void launcher_close(struct qwm_t *qwm, launcher_t *l);

void launcher_draw(struct qwm_t *qwm, launcher_t *l);

void launcher_handle_event(struct qwm_t *qwm, launcher_t *l,
                           xcb_generic_event_t *ev);

#endif // LAUNCHER_H
