#include "qwm.h"
#include "taskbar.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RIGHT_PAD 8

static void taskbar_draw_text(struct qwm_t *qwm, taskbar_t *tb, uint16_t x,
                              const char *text)
{
    xcb_image_text_8(qwm->conn, (uint8_t)strlen(text), tb->win, tb->gc,
                     (int16_t)x, 16, text);
}

static uint16_t text_px_width(xcb_connection_t *conn, xcb_font_t font,
                              const char *text)
{
    // Core X fonts are always 2-byte per character, even for ASCII.
    // The high byte MUST be initialized (usually zero), otherwise xcb
    // will send uninitialized data to the X server.
    uint64_t len = strlen(text);
    xcb_char2b_t *chars = calloc(len, sizeof(xcb_char2b_t));
    if (!chars) return 0;

    for (uint64_t i = 0; i < len; ++i)
    {
        chars[i].byte1 = 0;
        chars[i].byte2 = (uint8_t)text[i];
    }

    xcb_query_text_extents_cookie_t cookie =
        xcb_query_text_extents(conn, font, (uint32_t)len, chars);
    xcb_query_text_extents_reply_t *rep =
        xcb_query_text_extents_reply(conn, cookie, NULL);

    free(chars);
    if (!rep) return 0;

    uint16_t width = (uint16_t)rep->overall_width;
    free(rep);
    return width;
}

static void taskbar_draw_right_text(struct qwm_t *qwm, taskbar_t *tb,
                                    const char *text, uint16_t spacing)
{
    uint16_t text_width = text_px_width(qwm->conn, tb->font, text);
    tb->right_x -= text_width;

    taskbar_draw_text(qwm, tb, tb->right_x, text);

    tb->right_x -= spacing;
}

static xcb_atom_t get_atom(xcb_connection_t *conn, const char *name)
{
    xcb_intern_atom_cookie_t cookie =
        xcb_intern_atom(conn, 0, (uint16_t)strlen(name), name);

    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn, cookie, NULL);

    if (!reply) return XCB_ATOM_NONE;

    xcb_atom_t atom = reply->atom;
    free(reply);
    return atom;
}

static char *battery_status_string(battery_state_t bat_state)
{
    switch (bat_state)
    {
    case BAT_CHARGING: return "Charging";
    case BAT_DISCHARGING: return "Discharging";
    case BAT_FULL: return "Full";
    default: return "Not Available";
    }
}

static const char *layout_name(layout_type_t t)
{
    switch (t)
    {
    case LAYOUT_MONOCLE: return "[Monocle]";
    case LAYOUT_TILE: return "[Tiled]";
    case LAYOUT_FLOAT: return "[Float]";
    default: return "[?]";
    }
}

static void format_uptime(uint64_t min, char *buf, size_t sz)
{
    int d = (int)min / (60 * 24);
    int h = (min / 60) % 24;
    int m = min % 60;

    if (d > 0)
        snprintf(buf, sz, "| %dd %02dh %02dm", d, h, m);
    else
        snprintf(buf, sz, "| %02dh %02dm", h, m);
}

static const char *connection_type_str(connect_type_t type)
{
    switch (type)
    {
    case WIFI: return "Wi-Fi";
    case LAN: return "LAN";
    case UNKNOWN: return "Unknown";
    default: return "???";
    }
}

static const char *connection_state_str(connect_state_t state)
{
    switch (state)
    {
    case CONNECT: return "Connected";
    case DISCONNECT: return "Disconnected";
    default: return "???";
    }
}

/*****************************
 * TASKBAR
 *****************************/

taskbar_t *taskbar_init(struct qwm_t *qwm)
{
    taskbar_t *tb = calloc(1, sizeof(taskbar_t));
    if (!tb) return NULL;

    tb->height = 25;
    tb->width = qwm->w;
    tb->y_pos = qwm->screen->height_in_pixels - tb->height;
    tb->right_x = tb->width - RIGHT_PAD;

    // clang-format off
	uint32_t gray = 0x444444;
    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
    uint32_t values[3] = {gray, 1, XCB_EVENT_MASK_EXPOSURE};

	tb->win = xcb_generate_id(qwm->conn);
    xcb_create_window(qwm->conn, XCB_COPY_FROM_PARENT, tb->win, qwm->root,
        0, (int16_t)tb->y_pos, // position x,y
        tb->width, tb->height, // size
        0, // border
        XCB_WINDOW_CLASS_INPUT_OUTPUT, qwm->screen->root_visual, mask, values);

    uint32_t stack_values[] = {
        XCB_NONE,
        XCB_STACK_MODE_ABOVE // stack mode: above sibling
    };
    xcb_configure_window(qwm->conn, tb->win,
                         XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE,
                         stack_values);

	// set to dock
    xcb_atom_t type_atom = get_atom(qwm->conn, "_NET_WM_WINDOW_TYPE");
	xcb_atom_t dock_atom = get_atom(qwm->conn, "_NET_WM_WINDOW_TYPE_DOCK");
    xcb_change_property(qwm->conn, XCB_PROP_MODE_REPLACE, tb->win,
                        dock_atom, XCB_ATOM_ATOM, 32, 1,
                        &type_atom);
    // clang-format on

    xcb_map_window(qwm->conn, tb->win);

    // setup font
    tb->font = xcb_generate_id(qwm->conn);
    xcb_open_font(qwm->conn, tb->font, 7, "fixed");

    // setup graphics context
    tb->gc = xcb_generate_id(qwm->conn);
    uint32_t gc_values[] = {
        qwm->screen->white_pixel, // foreground color
        gray,                     // background color
        tb->font                  // font
    };
    xcb_create_gc(qwm->conn, tb->gc, tb->win,
                  XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT,
                  gc_values);

    xcb_flush(qwm->conn);

    return tb;
}

void taskbar_kill(struct qwm_t *qwm, taskbar_t *tb)
{
    if (!tb) return;
    if (tb->gc) xcb_free_gc(qwm->conn, tb->gc);
    if (tb->font) xcb_close_font(qwm->conn, tb->font);
    if (tb->win) xcb_destroy_window(qwm->conn, tb->win);
    free(tb);
}

void taskbar_draw(struct qwm_t *qwm, taskbar_t *tb, tray_status_t *ts)
{
    xcb_clear_area(qwm->conn, 0, tb->win, 0, 0, tb->width, tb->height);

    // left side
    taskbar_draw_text(qwm, tb, 8, "2am-qwm");

    char ws[8];
    snprintf(ws, sizeof(ws), "| WS: %d", qwm->current_ws + 1);
    taskbar_draw_text(qwm, tb, 60, ws);

    const char *layout = layout_name(qwm->workspaces[qwm->current_ws].type);
    taskbar_draw_text(qwm, tb, 110, layout);

    // right side
    tb->right_x = tb->width - RIGHT_PAD;
    uint16_t spacing = 8;

    taskbar_draw_right_text(qwm, tb, ts->time_date.date, spacing);
    taskbar_draw_right_text(qwm, tb, ts->time_date.time, spacing);

    char gov_buf[16];
    snprintf(gov_buf, sizeof(gov_buf), "| %s", ts->gov.name);
    taskbar_draw_right_text(qwm, tb, gov_buf, spacing);

    char freq_buf[16];
    snprintf(freq_buf, sizeof(freq_buf), "| %d.%02dGHz", ts->cpu.mhz / 1000,
             (ts->cpu.mhz % 1000) / 10);
    taskbar_draw_right_text(qwm, tb, freq_buf, spacing);

    char mem[32];
    snprintf(mem, sizeof(mem), "| %d/%dMB", ts->mems.current, ts->mems.total);
    taskbar_draw_right_text(qwm, tb, mem, spacing);

    if (ts->bat.capacity > 0)
    {
        char bat[32];
        const char *status_str = battery_status_string(ts->bat.state);
        snprintf(bat, sizeof(bat), "| %d%% (%s)", ts->bat.capacity,
                 status_str);
        taskbar_draw_right_text(qwm, tb, bat, spacing);
    }

    char up[16];
    format_uptime(ts->up.current, up, sizeof(up));
    taskbar_draw_right_text(qwm, tb, up, spacing);

    char wifi[128];
    snprintf(wifi, sizeof(wifi), "%s: %s",
             connection_type_str(ts->connection.cn_type), ts->connection.name);
    taskbar_draw_right_text(qwm, tb, wifi, spacing);

    char con_state[16];
    snprintf(con_state, sizeof(con_state), "%s",
             connection_state_str(ts->connection.cn_state));
    taskbar_draw_right_text(qwm, tb, con_state, spacing);

    xcb_flush(qwm->conn);
}

void taskbar_handle_expose(struct qwm_t *qwm, taskbar_t *tb,
                           xcb_expose_event_t *ev)
{
    if (ev->count == 0) taskbar_draw(qwm, tb, &qwm->tray);
}
