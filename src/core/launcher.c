#include "qwm.h"
#include "launcher.h"
#include "keys.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>   // access, X_OK
#include <dirent.h>   // DIR, dirent, opendir, closedir
#include <sys/stat.h> // stat, S_ISREG

#define LINE_HEIGHT 16
#define PADDING 4
#define MAX_DRAW 8

static char keycode_to_char(uint8_t code);

// clang-format off
static const char *banned_cmds[] = {
    "rm", "rmdir", "mkfs", "dd", "sudo", "doas",
    "chmod", "chown", "chgrp", "truncate",

    "shutdown", "reboot", "halt", "poweroff",

    // package management (qwm not run with root previlage)
	// but i added if user cheating. hahaha
    "apt-get", "apt", "dnf", "yum", "pacman", "zypper",
    "emerge", "xbps",

    // advanced system / network
    "mount", "umount", "kill", "killall", "pkill",
    "iptables", "ip6tables", "systemctl", "service", "rfkill",

    // other destructive / low-level
    "wipe", "shred", "fsck",
    "mkfs.ext4", "mkfs.fat", "mkfs.ntfs",
    "ddrescue", ":(){ :|:& };:",
    NULL
};
// clang-format on

static int32_t is_banned(const char *name)
{
    if (!name) return 0;

    const char *base = strrchr(name, '/');
    if (base)
        base = base + 1;
    else
        base = name;

    for (int i = 0; banned_cmds[i]; ++i)
    {
        if (strcmp(base, banned_cmds[i]) == 0) return 1;
    }
    return 0;
}

static void launcher_resize(qwm_t *qwm, launcher_t *l)
{
    uint32_t lines = 1 + l->match_count;
    if (lines > 1 + MAX_DRAW) lines = 1 + MAX_DRAW;

    l->h = (int16_t)(lines * LINE_HEIGHT + PADDING * 2);

    uint32_t values[] = {(uint32_t)l->h};
    xcb_configure_window(qwm->conn, l->win, XCB_CONFIG_WINDOW_HEIGHT, values);
}

static void str_copy(char *dst, const char *src, size_t size)
{
    if (size == 0) return;
    strncpy(dst, src, size - 1);
    dst[size - 1] = '\0';
}

static int32_t cmd_exists(launcher_t *l, const char *name)
{
    for (uint32_t i = 0; i < l->cmd_count; ++i)
    {
        if (strcmp(l->cmds[i].name, name) == 0) return 1;
    }
    return 0;
}

static int32_t ensure_cmd_capacity(launcher_t *l)
{
    if (l->cmd_count < l->cmd_cap) return 1;

    uint32_t new_cap = l->cmd_cap ? l->cmd_cap * 2 : 256;
    cmd_entry_t *new_cmds = realloc(l->cmds, new_cap * sizeof(cmd_entry_t));
    if (!new_cmds) return 0;

    l->cmds = new_cmds;
    l->cmd_cap = new_cap;
    return 1;
}

static void scan_path(launcher_t *l)
{
    const char *path_env = getenv("PATH");
    if (!path_env) return;

    char pathbuf[4096];
    str_copy(pathbuf, path_env, sizeof(pathbuf));

    l->cmd_count = 0;

    char *saveptr;
    char *dir = strtok_r(pathbuf, ":", &saveptr);

    while (dir)
    {
        DIR *d = opendir(dir);
        if (!d)
        {
            dir = strtok_r(NULL, ":", &saveptr);
            continue;
        }

        struct dirent *ent;
        while ((ent = readdir(d)))
        {
            if (ent->d_name[0] == '.') continue;

            char full[1024];
            snprintf(full, sizeof(full), "%s/%s", dir, ent->d_name);

            struct stat st;
            if (stat(full, &st) != 0) continue;
            if (!S_ISREG(st.st_mode)) continue;
            if (access(full, X_OK) != 0) continue;
            if (cmd_exists(l, ent->d_name)) continue;

            if (!ensure_cmd_capacity(l)) goto out;

            str_copy(l->cmds[l->cmd_count].name, ent->d_name,
                     sizeof(l->cmds[0].name));

            str_copy(l->cmds[l->cmd_count].path, full,
                     sizeof(l->cmds[0].path));

            l->cmd_count++;
        }

        closedir(d);
        dir = strtok_r(NULL, ":", &saveptr);
    }
out:;
    // printf("scanned %u commands\n", l->cmd_count);
}

static int simple_match(const char *input, const char *cmd)
{
    if (!input[0]) return 1;
    if (strncmp(cmd, input, strlen(input)) == 0) return 1;

    return strstr(cmd, input) != NULL;
}

void update_matches(launcher_t *l)
{
    l->match_count = 0;
    l->sel = 0;

    for (uint32_t i = 0; i < l->cmd_count; ++i)
    {
        if (!simple_match(l->input, l->cmds[i].name)) continue;

        uint16_t mi = l->match_count;
        l->match_indices[mi] = (uint16_t)i;
        l->match_count++;

        if (l->match_count == MAX_MATCH) break;
    }
}

static void spawn_exec(const char *cmd)
{
    if (!cmd || !*cmd) return;
    if (is_banned(cmd)) return;

    if (fork() == 0)
    {
        setsid();

        // execvp, execv, execlp ???????
        char *argv[] = {(char *)cmd, NULL};
        execvp(cmd, argv);

        perror("execvp");
        _exit(1);
    }
}

void launcher_init(launcher_t *l)
{
    memset(l, 0, sizeof(*l));

    const char *path_env = getenv("PATH");
    if (path_env) str_copy(l->scanned_path, path_env, sizeof(l->scanned_path));

    scan_path(l);
}

void launcher_kill(launcher_t *l)
{
    free(l->cmds);
    l->cmds = NULL;
    l->cmd_cap = 0;
    l->cmd_count = 0;
}

void launcher_open(struct qwm_t *qwm, launcher_t *l)
{
    l->opened = 0;
    l->input_len = 0;
    l->input[0] = '\0';
    l->match_count = 0;

    l->w = LAUNCHER_WIDTH;
    l->h = 24;
    l->x = (qwm->w - l->w) / 2;
    l->y = LAUNCHER_POSITION_Y;

    // clang-format off
    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
    uint32_t values[3] = {LAUNCHER_BG_COLOR, 1, XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_EXPOSURE};

    l->win = xcb_generate_id(qwm->conn);
    xcb_create_window(qwm->conn, XCB_COPY_FROM_PARENT, l->win, qwm->root,
					  l->x, l->y,
					  (uint16_t)l->w, (uint16_t)l->h,
					  0,
					  XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      qwm->screen->root_visual, mask, values);

    uint32_t stack_values[] = {XCB_NONE, XCB_STACK_MODE_ABOVE};
    xcb_configure_window(qwm->conn, l->win,
						 XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE,
						 stack_values);
    // clang-format on

    xcb_map_window(qwm->conn, l->win);
    xcb_set_input_focus(qwm->conn, XCB_INPUT_FOCUS_POINTER_ROOT, l->win,
                        XCB_CURRENT_TIME);

    l->sel_text_gc = xcb_generate_id(qwm->conn);
    uint32_t bg_values[] = {LAUNCHER_FG_COLOR};
    xcb_create_gc(qwm->conn, l->sel_text_gc, l->win, XCB_GC_FOREGROUND,
                  bg_values);

    l->text_gc = xcb_generate_id(qwm->conn);
    uint32_t text_values[] = {LAUNCHER_FONT_COLOR, LAUNCHER_FG_COLOR};
    xcb_create_gc(qwm->conn, l->text_gc, l->win,
                  XCB_GC_FOREGROUND | XCB_GC_BACKGROUND, text_values);

    xcb_flush(qwm->conn);
    l->opened = 1;
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
    l->opened = 0;
}

void launcher_draw(struct qwm_t *qwm, launcher_t *l)
{
    if (!l->opened) return;

    xcb_clear_area(qwm->conn, 0, l->win, 0, 0, (uint16_t)l->w, (uint16_t)l->h);

    int y = PADDING + LINE_HEIGHT;

    xcb_image_text_8(qwm->conn, (uint8_t)strlen(l->input), l->win,
                     qwm->taskbar.gc, 8, 16, l->input);

    // draw matches below
    uint32_t draw_count = l->match_count;
    if (draw_count > MAX_DRAW) draw_count = MAX_DRAW;

    for (uint32_t i = 0; i < draw_count; ++i)
    {
        uint16_t idx = l->match_indices[i];
        const char *name = l->cmds[idx].name;

        y += LINE_HEIGHT;
        if (i == l->sel)
        {
            xcb_rectangle_t r = {.x = 0,
                                 .y = (int16_t)(y - LINE_HEIGHT + 4),
                                 .width = (uint16_t)l->w,
                                 .height = (uint16_t)LINE_HEIGHT};

            xcb_poly_fill_rectangle(qwm->conn, l->win, l->sel_text_gc, 1, &r);
        }

        xcb_gcontext_t gc = (i == l->sel) ? l->text_gc : qwm->taskbar.gc;

        xcb_image_text_8(qwm->conn, (uint8_t)strlen(name), l->win, gc, PADDING,
                         (int16_t)y, name);
    }

    xcb_flush(qwm->conn);
}

void launcher_handle_event(struct qwm_t *qwm, launcher_t *l,
                           xcb_generic_event_t *ev)
{
    if (!qwm || !l || !ev) return;
    if ((ev->response_type & 0x7f) != XCB_KEY_PRESS) return;

    xcb_key_press_event_t *kp = (xcb_key_press_event_t *)ev;
    uint8_t code = kp->detail;

    if (code == KEY_ESCAPE) launcher_close(qwm, l);

    if (code == KEY_ENTER)
    {
        if (l->match_count > 0)
        {
            uint16_t idx = l->match_indices[l->sel];
            if (!is_banned(l->cmds[idx].path)) spawn_exec(l->cmds[idx].path);
        }
        else if (l->input_len > 0)
        {
            spawn_exec(l->input);
        }

        launcher_close(qwm, l);
        return;
    }

    if (code == KEY_DOWN)
    {
        if (l->match_count > 0)
        {
            if (l->sel + 1 < l->match_count) l->sel++;
        }
        launcher_draw(qwm, l);
        return;
    }

    if (code == KEY_UP)
    {
        if (l->match_count > 0)
        {
            if (l->sel > 0) l->sel--;
        }
        launcher_draw(qwm, l);
        return;
    }

    if (code == KEY_BACKSPACE)
    {
        if (l->input_len > 0)
        {
            l->input[--l->input_len] = '\0';
            update_matches(l);
            launcher_resize(qwm, l);
            launcher_draw(qwm, l);
        }
        return;
    }

    char ch = keycode_to_char(code);
    if (ch && l->input_len < sizeof(l->input) - 1)
    {
        l->input[l->input_len++] = ch;
        l->input[l->input_len] = '\0';

        update_matches(l);
        launcher_resize(qwm, l);
        launcher_draw(qwm, l);
        return;
    }
}

// TODO: revisit this later!!
static char keycode_to_char(uint8_t code)
{
    switch (code)
    {
    case KEY_A: return 'a';
    case KEY_B: return 'b';
    case KEY_C: return 'c';
    case KEY_D: return 'd';
    case KEY_E: return 'e';
    case KEY_F: return 'f';
    case KEY_G: return 'g';
    case KEY_H: return 'h';
    case KEY_I: return 'i';
    case KEY_J: return 'j';
    case KEY_K: return 'k';
    case KEY_L: return 'l';
    case KEY_M: return 'm';
    case KEY_N: return 'n';
    case KEY_O: return 'o';
    case KEY_P: return 'p';
    case KEY_Q: return 'q';
    case KEY_R: return 'r';
    case KEY_S: return 's';
    case KEY_T: return 't';
    case KEY_U: return 'u';
    case KEY_V: return 'v';
    case KEY_W: return 'w';
    case KEY_X: return 'x';
    case KEY_Y: return 'y';
    case KEY_Z: return 'z';

    case KEY_1: return '1';
    case KEY_2: return '2';
    case KEY_3: return '3';
    case KEY_4: return '4';
    case KEY_5: return '5';
    case KEY_6: return '6';
    case KEY_7: return '7';
    case KEY_8: return '8';
    case KEY_9: return '9';
    case KEY_0: return '0';
    case KEY_MINUS: return '-';

    case KEY_SPACE: return ' ';
    default: return 0;
    }
}

