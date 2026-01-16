#ifndef TRAY_STATUS_H
#define TRAY_STATUS_H

#include "views.h"

typedef struct {
    layout_type_t last_layout;
    uint16_t last_workspace;
    uint16_t client_count;
} views_t;

typedef struct {
    int32_t last_minute;
    char time[8];
    char date[16];
} time_date_t;

typedef struct {
    char name[16];
} governor_t;

typedef struct {
    int mhz;
    time_t last_update;
} cpu_status_t;

typedef struct {
    uint32_t total;
    uint32_t current;
    uint32_t last;
    time_t last_update;
} memory_t;

typedef enum {
    BAT_UNKNOWN = 0,
    BAT_CHARGING,
    BAT_DISCHARGING,
    BAT_FULL
} battery_state_t;

typedef struct {
    uint16_t capacity;
    battery_state_t state;
} battery_t;

typedef struct {
    uint64_t current;
    uint64_t last;
} uptime_t;

typedef enum {
    CONNECT = 0,
    DISCONNECT,
} connect_state_t;

typedef enum {
    UNKNOWN = 0,
    WIFI,
    LAN,
} connect_type_t;

typedef struct {
    connect_state_t cn_state;
    connect_type_t cn_type;
    char name[32];
    time_t last_update;
} connection_t;

typedef struct {
    views_t view;
    time_date_t time_date;
    governor_t gov;
    cpu_status_t cpu;
    memory_t mems;
    battery_t bat;
    uptime_t up;
    connection_t connection;
} tray_status_t;

void tray_init(tray_status_t *ts);

int32_t tray_update(struct qwm_t *wm, tray_status_t *ts);

#endif // TRAY_STATUS_H
