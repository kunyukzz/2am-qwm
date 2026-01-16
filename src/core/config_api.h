/*
 * Config API
 * Everything in this header is safe to use from config.h
 * Do not include WM internals here.
 */

#ifndef CONFIG_API_H
#define CONFIG_API_H

#include "keys.h" // IWYU pragma: keep

struct qwm_t;

typedef struct {
    uint16_t mod;
    xcb_keycode_t key;
    void (*func)(struct qwm_t *);
} keybind_t;

extern void spawn(const char *program, ...);

void quit_wm(struct qwm_t *qwm);
void quit_application(struct qwm_t *qwm);

void workspace_1(struct qwm_t *qwm);
void workspace_2(struct qwm_t *qwm);
void workspace_3(struct qwm_t *qwm);
void workspace_4(struct qwm_t *qwm);
void workspace_5(struct qwm_t *qwm);

void move_to_workspace_1(struct qwm_t *qwm);
void move_to_workspace_2(struct qwm_t *qwm);
void move_to_workspace_3(struct qwm_t *qwm);
void move_to_workspace_4(struct qwm_t *qwm);
void move_to_workspace_5(struct qwm_t *qwm);

void toggle_tile_orient(struct qwm_t *qwm);
void toggle_layout(struct qwm_t *qwm);
void focus_next(struct qwm_t *qwm);
void focus_prev(struct qwm_t *qwm);
void swap_master(struct qwm_t *wm);

void spawn_launcher(struct qwm_t *qwm);

#endif // CONFIG_API_H
