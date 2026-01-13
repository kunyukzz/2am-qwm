#include "qwm.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h> // waitpid, sigemptyset, sigaction, SA_RESTART, SA_NOCLDSTOP
#include <unistd.h> // fork, setsid, execlp, _exit
#include <poll.h>   // struct pollfd, POLL_IN

// NOTE: for now use what xcb keycode provide
#define KEY_Q 24
#define KEY_W 25
#define KEY_T 28
#define KEY_O 32

#define KEY_H 43
#define KEY_J 44
#define KEY_K 45
#define KEY_L 46

#define KEY_V 55

#define KEY_ENTER 36
#define KEY_ESCAPE 9
#define KEY_SPACE 65

#define KEY_1 10
#define KEY_2 11
#define KEY_3 12
#define KEY_4 13
#define KEY_5 14

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

static void quit_client_focused(qwm_t *wm)
{
    if (!wm) return;

    workspace_t *ws = &wm->workspaces[wm->current_ws];
    client_t *c = ws->focused;
    if (!c) return;

    xcb_void_cookie_t ck = xcb_kill_client_checked(wm->conn, c->win);
    xcb_generic_error_t *err = xcb_request_check(wm->conn, ck);
    if (err) free(err);

    xcb_kill_client(wm->conn, c->win);
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

static void spawn_vlc(qwm_t *qwm)
{
    (void)qwm;
    if (fork() == 0)
    {
        setsid();
        execlp("vlc", "vlc", NULL);
        _exit(1);
    }
}

static void spawn_launcher(qwm_t *qwm)
{
    if (qwm->launcher.opened) return;
    launcher_open(qwm, &qwm->launcher);
    qwm->launcher.opened = 1;
}

static void workspace_switch(qwm_t *wm, uint16_t new_ws)
{
    if (!wm) return;
    if (new_ws < 0 || new_ws >= WORKSPACE_COUNT) return;
    if (new_ws == wm->current_ws) return;

    // hide old workspace
    workspace_t *old = &wm->workspaces[wm->current_ws];
    for (client_t *c = old->clients; c; c = c->next)
        xcb_unmap_window(wm->conn, c->win);

    wm->current_ws = (uint16_t)new_ws;

    // show new workspace
    workspace_t *cur = &wm->workspaces[wm->current_ws];
    for (client_t *c = cur->clients; c; c = c->next)
    {
        xcb_map_window(wm->conn, c->win);
    }

    if (cur->focused)
    {
        xcb_set_input_focus(wm->conn, XCB_INPUT_FOCUS_POINTER_ROOT,
                            cur->focused->win, XCB_CURRENT_TIME);
    }
}

static void toggle_layout(qwm_t *wm)
{
    workspace_t *w = &wm->workspaces[wm->current_ws];
    w->type = (w->type + 1) % 3;

    layout_apply(wm, wm->current_ws);
}

static void focus_next(qwm_t *wm)
{
    workspace_t *ws = &wm->workspaces[wm->current_ws];
    if (!ws->focused) return;

    client_t *next = ws->focused->next;
    if (!next) next = ws->clients;

    ws->focused = next;

    xcb_set_input_focus(wm->conn, XCB_INPUT_FOCUS_POINTER_ROOT, next->win,
                        XCB_CURRENT_TIME);

    uint32_t v[] = {XCB_STACK_MODE_ABOVE};
    xcb_configure_window(wm->conn, ws->focused->win,
                         XCB_CONFIG_WINDOW_STACK_MODE, v);
}

static void focus_prev(qwm_t *wm)
{
    workspace_t *ws = &wm->workspaces[wm->current_ws];
    if (!ws->focused || !ws->clients) return;

    client_t *prev = NULL;
    client_t *c = ws->clients;

    while (c && c != ws->focused)
    {
        prev = c;
        c = c->next;
    }

    if (!prev)
    {
        // wrap: go to last
        for (c = ws->clients; c->next; c = c->next);
        prev = c;
    }

    ws->focused = prev;

    xcb_set_input_focus(wm->conn, XCB_INPUT_FOCUS_POINTER_ROOT, prev->win,
                        XCB_CURRENT_TIME);

    uint32_t v[] = {XCB_STACK_MODE_ABOVE};
    xcb_configure_window(wm->conn, ws->focused->win,
                         XCB_CONFIG_WINDOW_STACK_MODE, v);
}

static void ws_1(qwm_t *qwm) { workspace_switch(qwm, 0); }
static void ws_2(qwm_t *qwm) { workspace_switch(qwm, 1); }
static void ws_3(qwm_t *qwm) { workspace_switch(qwm, 2); }
static void ws_4(qwm_t *qwm) { workspace_switch(qwm, 3); }
static void ws_5(qwm_t *qwm) { workspace_switch(qwm, 4); }

// TODO: make this generic & configureable
static const keybind_t std_keybinds[] = {
    {XCB_MOD_MASK_1, KEY_Q, quit_wm},
    {XCB_MOD_MASK_1, KEY_W, quit_client_focused},

    {XCB_MOD_MASK_1, KEY_T, spawn_kitty},
    {XCB_MOD_MASK_1, KEY_ENTER, spawn_xfce_term},
    {XCB_MOD_MASK_1, KEY_V, spawn_vlc},

    {XCB_MOD_MASK_1, KEY_O, toggle_layout},
    {XCB_MOD_MASK_1, KEY_J, focus_next},
    {XCB_MOD_MASK_1, KEY_K, focus_prev},

    {XCB_MOD_MASK_1, KEY_1, ws_1},
    {XCB_MOD_MASK_1, KEY_2, ws_2},
    {XCB_MOD_MASK_1, KEY_3, ws_3},
    {XCB_MOD_MASK_1, KEY_4, ws_4},
    {XCB_MOD_MASK_1, KEY_5, ws_5},

    {XCB_MOD_MASK_1, KEY_SPACE, spawn_launcher},
    // TODO: add another things
};

static void handle_map_request(qwm_t *wm, xcb_map_request_event_t *ev)
{
    client_t *c = client_init(wm, ev->window);
    wm->workspaces[wm->current_ws].focused = c;

    xcb_map_window(wm->conn, ev->window);

    layout_apply(wm, wm->current_ws);

    xcb_set_input_focus(wm->conn, XCB_INPUT_FOCUS_POINTER_ROOT, ev->window,
                        XCB_CURRENT_TIME);

    xcb_flush(wm->conn);
}

static void handle_enter_notify(qwm_t *wm, xcb_enter_notify_event_t *ev)
{
    for (uint16_t ws = 0; ws < WORKSPACE_COUNT; ws++)
    {
        workspace_t *w = &wm->workspaces[ws];
        for (client_t *c = w->clients; c; c = c->next)
        {
            if (c->win == ev->event)
            {
                if (wm->current_ws != ws) return;
                w->focused = c;
                xcb_set_input_focus(wm->conn, XCB_INPUT_FOCUS_POINTER_ROOT,
                                    c->win, XCB_CURRENT_TIME);

                // raise window
                uint32_t v[] = {XCB_STACK_MODE_ABOVE};
                xcb_configure_window(wm->conn, c->win,
                                     XCB_CONFIG_WINDOW_STACK_MODE, v);

                return;
            }
        }
    }
}

static void handle_destroy_notify(qwm_t *wm, xcb_destroy_notify_event_t *ev)
{
    for (uint16_t ws = 0; ws < WORKSPACE_COUNT; ws++)
    {
        client_t **pc = &wm->workspaces[ws].clients;
        while (*pc)
        {
            client_t *c = *pc;
            if (c->win == ev->window)
            {
                *pc = c->next;

                // update focus if this was focused
                if (wm->workspaces[ws].focused == c)
                    wm->workspaces[ws].focused =
                        wm->workspaces[ws].clients ? wm->workspaces[ws].clients
                                                   : NULL;

                client_kill(wm, c);
                layout_apply(wm, ws);
                return;
            }
            pc = &c->next;
        }
    }
}

static void handle_configure_request(qwm_t *wm,
                                     xcb_configure_request_event_t *ev)
{
    if (ev->window == wm->root) return;
    if (ev->window == wm->taskbar->win) return;

    uint32_t values[7];
    uint32_t i = 0;

    if (ev->value_mask & XCB_CONFIG_WINDOW_X)
    {
        values[i++] = (uint32_t)ev->x;
    }
    if (ev->value_mask & XCB_CONFIG_WINDOW_Y)
    {
        values[i++] = (uint32_t)ev->y;
    }
    if (ev->value_mask & XCB_CONFIG_WINDOW_WIDTH)
    {
        values[i++] = ev->width;
    }
    if (ev->value_mask & XCB_CONFIG_WINDOW_HEIGHT)
    {
        values[i++] = ev->height;
    }
    if (ev->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH)
    {
        values[i++] = ev->border_width;
    }
    if (ev->value_mask & XCB_CONFIG_WINDOW_SIBLING)
    {
        values[i++] = ev->sibling;
    }
    if (ev->value_mask & XCB_CONFIG_WINDOW_STACK_MODE)
    {
        values[i++] = ev->stack_mode;
    }

    xcb_configure_window(wm->conn, ev->window, ev->value_mask, values);
}

static void handle_event(qwm_t *qwm, xcb_generic_event_t *event)
{
    uint8_t type = event->response_type & ~0x80;

    switch (type)
    {
    case XCB_EXPOSE:
    {
        if (qwm->launcher.opened)
            launcher_draw(qwm, &qwm->launcher);
        else
            taskbar_handle_expose(qwm, qwm->taskbar,
                                  (xcb_expose_event_t *)event);
    }
    break;
    case XCB_CLIENT_MESSAGE:
    {
        xcb_client_message_event_t *cev = (xcb_client_message_event_t *)event;
        if (cev->type == qwm->atom.wm_protocols)
        {
            if (cev->data.data32[0] == qwm->atom.wm_delete_window)
            {
                xcb_destroy_window(qwm->conn, cev->window);
            }
        }
    }
    break;
    case XCB_MAP_REQUEST:
        handle_map_request(qwm, (xcb_map_request_event_t *)event);
        break;
    case XCB_CONFIGURE_REQUEST:
        handle_configure_request(qwm, (xcb_configure_request_event_t *)event);
        break;
    case XCB_ENTER_NOTIFY:
        handle_enter_notify(qwm, (xcb_enter_notify_event_t *)event);
        break;
    case XCB_DESTROY_NOTIFY:
        handle_destroy_notify(qwm, (xcb_destroy_notify_event_t *)event);
        break;
    case XCB_KEY_PRESS:
    {
        xcb_key_press_event_t *kev = (xcb_key_press_event_t *)event;

        if (qwm->launcher.opened)
        {
            launcher_handle_event(qwm, &qwm->launcher, event);
            return;
        }

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

static xcb_atom_t intern_atom(qwm_t *qwm, const char *name)
{
    xcb_intern_atom_cookie_t cookie =
        xcb_intern_atom(qwm->conn, 0, (uint16_t)strlen(name), name);
    xcb_intern_atom_reply_t *reply =
        xcb_intern_atom_reply(qwm->conn, cookie, NULL);
    xcb_atom_t atom = reply ? reply->atom : XCB_NONE;
    free(reply);
    return atom;
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
    if (xcb_connection_has_error(qwm->conn))
    {
        free(qwm);
        return NULL;
    }

    const xcb_setup_t *setup = xcb_get_setup(qwm->conn);
    xcb_screen_iterator_t qwm_it = xcb_setup_roots_iterator(setup);
    qwm->screen = qwm_it.data;
    qwm->root = qwm->screen->root;
    qwm->w = qwm->screen->width_in_pixels;
    qwm->h = qwm->screen->height_in_pixels;

    qwm->current_ws = 0;
    for (uint16_t i = 0; i < WORKSPACE_COUNT; ++i)
    {
        qwm->workspaces[i].clients = NULL;
        qwm->workspaces[i].focused = NULL;
        qwm->workspaces[i].type = LAYOUT_MONOCLE;
    }

    // clang-format off
	uint32_t qwm_mask = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_KEY_PRESS |
						XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_ENTER_WINDOW |XCB_EVENT_MASK_FOCUS_CHANGE;

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

    qwm->atom.wm_protocols = intern_atom(qwm, "WM_PROTOCOLS");
    qwm->atom.wm_delete_window = intern_atom(qwm, "WM_DELETE_WINDOW");
    qwm->atom.wm_take_focus = intern_atom(qwm, "WM_TAKE_FOCUS");
    qwm->atom.net_wm_name = intern_atom(qwm, "_NET_WM_NAME");
    qwm->atom.net_supported = intern_atom(qwm, "_NET_SUPPORTED");
    qwm->atom.net_active_window = intern_atom(qwm, "_NET_ACTIVE_WINDOW");

    xcb_atom_t supported[] = {
        qwm->atom.net_supported,
        qwm->atom.net_wm_name,
        qwm->atom.net_active_window,
    };

    xcb_change_property(qwm->conn, XCB_PROP_MODE_REPLACE, qwm->root,
                        qwm->atom.net_supported, XCB_ATOM_ATOM, 32,
                        sizeof(supported) / sizeof(xcb_atom_t), supported);

    // setup keybinding
    qwm->keybinds = std_keybinds;
    qwm->keybind_count = sizeof(std_keybinds) / sizeof(std_keybinds[0]);
    for (uint64_t i = 0; i < qwm->keybind_count; ++i)
    {
        xcb_grab_key(qwm->conn, 1, qwm->root, qwm->keybinds[i].mod,
                     qwm->keybinds[i].key, XCB_GRAB_MODE_ASYNC,
                     XCB_GRAB_MODE_ASYNC);
    }

    // setup taskbar
    qwm->taskbar = taskbar_init(qwm);
    if (!qwm->taskbar)
    {
        xcb_disconnect(qwm->conn);
        free(qwm);
        return NULL;
    }

    tray_init(&qwm->tray);

    launcher_init(&qwm->launcher);

    xcb_flush(qwm->conn);

    return qwm;
}

void qwm_run(qwm_t *qwm)
{
    if (!qwm) return;

    int xfd = xcb_get_file_descriptor(qwm->conn);
    int32_t dirty = 1;

    while (!xcb_connection_has_error(qwm->conn))
    {
        struct pollfd pfd = {.fd = xfd, .events = POLL_IN};
        poll(&pfd, 1, 1000);

        xcb_generic_event_t *ev;
        while ((ev = xcb_poll_for_event(qwm->conn)))
        {
            handle_event(qwm, ev);
            free(ev);
        }

        dirty |= tray_update(qwm, &qwm->tray);
        if (dirty)
        {
            taskbar_draw(qwm, qwm->taskbar, &qwm->tray);
            dirty = 0;
        }
    }

    fprintf(stderr, "X connection closed\n");
}

void qwm_kill(qwm_t *qwm)
{
    if (!qwm) return;

    launcher_kill(&qwm->launcher);

    if (qwm->taskbar) taskbar_kill(qwm, qwm->taskbar);

    if (qwm->conn) xcb_disconnect(qwm->conn);
    free(qwm);
}

