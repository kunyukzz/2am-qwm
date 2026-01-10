#include "views.h"
#include "qwm.h"

#define MIN_W 50
#define MIN_H 50

static void layout_monocle(struct qwm_t *wm, uint16_t ws)
{
    workspace_t *w = &wm->workspaces[ws];
    if (!w->clients) return;

    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t width = wm->w;
    uint32_t height = wm->h - wm->taskbar->height;

    if (width < MIN_W || height < MIN_H) return;

    uint32_t values[] = {x, y, width, height};
    uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                    XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;

    for (client_t *c = w->clients; c; c = c->next)
        xcb_configure_window(wm->conn, c->win, mask, values);
}

static void layout_floating(struct qwm_t *wm, uint16_t ws)
{
    workspace_t *w = &wm->workspaces[ws];
    if (!w->clients) return;

    uint16_t screen_w = wm->w;
    uint16_t screen_h = wm->h - wm->taskbar->height;

    uint16_t win_w = screen_w * (uint16_t)0.6;
    uint16_t win_h = screen_h * (uint16_t)0.6;

    uint16_t x = (screen_w - win_w) / 2;
    uint16_t y = (screen_h - win_h) / 2;

    for (client_t *c = w->clients; c; c = c->next)
    {
        uint32_t v[] = {x, y, win_w, win_h};
        xcb_configure_window(wm->conn, c->win,
                             XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                                 XCB_CONFIG_WINDOW_WIDTH |
                                 XCB_CONFIG_WINDOW_HEIGHT,
                             v);
    }
}

static void layout_tile(struct qwm_t *wm, uint16_t ws)
{
    workspace_t *w = &wm->workspaces[ws];
    if (!w->clients) return;

    uint32_t full_w = wm->w;
    uint32_t full_h = wm->h - wm->taskbar->height;
    if (full_w < MIN_W || full_h < MIN_H) return;

    client_t *c = w->clients;

    // count clients
    uint16_t n = 0;
    for (client_t *t = c; t; t = t->next) n++;

    if (n == 1)
    {
        uint32_t v[] = {0, 0, full_w, full_h};
        xcb_configure_window(wm->conn, c->win,
                             XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                                 XCB_CONFIG_WINDOW_WIDTH |
                                 XCB_CONFIG_WINDOW_HEIGHT,
                             v);
        return;
    }

    uint32_t master_w = full_w / 2;
    if (master_w < MIN_W) master_w = full_w;

    uint32_t master[] = {0, 0, master_w, full_h};
    xcb_configure_window(wm->conn, c->win,
                         XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                             XCB_CONFIG_WINDOW_WIDTH |
                             XCB_CONFIG_WINDOW_HEIGHT,
                         master);

    uint16_t stack_n = n - 1;
    uint32_t stack_h = full_h / (uint32_t)stack_n;
    if (stack_h < MIN_H) stack_h = MIN_H;

    uint32_t y = 0;
    for (c = c->next; c; c = c->next)
    {
        uint32_t v[] = {master_w, y, full_w - master_w, stack_h};

        xcb_configure_window(wm->conn, c->win,
                             XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                                 XCB_CONFIG_WINDOW_WIDTH |
                                 XCB_CONFIG_WINDOW_HEIGHT,
                             v);

        y += stack_h;
    }
}

void layout_apply(struct qwm_t *wm, uint16_t ws)
{
    workspace_t *w = &wm->workspaces[ws];

    switch (w->type)
    {
    case LAYOUT_MONOCLE: layout_monocle(wm, ws); break;
    case LAYOUT_FLOAT: layout_floating(wm, ws); break;
    case LAYOUT_TILE: layout_tile(wm, ws); break;
    }
}
