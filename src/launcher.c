#include "qwm.h"
#include "launcher.h"

#include <string.h>
#include <stdio.h>

#define KEY_ENTER 36

void launcher_init(launcher_t *l) { memset(l, 0, sizeof(*l)); }

void launcher_open(struct qwm_t *qwm, launcher_t *l)
{
    l->opened = 0;
    l->w = 400;
    l->h = 24;
    l->x = (qwm->w - l->w) / 2;
    l->y = 50;

    // clang-format off
    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
    uint32_t values[3] = {qwm->screen->white_pixel, 1, XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_EXPOSURE};

    l->win = xcb_generate_id(qwm->conn);
    xcb_create_window(qwm->conn, XCB_COPY_FROM_PARENT, l->win, qwm->root,
					  l->x, l->y,
					  (uint16_t)l->w, (uint16_t)l->h,
					  0,
					  XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      qwm->screen->root_visual, mask, values);

    uint32_t stack_values[] = {
		XCB_NONE,
        XCB_STACK_MODE_ABOVE // stack mode: above sibling
    };
    xcb_configure_window(qwm->conn, l->win, XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE, stack_values);
    // clang-format on

    xcb_map_window(qwm->conn, l->win);
    xcb_set_input_focus(qwm->conn, XCB_INPUT_FOCUS_POINTER_ROOT, l->win,
                        XCB_CURRENT_TIME);

    xcb_flush(qwm->conn);
    l->opened = 1;
    printf("launcher open\n");
}

void launcher_close(struct qwm_t *qwm, launcher_t *l)
{
    if (!l) return;
    if (!l->opened) return;

    xcb_unmap_window(qwm->conn, l->win);
    xcb_destroy_window(qwm->conn, l->win);
    xcb_flush(qwm->conn);

    l->win = 0;
    l->input_len = 0;
    l->match_count = 0;
    printf("launcher closed\n");
}

void launcher_handle_event(struct qwm_t *qwm, launcher_t *l,
                           xcb_generic_event_t *ev)
{
    if (!qwm || !l || !ev) return;

    if ((ev->response_type & 0x7f) != XCB_KEY_PRESS) return;

    xcb_key_press_event_t *kp = (xcb_key_press_event_t *)ev;
    uint8_t code = kp->detail;

    // Only ENTER closes the launcher for now
    if (code == KEY_ENTER)
    {
        launcher_close(qwm, l);
        qwm->launcher.opened = 0;
        return;
    }
}
