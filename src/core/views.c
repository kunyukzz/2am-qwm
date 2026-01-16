#include "views.h"
#include "qwm.h"

#define MIN_W 100
#define MIN_H 100

#define BW BORDER_WIDTH
#define BAR wm->taskbar.height

static uint16_t count_clients(client_t *c)
{
    uint16_t n = 0;
    for (; c; c = c->next) n++;
    return n;
}

static void set_layout_stack(qwm_t *wm, client_t *c, uint32_t x, uint32_t y,
                             uint32_t total_w, uint32_t total_h,
                             uint16_t stack_n, uint8_t vertical)
{
    if (stack_n == 0) return;

    uint32_t step = vertical ? total_h / stack_n : total_w / stack_n;
    if (vertical && step < MIN_H) step = MIN_H;
    if (!vertical && step < MIN_W) step = MIN_W;

    for (; c; c = c->next)
    {
        if (vertical)
        {
            client_configure(wm, c, x + BW - 1, y + BW - 1, total_w - 3 * BW,
                             step - 3 * BW);
            y += step;
        }
        else
        {
            client_configure(wm, c, x + BW - 1, y + BW - 1, step - 3 * BW,
                             total_h - 3 * BW);
            x += step;
        }
    }
}

static void layout_tile(struct qwm_t *wm, uint16_t ws, uint8_t vertical)
{
    workspace_t *w = &wm->workspaces[ws];
    if (!w->clients) return;

    uint32_t full_w = wm->w;
    uint32_t full_h = wm->h - wm->taskbar.height;
    if (full_w < MIN_W || full_h < MIN_H) return;

    client_t *c = w->clients;
    uint16_t n = count_clients(c);
    if (n == 1)
    {
        client_configure(wm, c, BW - 1, BW - 1, full_w - 3 * BW,
                         full_h - 3 * BW);
        return;
    }

    // master window
    uint32_t master_w = vertical ? full_w / 2 : full_w;
    uint32_t master_h = vertical ? full_h : full_h / 2;
    if (master_w < MIN_W) master_w = full_w;
    if (master_h < MIN_H) master_h = full_h;

    client_configure(wm, c, BW - 1, BW - 1, master_w - 3 * BW,
                     master_h - 3 * BW);

    // stack windows
    c = c->next;
    if (vertical)
    {
        set_layout_stack(wm, c, master_w, 0, full_w - master_w, full_h, n - 1,
                         1);
    }
    else
    {
        set_layout_stack(wm, c, 0, master_h, full_w, full_h - master_h, n - 1,
                         0);
    }
}

static void layout_monocle(struct qwm_t *wm, uint16_t ws)
{
    workspace_t *w = &wm->workspaces[ws];
    if (!w->clients) return;

    uint32_t x = BW - 1;
    uint32_t y = BW - 1;
    uint32_t width = wm->w - 3 * BW;
    uint32_t height = wm->h - BAR - 3 * BW;

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

void layout_apply(struct qwm_t *wm, uint16_t ws)
{
    workspace_t *w = &wm->workspaces[ws];

    switch (w->type)
    {
    case LAYOUT_MONOCLE: layout_monocle(wm, ws); break;
    case LAYOUT_FLOAT: layout_floating(wm, ws); break;
    case LAYOUT_TILE: layout_tile(wm, ws, w->vertical); break;
    }
}
