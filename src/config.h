#ifndef CONFIG_H
#define CONFIG_H

#include "config_api.h"

// Color configuration
#define BORDER_WIDTH 2
#define BORDER_FOCUS 0xFF0000   // RED
#define BORDER_UNFOCUS 0x222222 // GRAY

#define TASKBAR_COLOR 0x444444      // DARK GRAY
#define TASKBAR_FONT_COLOR 0xFFFFFF // WHITE

#define LAUNCHER_WIDTH 400
#define LAUNCHER_POSITION_Y 100
#define LAUNCHER_BG_COLOR 0x444444
#define LAUNCHER_FG_COLOR 0x808080 // LIGHT GRAY
#define LAUNCHER_FONT_COLOR 0xFF0000

// application spawning configuration
static inline void spawn_terminal(struct qwm_t *qwm)
{
    (void)qwm;
    spawn("xfce4-terminal");
}

static inline void spawn_mousepad(struct qwm_t *qwm)
{
    (void)qwm;
    spawn("mousepad");
}

static const keybind_t my_keybinds[] = {
    {KEY_ALT, KEY_Q, quit_wm},
    {KEY_ALT, KEY_W, quit_application},

    {KEY_ALT, KEY_1, workspace_1},
    {KEY_ALT, KEY_2, workspace_2},
    {KEY_ALT, KEY_3, workspace_3},
    {KEY_ALT, KEY_4, workspace_4},
    {KEY_ALT, KEY_5, workspace_5},

    {KEY_ALT | KEY_SHIFT, KEY_1, move_to_workspace_1},
    {KEY_ALT | KEY_SHIFT, KEY_2, move_to_workspace_2},
    {KEY_ALT | KEY_SHIFT, KEY_3, move_to_workspace_3},
    {KEY_ALT | KEY_SHIFT, KEY_4, move_to_workspace_4},
    {KEY_ALT | KEY_SHIFT, KEY_5, move_to_workspace_5},

    {KEY_ALT, KEY_L, toggle_layout},
    {KEY_ALT, KEY_K, focus_next},
    {KEY_ALT, KEY_J, focus_prev},

    {KEY_ALT, KEY_SPACE, spawn_launcher},

    {KEY_ALT, KEY_ENTER, spawn_terminal},
    {KEY_ALT, KEY_M, spawn_mousepad},

};

#endif // CONFIG_H
