#include "views.h"
#include "qwm.h"

#define MIN_W 100
#define MIN_H 100

#define BW BORDER_WIDTH
#define BAR wm->taskbar->height

static void layout_monocle(struct qwm_t *wm, uint16_t ws)
{
    workspace_t *w = &wm->workspaces[ws];
    if (!w->clients) return;

    uint32_t x = BW;
    uint32_t y = BW;
    uint32_t width = wm->w - 2 * BW;
    uint32_t height = wm->h - BAR - 2 * BW;

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

    uint32_t screen_w = wm->w > 2 * BW ? wm->w - 2 * BW : wm->w;
    uint32_t screen_h = wm->h > 2 * BW ? wm->h - 2 * BW : wm->h;

    uint32_t win_w = screen_w * 6 / 10;
    uint32_t win_h = screen_h * 6 / 10;

    if (win_w < MIN_W) win_w = MIN_W;
    if (win_h < MIN_H) win_h = MIN_H;
    if (win_w > wm->w) win_w = wm->w;
    if (win_h > wm->h) win_h = wm->h;

    for (client_t *c = w->clients; c; c = c->next)
    {
        uint32_t x = c->x;
        uint32_t y = c->y;

        uint32_t v[] = {x, y, win_w, win_h};
        uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                        XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
        xcb_configure_window(wm->conn, c->win, mask, v);
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

    uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                    XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;

    if (n == 1)
    {
        uint32_t v[] = {BW, BW, full_w - 2 * BW, full_h - 2 * BW};
        xcb_configure_window(wm->conn, c->win, mask, v);
        return;
    }

    uint32_t master_w = full_w / 2;
    if (master_w < MIN_W) master_w = full_w;

    uint32_t master[] = {BW, BW, master_w - 2 * BW, full_h - 2 * BW};
    xcb_configure_window(wm->conn, c->win, mask, master);

    uint32_t stack_n = n - 1;
    uint32_t stack_h = full_h / (uint32_t)stack_n;
    if (stack_h < MIN_H) stack_h = MIN_H;

    uint32_t y = 0;
    for (c = c->next; c; c = c->next)
    {
        uint32_t v[] = {master_w + BW, y + BW, full_w - master_w - 2 * BW,
                        stack_h - 2 * BW};
        xcb_configure_window(wm->conn, c->win, mask, v);
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
