#include "qwm.h"
#include "taskbar.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RIGHT_PAD 10

static int32_t read_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    int val = -1;
    if (fscanf(f, "%d", &val) != 1) val = -1;

    fclose(f);
    return val;
}

static int32_t battery_read_health(void)
{
    int32_t full = read_file("/sys/class/power_supply/BAT0/charge_full");
    int32_t design =
        read_file("/sys/class/power_supply/BAT0/charge_full_design");

    if (full <= 0 || design <= 0) return -1;

    return (full * 100) / design;
}

static int32_t battery_read_capacity(void)
{
    return read_file("/sys/class/power_supply/BAT0/capacity");
}

static int32_t wifi_get_ssid(char *buf, size_t len)
{
    FILE *fp = popen("nmcli -t -f TYPE,STATE,CONNECTION device status", "r");
    if (!fp) return -1;

    char line[256];
    while (fgets(line, sizeof(line), fp))
    {
        char *type = strtok(line, ":");
        char *state = strtok(NULL, ":");
        char *conn = strtok(NULL, "\n");

        if (type && state && conn && strcmp(type, "wifi") == 0 &&
            strcmp(state, "connected") == 0)
        {
            strncpy(buf, conn, len - 1);
            buf[len - 1] = '\0';
            pclose(fp);
            return 0;
        }
    }

    pclose(fp);
    return -1;
}

static uint16_t text_px_width(xcb_connection_t *conn, xcb_font_t font,
                              const char *text)
{
    xcb_query_text_extents_cookie_t cookie = xcb_query_text_extents(
        conn, font, (uint32_t)strlen(text), (xcb_char2b_t *)text);
    xcb_query_text_extents_reply_t *rep =
        xcb_query_text_extents_reply(conn, cookie, NULL);
    if (!rep) return 0;

    uint16_t width = (uint16_t)rep->overall_width;
    free(rep);
    return width;
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

taskbar_t *taskbar_init(struct qwm_t *qwm)
{
    taskbar_t *tb = malloc(sizeof(taskbar_t));
    if (!tb) return NULL;

    tb->last_minute = -1;
    tb->bat_capacity = -1;
    tb->bat_health = -1;

    tb->height = 25;
    tb->width = qwm->w;
    tb->y_pos = qwm->screen->height_in_pixels - tb->height;
    tb->right_x = tb->width - RIGHT_PAD;

    // clang-format off
	uint32_t gray = 0x808080;
    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
    uint32_t values[3] = {gray,
                          1, // override_redirect
                          XCB_EVENT_MASK_EXPOSURE};

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

void taskbar_update(struct qwm_t *qwm, taskbar_t *tb)
{
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    if (tm->tm_min == tb->last_minute) return;
    tb->last_minute = tm->tm_min;

    taskbar_draw(qwm, tb);
}

void taskbar_draw(struct qwm_t *qwm, taskbar_t *tb)
{
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    if (tm->tm_min == tb->last_minute) return;
    tb->last_minute = tm->tm_min;

    xcb_clear_area(qwm->conn, 0, tb->win, 0, 0, tb->width, tb->height);

    tb->right_x = tb->width - RIGHT_PAD;
    taskbar_draw_text(qwm, tb, 10, "2am-qwm");

    char time_str[32];

    // Date
    strftime(time_str, sizeof(time_str), "%d-%m-%Y", tm);
    taskbar_draw_right_text(qwm, tb, time_str, 10);

    // clock
    strftime(time_str, sizeof(time_str), "%H:%M", tm);
    taskbar_draw_right_text(qwm, tb, time_str, 10);

    // battery
    char bat_str[64];
    int32_t cap = battery_read_capacity();
    int32_t health = battery_read_health();
    if (cap >= 0)
    {
        if (health >= 0)
            snprintf(bat_str, sizeof(bat_str), "BAT: %d%% [H:%d%%]", cap,
                     health);
        else
            snprintf(bat_str, sizeof(bat_str), "BAT: %d%%", cap);

        taskbar_draw_right_text(qwm, tb, bat_str, 5);
    }

    char ssid[64];
    if (wifi_get_ssid(ssid, sizeof(ssid)) == 0)
    {
        char wifi_str[96];
        snprintf(wifi_str, sizeof(wifi_str), "WIFI: %s", ssid);
        taskbar_draw_right_text(qwm, tb, wifi_str, 5);
    }

    xcb_flush(qwm->conn);
}

void taskbar_handle_expose(struct qwm_t *qwm, taskbar_t *tb,
                           xcb_expose_event_t *ev)
{
    if (ev->count == 0) taskbar_draw(qwm, tb);
}

void taskbar_draw_text(struct qwm_t *qwm, taskbar_t *tb, uint16_t x,
                       const char *text)
{
    xcb_image_text_8(qwm->conn, (uint8_t)strlen(text), tb->win, tb->gc,
                     (int16_t)x, tb->height / 2 + 4, text);
}

void taskbar_draw_right_text(struct qwm_t *qwm, taskbar_t *tb,
                             const char *text, uint16_t spacing)
{
    uint16_t text_width = text_px_width(qwm->conn, tb->font, text);
    tb->right_x -= text_width;

    xcb_image_text_8(qwm->conn, (uint8_t)strlen(text), tb->win, tb->gc,
                     (int16_t)tb->right_x, tb->height / 2 + 4, text);

    tb->right_x -= spacing;
}
