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

static int32_t read_file_string(const char *path, char *buf, uint64_t bufsize)
{
    if (!buf || bufsize == 0) return -1;

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    if (!fgets(buf, (int32_t)bufsize, f))
    {
        fclose(f);
        return -1;
    }
    fclose(f);

    buf[strcspn(buf, "\n")] = 0;

    return (int32_t)strlen(buf);
}

static battery_state_t battery_read_status(void)
{
    // most battery on laptop was BAT0 / BAT1
    char buf[16];
    int len = read_file_string("/sys/class/power_supply/BAT0/status", buf,
                               sizeof(buf));
    if (len <= 0) return BAT_UNKNOWN;

    if (buf[len - 1] == '\n') buf[len - 1] = '\0';

    if (strcmp(buf, "Charging") == 0) return BAT_CHARGING;
    if (strcmp(buf, "Discharging") == 0) return BAT_DISCHARGING;
    if (strcmp(buf, "Full") == 0) return BAT_FULL;

    return BAT_UNKNOWN;
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

static int32_t battery_read_capacity(void)
{
    return read_file("/sys/class/power_supply/BAT0/capacity");
}

static int32_t wifi_get_ssid(char *ssid, uint64_t len)
{
    if (!ssid || len == 0) return -1;

    FILE *fp = popen("nmcli -t -f NAME,DEVICE connection show --active", "r");
    if (!fp) return -1;

    char line[128];
    int32_t ret = -1;

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        char *colon = strchr(line, ':');
        if (!colon) continue;
        if (strstr(colon + 1, "wlp") == NULL) continue;
        *colon = '\0';

        uint64_t slen = strlen(line);
        if (slen > 0 && line[slen - 1] == '\n') line[slen - 1] = '\0';

        strncpy(ssid, line, len - 1);
        ssid[len - 1] = '\0';

        ret = 0;
        break;
    }

    pclose(fp);
    if (ret != 0) ssid[0] = '\0';
    return ret;
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

int32_t taskbar_update(taskbar_t *tb)
{
    int32_t dirty = 0;

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    if (tm->tm_min != tb->last_minute)
    {
        tb->last_minute = tm->tm_min;
        strftime(tb->date, sizeof(tb->date), "| %d-%m-%Y", tm);
        strftime(tb->time, sizeof(tb->time), "| %H:%M", tm);
        dirty = 1;
    }

    int32_t cap = battery_read_capacity();
    battery_state_t bat_st = battery_read_status();
    if (cap != tb->bat_capacity || bat_st != tb->bat_state)
    {
        tb->bat_capacity = cap;
        tb->bat_state = bat_st;
        dirty = 1;
    }

    if (wifi_get_ssid(tb->ssid, sizeof(tb->ssid)) != 0) tb->ssid[0] = '\0';

    return dirty;
}

void taskbar_draw(struct qwm_t *qwm, taskbar_t *tb)
{
    xcb_clear_area(qwm->conn, 0, tb->win, 0, 0, tb->width, tb->height);

    tb->right_x = tb->width - RIGHT_PAD;
    taskbar_draw_text(qwm, tb, 10, "2am-qwm");

    taskbar_draw_right_text(qwm, tb, tb->date, 10);
    taskbar_draw_right_text(qwm, tb, tb->time, 10);

    if (tb->bat_capacity >= 0)
    {
        char bat[64];
        const char *status_str = battery_status_string(tb->bat_state);
        snprintf(bat, sizeof(bat), "| BAT: %d%% (%s)", tb->bat_capacity,
                 status_str);
        taskbar_draw_right_text(qwm, tb, bat, 10);
    }

    if (tb->ssid[0])
    {
        char wifi[128];
        snprintf(wifi, sizeof(wifi), "WIFI: %s", tb->ssid);
        taskbar_draw_right_text(qwm, tb, wifi, 10);
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
