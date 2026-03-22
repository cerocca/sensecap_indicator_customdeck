#pragma once

#include <stdbool.h>

#define HUE_LIGHT_COUNT 4

typedef struct {
    char  name[32];
    char  id[64];
    bool  on;
    float brightness;   /* 0.0 – 100.0 */
    bool  reachable;    /* set true after first successful GET */
} hue_light_t;

/* Callback chiamata dal poll task (con LVGL sem già acquisito)
 * dopo ogni aggiornamento di stato — aggiorna switch e slider in UI. */
typedef void (*hue_update_cb_t)(void);

/* Registra handler IP_EVENT_STA_GOT_IP e avvia il poll task. */
void indicator_hue_init(void);

/* Registra la callback UI (chiamare da screen_hue_populate). */
void indicator_hue_set_update_cb(hue_update_cb_t cb);

/* Snapshot thread-safe dello stato corrente. */
void indicator_hue_get_light(int idx, hue_light_t *out);

/* Comandi asincroni — lanciano un task FreeRTOS one-shot. */
void indicator_hue_toggle(int idx);
void indicator_hue_set_brightness(int idx, float brightness);
