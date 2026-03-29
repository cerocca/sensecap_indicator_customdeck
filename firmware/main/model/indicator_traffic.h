#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int     duration_sec;
    int     duration_normal_sec;
    int     delta_sec;
    int     distance_m;
    char    status[8];       /* "ok", "slow", "bad", "error" */
    bool    valid;
    int64_t last_update_ms;  /* esp_timer_get_time()/1000 al momento dell'update */
} traffic_data_t;

extern traffic_data_t g_traffic;

/* Registra handler IP_EVENT_STA_GOT_IP e avvia il polling. */
void indicator_traffic_init(void);
