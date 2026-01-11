#include "qwm.h"
#include "tray_status.h"
#include "util.h"

#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BAT_CAPACITY "/sys/class/power_supply/BAT0/capacity"
#define BAT_STATUS "/sys/class/power_supply/BAT0/status"
#define CPU_GOV_PATH "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
#define CPU_FREQ_PATH "/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq"
#define MEMINFO_PATH "/proc/meminfo"

static int32_t get_active_connection(char *name, size_t namesz, char *type,
                                     size_t typesz)
{
    int pipefd[2];
    if (pipe(pipefd) < 0) return -1;

    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0)
    {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        execlp("nmcli", "nmcli", "-t", "-f", "NAME,TYPE", "connection", "show",
               "--active", (char *)NULL);
        _exit(1);
    }
    close(pipefd[1]);

    FILE *fp = fdopen(pipefd[0], "r");
    if (!fp) return -1;

    char line[128];
    int ret = -1;
    while (fgets(line, sizeof(line), fp))
    {
        char *colon = strchr(line, ':');
        if (!colon) continue;

        *colon = '\0';
        char *n = line;
        char *t = colon + 1;
        t[strcspn(t, "\r\n")] = 0;

        strncpy(name, n, namesz - 1);
        name[namesz - 1] = '\0';
        strncpy(type, t, typesz - 1);
        type[typesz - 1] = '\0';
        ret = 0;
        break; // NOTE: first active connection only
    }

    fclose(fp);
    waitpid(pid, NULL, 0);
    return ret;
}

static int32_t update_connection(connection_t *conn)
{
    char name[32] = {0};
    char type[16] = {0};
    connect_state_t new_state = DISCONNECT;
    connect_type_t new_type = UNKNOWN;

    if (get_active_connection(name, sizeof(name), type, sizeof(type)) == 0)
    {
        new_state = CONNECT;
        if (strcmp(type, "802-11-wireless") == 0)
        {
            new_type = WIFI;
        }
        else if (strcmp(type, "ethernet") == 0)
        {
            new_type = LAN;
        }
        else
        {
            new_type = UNKNOWN;
            new_state = DISCONNECT;
        }
    }

    if (conn->cn_state != new_state)
    {
        conn->cn_state = new_state;
        return 1;
    }

    if (conn->cn_type != new_type)
    {
        conn->cn_type = new_type;
        return 1;
    }

    if (strcmp(conn->name, name) != 0)
    {
        if (strcmp(name, "lo") == 0)
            conn->name[0] = '\0';
        else
            snprintf(conn->name, sizeof(conn->name), "%s", name);
        return 1;
    }

    return 0;
}

static int32_t update_workspace(qwm_t *wm, views_t *vw)
{
    if (vw->last_workspace != wm->current_ws)
    {
        vw->last_workspace = wm->current_ws;
        return 1;
    }

    layout_type_t cur = wm->workspaces[wm->current_ws].type;
    if (vw->last_layout != cur)
    {
        vw->last_layout = cur;
        return 1;
    }

    return 0;
}

static int32_t update_clock(time_date_t *td)
{
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    if (tm->tm_min != td->last_minute)
    {
        td->last_minute = tm->tm_min;
        strftime(td->date, sizeof(td->date), "| %d-%m-%Y", tm);
        strftime(td->time, sizeof(td->time), "| %H:%M", tm);
        return 1;
    }
    return 0;
}

static int32_t update_governor(governor_t *g)
{
    char buf[16];

    if (file_read_string(CPU_GOV_PATH, buf, sizeof(buf)) < 0) return 0;

    if (strcmp(buf, g->name) != 0)
    {
        strncpy(g->name, buf, sizeof(g->name) - 1);
        g->name[sizeof(g->name) - 1] = '\0';
        return 1;
    }

    return 0;
}

static int32_t update_cpu_freq(cpu_status_t *cpu)
{
    // update per 5 seconds.
    time_t now = time(NULL);
    if (now - cpu->last_update < 5) return 0;

    cpu->last_update = now;

    long khz;
    if (file_read_long(CPU_FREQ_PATH, &khz) < 0) return 0;

    int32_t mhz = (int)khz / 1000;
    if (abs(mhz - cpu->mhz) >= 50)
    {
        cpu->mhz += (mhz - cpu->mhz) / 2;
        return 1;
    }

    return 0;
}

static void memory_init(memory_t *mem)
{
    FILE *f = fopen(MEMINFO_PATH, "r");
    if (!f) return;

    char key[32];
    uint64_t value;
    char unit[16];

    while (fscanf(f, "%31s %lu %15s\n", key, &value, unit) == 3)
    {
        if (strcmp(key, "MemTotal:") == 0)
        {
            mem->total = (uint32_t)(value / 1024);
            break;
        }
    }

    fclose(f);

    mem->current = 0;
    mem->last = 0;
}

static int32_t update_memory(memory_t *mem)
{
    // update per 30 seconds. 60 update per minute
    time_t now = time(NULL);
    if (now - mem->last_update < 30) return 0;
    mem->last_update = now;

    FILE *f = fopen(MEMINFO_PATH, "r");
    if (!f) return 0;

    uint64_t value;
    uint64_t available = 0;

    char line[128];
    while (fgets(line, sizeof(line), f))
    {
        if (sscanf(line, "MemAvailable: %lu", &value) == 1)
        {
            available = value;
            break;
        }
    }
    fclose(f);

    if (!available || !mem->total) return 0;
    uint32_t used_mb = mem->total - (uint32_t)(available / 1024);
    if (used_mb != mem->current)
    {
        mem->last = mem->current;
        mem->current = used_mb;
        return 1;
    }

    return 0;
}

static int32_t update_battery_status(battery_t *bat)
{
    int cap;
    if (file_read_int(BAT_CAPACITY, &cap) == 0)
    {
        uint16_t new_cap = (uint16_t)cap;
        if (new_cap != bat->capacity)
        {
            bat->capacity = new_cap;
            return 1;
        }
    }

    char buf[16];
    if (file_read_string(BAT_STATUS, buf, sizeof(buf)) == 0)
    {
        battery_state_t new_state = BAT_UNKNOWN;

        if (strcmp(buf, "Charging") == 0)
            new_state = BAT_CHARGING;
        else if (strcmp(buf, "Discharging") == 0)
            new_state = BAT_DISCHARGING;
        else if (strcmp(buf, "Full") == 0)
            new_state = BAT_FULL;

        if (new_state != bat->state)
        {
            bat->state = new_state;
            return 1;
        }
    }

    return 0;
}

static int32_t update_uptime(uptime_t *up)
{
    double seconds;
    if (file_read_double("/proc/uptime", &seconds) < 0) return 0;

    uint64_t minutes = (uint64_t)(seconds / 60);
    if (minutes != up->current)
    {
        up->last = up->current;
        up->current = minutes;
        return 1;
    }

    return 0;
}

void tray_init(tray_status_t *ts)
{
    memset(ts, 0, sizeof(*ts));
    memory_init(&ts->mems);
}

int32_t tray_update(struct qwm_t *wm, tray_status_t *ts)
{
    int32_t dirty = 0;

    dirty |= update_workspace(wm, &ts->view);

    dirty |= update_clock(&ts->time_date);
    dirty |= update_governor(&ts->gov);
    dirty |= update_cpu_freq(&ts->cpu);
    dirty |= update_memory(&ts->mems);
    dirty |= update_battery_status(&ts->bat);
    dirty |= update_uptime(&ts->up);

    dirty |= update_connection(&ts->connection);

    return dirty;
}

