#include "taskbar.h"
#include "qwm.h"

#include <stdlib.h>
#include <string.h>

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

    tb->height = 25;
    tb->width = qwm->w;
    tb->y_pos = qwm->screen->height_in_pixels - tb->height;

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

void taskbar_draw(struct qwm_t *qwm, taskbar_t *tb)
{
    xcb_clear_area(qwm->conn, 0, tb->win, 0, 0, tb->width, tb->height);

    taskbar_draw_text(qwm, tb, 10, "2am-qwm");
    // TODO: Draw clock, battery, etc

    xcb_flush(qwm->conn);
}

void taskbar_handle_expose(struct qwm_t *qwm, taskbar_t *tb,
                           xcb_expose_event_t *ev)
{
    /*
    if (ev->window == tb->win)
    {
        taskbar_draw(qwm, tb);
    }
    */

    if (ev->count == 0) taskbar_draw(qwm, tb);
}

void taskbar_draw_text(struct qwm_t *qwm, taskbar_t *tb, uint16_t x,
                       const char *text)
{
    xcb_image_text_8(qwm->conn, (uint8_t)strlen(text), tb->win, tb->gc,
                     (int16_t)x, tb->height / 2 + 4, text);
}

