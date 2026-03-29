#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    char name[32];
    int  duration_sec;
    int  duration_normal_sec;
    int  delta_sec;
    int  distance_m;
    char status[8];       /* "ok", "slow", "bad" */
} traffic_route_t;

typedef struct {
    traffic_route_t routes[2];
    int     route_count;      /* quante route valide (0, 1 o 2) */
    bool    valid;
    int64_t last_update_ms;   /* esp_timer_get_time()/1000 al momento dell'update */
} traffic_data_t;

extern traffic_data_t g_traffic;

/* Registra handler IP_EVENT_STA_GOT_IP e avvia il polling. */
void indicator_traffic_init(void);

/* Esegue immediatamente un poll one-shot senza attendere il timer.
 * Chiamare dopo config_fetch_from_proxy() per aggiornare la schermata Traffic. */
void indicator_traffic_force_poll(void);
