#include "qwm.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h> // waitpid, sigemptyset, sigaction, SA_RESTART, SA_NOCLDSTOP
#include <unistd.h> // fork, setsid, execlp, _exit

// NOTE: for now use what xcb keycode provide
#define KEY_Q 24
#define KEY_T 28
#define KEY_ENTER 36

static void handle_child_signal(int sig)
{
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

static void quit_wm(qwm_t *qwm)
{
    qwm_kill(qwm);
    exit(0);
}

static void spawn_kitty(qwm_t *qwm)
{
    (void)qwm;
    if (fork() == 0)
    {
        setsid();
        execlp("kitty", "kitty", NULL);
        _exit(1);
    }
}

static void spawn_xfce_term(qwm_t *qwm)
{
    (void)qwm;
    if (fork() == 0)
    {
        setsid();
        execlp("xfce4-terminal", "xfce4-terminal", NULL);
        _exit(1);
    }
}

static const keybind_t std_keybinds[] = {
    {XCB_MOD_MASK_1, KEY_Q, quit_wm},
    {XCB_MOD_MASK_1, KEY_T, spawn_kitty},
    {XCB_MOD_MASK_1, KEY_ENTER, spawn_xfce_term},
    // TODO: add another things
};

static void handle_map_request(qwm_t *wm, xcb_map_request_event_t *ev)
{
    /*
    uint32_t values[] = {
        0,                           // x
        0,                           // y
        wm->w,                       // width
        wm->h - wm->taskbar->height, // height
    };
    uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                    XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;

    xcb_configure_window(wm->conn, ev->window, mask, values);
    */

    xcb_map_window(wm->conn, ev->window);
    xcb_set_input_focus(wm->conn, XCB_INPUT_FOCUS_POINTER_ROOT, ev->window,
                        XCB_CURRENT_TIME);
}

static void handle_configure_notify(qwm_t *wm,
                                    xcb_configure_notify_event_t *ev)
{
    if (ev->window == wm->root) return;
    if (ev->window == wm->taskbar->win) return;

    uint32_t values[] = {0, 0, wm->w, wm->h - wm->taskbar->height};

    // clang-format off
    xcb_configure_window(wm->conn, ev->window,
						 XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
						 XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                         values);

    uint32_t stack_values[] = {wm->taskbar->win, XCB_STACK_MODE_BELOW};

    xcb_configure_window(wm->conn, ev->window,
                         XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE,
                         stack_values);
    // clang-format on
}

static void handle_configure_request(qwm_t *wm,
                                     xcb_configure_request_event_t *ev)
{
    if (ev->window == wm->root) return;
    if (ev->window == wm->taskbar->win) return;

    uint16_t mask = 0;
    uint32_t values[4];
    int i = 0;

    int16_t x = 0;
    int16_t y = 0;
    uint16_t w = wm->w;
    uint16_t h = wm->h - wm->taskbar->height;

    if (ev->value_mask & XCB_CONFIG_WINDOW_X)
    {
        mask |= XCB_CONFIG_WINDOW_X;
        values[i++] = (uint32_t)x;
    }
    if (ev->value_mask & XCB_CONFIG_WINDOW_Y)
    {
        mask |= XCB_CONFIG_WINDOW_Y;
        values[i++] = (uint32_t)y;
    }
    if (ev->value_mask & XCB_CONFIG_WINDOW_WIDTH)
    {
        mask |= XCB_CONFIG_WINDOW_WIDTH;
        values[i++] = w;
    }
    if (ev->value_mask & XCB_CONFIG_WINDOW_HEIGHT)
    {
        mask |= XCB_CONFIG_WINDOW_HEIGHT;
        values[i++] = h;
    }
    /*
    if (ev->value_mask & XCB_CONFIG_WINDOW_SIBLING)
    {
        mask |= XCB_CONFIG_WINDOW_SIBLING;
        values[i++] = ev->sibling;
    }
    if (ev->value_mask & XCB_CONFIG_WINDOW_STACK_MODE)
    {
        mask |= XCB_CONFIG_WINDOW_STACK_MODE;
        values[i++] = ev->stack_mode;
    }
    */

    if (mask)
    {
        xcb_configure_window(wm->conn, ev->window, mask, values);
    }
}

static void handle_event(qwm_t *qwm, xcb_generic_event_t *event)
{
    uint8_t type = event->response_type & ~0x80;

    switch (type)
    {
    case XCB_EXPOSE:
        taskbar_handle_expose(qwm, qwm->taskbar, (xcb_expose_event_t *)event);
        break;
    case XCB_CONFIGURE_NOTIFY:
        // handle_configure_notify(qwm, (xcb_configure_notify_event_t *)event);
        break;
    case XCB_MAP_REQUEST:
        handle_map_request(qwm, (xcb_map_request_event_t *)event);
        break;
    case XCB_CONFIGURE_REQUEST:
        handle_configure_request(qwm, (xcb_configure_request_event_t *)event);
        break;
    case XCB_KEY_PRESS:
    {
        xcb_key_press_event_t *kev = (xcb_key_press_event_t *)event;
        for (size_t i = 0; i < qwm->keybind_count; ++i)
        {
            if (kev->detail == qwm->keybinds[i].key &&
                (kev->state & qwm->keybinds[i].mod) == qwm->keybinds[i].mod)
            {
                qwm->keybinds[i].func(qwm);
                break;
            }
        }
    }
    break;

    default: break;
    }

    xcb_flush(qwm->conn);
}

/*****************************
 * WINDOW MANAGER
 *****************************/

qwm_t *qwm_init(void)
{
    // signal child handling
    struct sigaction sa = {0};
    sa.sa_handler = handle_child_signal;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGCHLD, &sa, NULL);

    // become window manager
    qwm_t *qwm = calloc(1, sizeof(*qwm));
    if (!qwm) return NULL;

    qwm->conn = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(qwm->conn)) return NULL;

    const xcb_setup_t *setup = xcb_get_setup(qwm->conn);
    xcb_screen_iterator_t qwm_it = xcb_setup_roots_iterator(setup);
    qwm->screen = qwm_it.data;
    qwm->root = qwm->screen->root;
    qwm->w = qwm->screen->width_in_pixels;
    qwm->h = qwm->screen->height_in_pixels;

    // clang-format off
    uint32_t qwm_mask = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_KEY_PRESS;
    xcb_void_cookie_t ck = xcb_change_window_attributes_checked( qwm->conn, qwm->root, XCB_CW_EVENT_MASK, &qwm_mask);
    xcb_generic_error_t *qwm_err = xcb_request_check(qwm->conn, ck);
    // clang-format on

    if (qwm_err)
    {
        free(qwm_err);
        xcb_disconnect(qwm->conn);
        free(qwm);
        return NULL;
    }

    // setup keybinding
    qwm->keybinds = std_keybinds;
    qwm->keybind_count = sizeof(std_keybinds) / sizeof(std_keybinds[0]);

    // setup taskbar
    qwm->taskbar = taskbar_init(qwm);
    if (!qwm->taskbar)
    {
        taskbar_kill(qwm, qwm->taskbar);
    }

    return qwm;
}

void qwm_run(qwm_t *qwm)
{
    if (!qwm) return;

    xcb_generic_event_t *ev;
    while ((ev = xcb_wait_for_event(qwm->conn)))
    {
        handle_event(qwm, ev);
        free(ev);
    }

    fprintf(stderr, "X connection closed\n");
}

void qwm_kill(qwm_t *qwm)
{
    if (!qwm) return;

    if (qwm->taskbar) taskbar_kill(qwm, qwm->taskbar);

    if (qwm->conn) xcb_disconnect(qwm->conn);
    free(qwm);
}

int main(void)
{
    qwm_t *qwm = qwm_init();
    if (!qwm) return 1;

    qwm_run(qwm);
    qwm_kill(qwm);

    return 0;
}
