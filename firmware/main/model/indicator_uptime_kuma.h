#pragma once

#include <stdbool.h>

#define UK_MAX_DOWN 6

/* Callback chiamata dal poll task (con LVGL sem già acquisito).
 * total      : totale servizi (esclusi gruppi "0-...")
 * up         : servizi UP
 * names_down : nomi servizi DOWN (max UK_MAX_DOWN)
 * n_down     : numero servizi DOWN */
typedef void (*uptime_kuma_cb_t)(int total, int up,
                                  const char names_down[UK_MAX_DOWN][32],
                                  int n_down);

/* Registra handler IP_EVENT_STA_GOT_IP e legge config iniziale da NVS. */
void indicator_uptime_kuma_init(void);

/* Registra la callback UI (chiamare da screen_server_populate). */
void indicator_uptime_kuma_set_callback(uptime_kuma_cb_t cb);

/* Avvia / sospende il polling (il task rimane in vita). */
void indicator_uptime_kuma_start(void);
void indicator_uptime_kuma_stop(void);
