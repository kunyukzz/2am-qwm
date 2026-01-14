#include "client.h"
#include "qwm.h"

#include <stdlib.h>
#include <stdio.h>

client_t *client_init(struct qwm_t *wm, xcb_window_t win)
{
    client_t *c = calloc(1, sizeof(client_t));
    if (!c) return NULL;

    c->win = win;
    c->workspace = wm->current_ws;
    c->next = wm->workspaces[c->workspace].clients;
    wm->workspaces[c->workspace].clients = c;

    uint32_t values[] = {XCB_EVENT_MASK_ENTER_WINDOW |
                         XCB_EVENT_MASK_FOCUS_CHANGE |
                         XCB_EVENT_MASK_PROPERTY_CHANGE};

    xcb_change_window_attributes(wm->conn, win, XCB_CW_EVENT_MASK, values);

    // fprintf(stderr, "client added: 0x%x (ws %d)\n", win, c->workspace);
    return c;
}

void client_kill(struct qwm_t *wm, client_t *c)
{
    (void)wm;
    if (!c) return;
    // fprintf(stderr, "client removed: 0x%x (ws %d)\n", c->win, c->workspace);
    free(c);
}

/*
// NOTE: this for window decoration - but it seems unstable
void client_add_overlay(struct qwm_t *wm, client_t *c)
{
    if (!c) return;

    int titlebar_height = 20;

    // Generate ID for overlay window
    c->frame = xcb_generate_id(wm->conn);

    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t values[2] = {0x444444, XCB_EVENT_MASK_EXPOSURE |
                                        XCB_EVENT_MASK_BUTTON_PRESS};

    // Create overlay window above client
    xcb_create_window(wm->conn,
                      XCB_COPY_FROM_PARENT,  // depth
                      c->frame,              // window id
                      wm->root,              // parent (root)
                      c->x, c->y,            // top-left position of client
                      c->w, titlebar_height, // width = client, height = bar
                      0,                     // border
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT,
                      mask, values);

    // Map overlay window
    xcb_map_window(wm->conn, c->frame);

    xcb_flush(wm->conn);
}
*/

void client_configure(struct qwm_t *wm, client_t *c, uint32_t x, uint32_t y,
                      uint32_t w, uint32_t h)
{
    if (!c) return;

    c->x = x;
    c->y = y;
    c->w = w;
    c->h = h;

    uint32_t values[] = {x, y, w, h};

    uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                    XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;

    xcb_configure_window(wm->conn, c->win, mask, values);
}

void client_set_focus(struct qwm_t *wm, client_t *c, int32_t focused)
{
    uint32_t v[2];

    v[0] = focused ? BORDER_WIDTH : 0;
    xcb_configure_window(wm->conn, c->win, XCB_CONFIG_WINDOW_BORDER_WIDTH, v);

    v[0] = focused ? COLOR_FOCUS : COLOR_UNFOCUS;
    xcb_change_window_attributes(wm->conn, c->win, XCB_CW_BORDER_PIXEL, v);
}
