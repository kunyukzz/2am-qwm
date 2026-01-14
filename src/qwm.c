#include "qwm.h"
#include "keys.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h> // waitpid, sigemptyset, sigaction, SA_RESTART, SA_NOCLDSTOP
#include <unistd.h> // fork, setsid, execlp, _exit
#include <poll.h>   // struct pollfd, POLL_IN

#include <xcb/xcb_keysyms.h>

static void handle_child_signal(int sig)
{
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void quit_wm(struct qwm_t *qwm)
{
    qwm_kill(qwm);
    exit(0);
}

void quit_application(struct qwm_t *wm)
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

void spawn_launcher(qwm_t *qwm)
{
    if (qwm->launcher.opened) return;
    launcher_open(qwm, &qwm->launcher);
    qwm->launcher.opened = 1;
}

void spawn(const char *name)
{
    if (fork() == 0)
    {
        setsid();
        execlp(name, name, NULL);
        _exit(1);
    }
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

void toggle_layout(struct qwm_t *wm)
{
    workspace_t *w = &wm->workspaces[wm->current_ws];
    w->type = (w->type + 1) % 3;

    layout_apply(wm, wm->current_ws);
}

void focus_next(struct qwm_t *wm)
{
    workspace_t *ws = &wm->workspaces[wm->current_ws];
    if (!ws->focused) return;

    client_t *next = ws->focused->next;
    if (!next) next = ws->clients;

    client_set_focus(wm, ws->focused, 0);
    ws->focused = next;
    client_set_focus(wm, ws->focused, 1);

    xcb_set_input_focus(wm->conn, XCB_INPUT_FOCUS_POINTER_ROOT, next->win,
                        XCB_CURRENT_TIME);

    uint32_t v[] = {XCB_STACK_MODE_ABOVE};
    xcb_configure_window(wm->conn, ws->focused->win,
                         XCB_CONFIG_WINDOW_STACK_MODE, v);
}

void focus_prev(struct qwm_t *wm)
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

    client_set_focus(wm, ws->focused, 0);
    ws->focused = prev;
    client_set_focus(wm, ws->focused, 1);

    xcb_set_input_focus(wm->conn, XCB_INPUT_FOCUS_POINTER_ROOT, prev->win,
                        XCB_CURRENT_TIME);

    uint32_t v[] = {XCB_STACK_MODE_ABOVE};
    xcb_configure_window(wm->conn, ws->focused->win,
                         XCB_CONFIG_WINDOW_STACK_MODE, v);
}

void swap_master(struct qwm_t *wm)
{
    workspace_t *w = &wm->workspaces[wm->current_ws];
    client_t *f = w->focused;

    if (!w->clients || !f) return;
    if (w->clients == f) return;

    client_t **pc = &w->clients;

    // unlink focused client
    while (*pc && *pc != f) pc = &(*pc)->next;

    if (!*pc) return;
    *pc = f->next;

    // move to front
    f->next = w->clients;
    w->clients = f;

    layout_apply(wm, wm->current_ws);
}

static void move_to_new_ws(qwm_t *wm, client_t *c, uint16_t dst)
{
    if (!wm || !c) return;
    if (dst >= WORKSPACE_COUNT) return;

    uint16_t src = c->workspace;
    if (src == dst) return;

    workspace_t *ws_src = &wm->workspaces[src];
    workspace_t *ws_dst = &wm->workspaces[dst];

    client_t **pc = &ws_src->clients;
    while (*pc)
    {
        if (*pc == c)
        {
            *pc = c->next;
            break;
        }
        pc = &(*pc)->next;
    }

    if (ws_src->focused == c) ws_src->focused = ws_src->clients;

    // insert into destination list (head)
    c->next = ws_dst->clients;
    ws_dst->clients = c;
    ws_dst->focused = c;

    c->workspace = dst;

    if (wm->current_ws != dst) xcb_unmap_window(wm->conn, c->win);

    layout_apply(wm, src);
    layout_apply(wm, dst);

    xcb_flush(wm->conn);
}

static void move_focused_to_ws(qwm_t *wm, uint16_t ws)
{
    workspace_t *w = &wm->workspaces[wm->current_ws];
    if (!w->focused) return;

    move_to_new_ws(wm, w->focused, ws);
}

void workspace_1(struct qwm_t *qwm) { workspace_switch(qwm, 0); }
void workspace_2(struct qwm_t *qwm) { workspace_switch(qwm, 1); }
void workspace_3(struct qwm_t *qwm) { workspace_switch(qwm, 2); }
void workspace_4(struct qwm_t *qwm) { workspace_switch(qwm, 3); }
void workspace_5(struct qwm_t *qwm) { workspace_switch(qwm, 4); }

void move_to_workspace_1(struct qwm_t *qwm) { move_focused_to_ws(qwm, 0); }
void move_to_workspace_2(struct qwm_t *qwm) { move_focused_to_ws(qwm, 1); }
void move_to_workspace_3(struct qwm_t *qwm) { move_focused_to_ws(qwm, 2); }
void move_to_workspace_4(struct qwm_t *qwm) { move_focused_to_ws(qwm, 3); }
void move_to_workspace_5(struct qwm_t *qwm) { move_focused_to_ws(qwm, 4); }

static void handle_map_request(qwm_t *wm, xcb_map_request_event_t *ev)
{
    workspace_t *ws = &wm->workspaces[wm->current_ws];
    client_t *c = client_init(wm, ev->window);

    if (ws->focused) client_set_focus(wm, ws->focused, 0);
    ws->focused = c;
    client_set_focus(wm, c, 1);

    xcb_map_window(wm->conn, ev->window);

    layout_apply(wm, wm->current_ws);

    // client_add_overlay(wm, c);

    xcb_set_input_focus(wm->conn, XCB_INPUT_FOCUS_POINTER_ROOT, ev->window,
                        XCB_CURRENT_TIME);

    xcb_flush(wm->conn);
}

static void handle_enter_notify(qwm_t *wm, xcb_enter_notify_event_t *ev)
{
    workspace_t *w = &wm->workspaces[wm->current_ws];

    for (client_t *c = w->clients; c; c = c->next)
    {
        if (c->win == ev->event)
        {
            if (w->focused == c) return;
            if (w->focused) client_set_focus(wm, w->focused, 0);

            // focus new
            w->focused = c;
            client_set_focus(wm, c, 1);

            xcb_set_input_focus(wm->conn, XCB_INPUT_FOCUS_POINTER_ROOT, c->win,
                                XCB_CURRENT_TIME);

            // raise window
            uint32_t v[] = {XCB_STACK_MODE_ABOVE};
            xcb_configure_window(wm->conn, c->win,
                                 XCB_CONFIG_WINDOW_STACK_MODE, v);

            return;
        }
    }
}

static void handle_destroy_notify(qwm_t *wm, xcb_destroy_notify_event_t *ev)
{
    for (uint16_t ws = 0; ws < WORKSPACE_COUNT; ws++)
    {
        workspace_t *w = &wm->workspaces[ws];
        client_t **pc = &wm->workspaces[ws].clients;
        while (*pc)
        {
            client_t *c = *pc;
            if (c->win == ev->window)
            {
                *pc = c->next;

                int32_t was_focused = (w->focused == c);
                client_kill(wm, c);

                // update focus if this was focused
                if (was_focused)
                {
                    w->focused = w->clients;

                    if (w->focused)
                    {
                        client_set_focus(wm, w->focused, 1);
                        xcb_set_input_focus(wm->conn,
                                            XCB_INPUT_FOCUS_POINTER_ROOT,
                                            w->focused->win, XCB_CURRENT_TIME);
                    }
                }

                layout_apply(wm, ws);
                return;
            }
            pc = &c->next;
        }
    }
}

static int32_t allow_configure(qwm_t *wm, xcb_window_t win)
{
    for (uint16_t ws = 0; ws < WORKSPACE_COUNT; ws++)
    {
        workspace_t *w = &wm->workspaces[ws];
        for (client_t *c = w->clients; c; c = c->next)
        {
            if (c->win == win) return 1;
        }
    }
    return 1;
}

static void handle_configure_request(qwm_t *wm,
                                     xcb_configure_request_event_t *ev)
{
    if (ev->window == wm->root) return;
    if (ev->window == wm->taskbar->win) return;

    if (!allow_configure(wm, ev->window)) return;

    uint32_t values[7];
    uint16_t mask = 0;
    uint32_t i = 0;

    int32_t x = ev->x;
    int32_t y = ev->y;
    int32_t w = ev->width;
    int32_t h = ev->height;

    int32_t max_x = wm->w - w - 2 * BORDER_WIDTH;
    int32_t max_y = wm->h - wm->taskbar->height - h - 2 * BORDER_WIDTH;

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > max_x) x = max_x;
    if (y > max_y) y = max_y;

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
        values[i++] = (uint32_t)w;
    }
    if (ev->value_mask & XCB_CONFIG_WINDOW_HEIGHT)
    {
        mask |= XCB_CONFIG_WINDOW_HEIGHT;
        values[i++] = (uint32_t)h;
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

    if (mask)
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

        uint16_t state = kev->state & ~(XCB_MOD_MASK_LOCK | XCB_MOD_MASK_2);
        for (size_t i = 0; i < qwm->keybind_count; ++i)
        {
            if (kev->detail == qwm->keybinds[i].key &&
                (state == qwm->keybinds[i].mod))
            {
                qwm->keybinds[i].func(qwm);
                break;
            }
        }
        break;
    }

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
						XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION |
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
    qwm->keybinds = my_keybinds;
    qwm->keybind_count = sizeof(my_keybinds) / sizeof(my_keybinds[0]);
    static const uint16_t lock_masks[] = {0, XCB_MOD_MASK_LOCK, XCB_MOD_MASK_2,
                                          XCB_MOD_MASK_LOCK | XCB_MOD_MASK_2};

    for (size_t i = 0; i < sizeof(my_keybinds) / sizeof(my_keybinds[0]); ++i)
    {
        for (size_t j = 0; j < 4; ++j)
        {
            xcb_grab_key(
                qwm->conn, 1, qwm->root, my_keybinds[i].mod | lock_masks[j],
                my_keybinds[i].key, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
        }
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

