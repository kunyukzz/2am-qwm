#include "qwm.h"
#include "taskbar.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RIGHT_PAD 10

#define CPU_GOV_PATH "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
#define BAT_CAPACITY_PATH "sys/class/power_supply/BAT0/capacity"

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

static int64_t read_uptime(void)
{
    FILE *f = fopen("/proc/uptime", "r");
    if (!f) return -1;

    double up = 0.0;
    if (fscanf(f, "%lf", &up) != 1) up = -1;

    fclose(f);
    return (int64_t)up;
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

    // use simpler way - Network Manager
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

static void format_uptime(int64_t min, char *buf, size_t sz)
{
    int d = (int)min / (60 * 24);
    int h = (min / 60) % 24;
    int m = min % 60;

    if (d > 0)
        snprintf(buf, sz, "| UP: %dd %02dh %02dm", d, h, m);
    else
        snprintf(buf, sz, "| UP: %02dh %02dm", h, m);
}

/*****************************
 * TASKBAR
 *****************************/

taskbar_t *taskbar_init(struct qwm_t *qwm)
{
    taskbar_t *tb = calloc(1, sizeof(taskbar_t));
    if (!tb) return NULL;

    tb->last_minute = -1;
    tb->last_ws = -1;
    tb->bat_capacity = -1;
    tb->bat_state = BAT_UNKNOWN;
    tb->uptime = -1;
    tb->last_uptime = -1;
    tb->date[0] = '\0';
    tb->time[0] = '\0';
    tb->ssid[0] = '\0';
    tb->governor[0] = '\0';

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

int32_t taskbar_update(struct qwm_t *qwm, taskbar_t *tb)
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

    if (tb->last_ws != qwm->current_ws)
    {
        tb->last_ws = qwm->current_ws;
        dirty = 1;
    }

    layout_type_t cur = qwm->workspaces[qwm->current_ws].type;
    if (tb->last_layout != cur)
    {
        tb->last_layout = cur;
        dirty = 1;
    }

    // NOTE: this works with governor auto-update in mind
    // if not, can be move to initialized taskbar
    char gov[16];
    if (read_file_string(CPU_GOV_PATH, gov, sizeof(gov)) > 0)
    {
        if (strcmp(gov, tb->governor) != 0)
        {
            strncpy(tb->governor, gov, sizeof(tb->governor));
            dirty = 1;
        }
    }

    int64_t up = read_uptime();
    if (up >= 0)
    {
        int64_t up_min = up / 60;

        if (up_min != tb->last_uptime)
        {
            tb->last_uptime = up_min;
            tb->uptime = up_min;
            dirty = 1;
        }
    }

    return dirty;
}

void taskbar_draw(struct qwm_t *qwm, taskbar_t *tb)
{
    xcb_clear_area(qwm->conn, 0, tb->win, 0, 0, tb->width, tb->height);

    tb->right_x = tb->width - RIGHT_PAD;
    taskbar_draw_text(qwm, tb, 10, "2am-qwm");

    char ws[16];
    snprintf(ws, sizeof(ws), "| WS: %d", qwm->current_ws + 1);
    taskbar_draw_text(qwm, tb, 60, ws);

    const char *layout = layout_name(qwm->workspaces[qwm->current_ws].type);
    taskbar_draw_text(qwm, tb, 110, layout);

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

    if (tb->governor[0])
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "| GOV: %s", tb->governor);
        taskbar_draw_right_text(qwm, tb, buf, 10);
    }

    char up[32];
    format_uptime(tb->uptime, up, sizeof(up));
    taskbar_draw_right_text(qwm, tb, up, 10);

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

    taskbar_draw_text(qwm, tb, tb->right_x, text);

    tb->right_x -= spacing;
}
