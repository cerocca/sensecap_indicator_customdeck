#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    char  day_label[8];   /* giorno abbreviato: "Mon", "Tue", "Wed" */
    char  icon[8];        /* OWM icon code → ASCII via weather_icon_label */
    float temp_min;
    float temp_max;
} weather_day_t;

typedef struct {
    float temp;
    float feels_like;
    char  desc[32];
    char  icon[8];        /* OWM icon code es. "04d" */
    int   humidity;
    float wind_kph;       /* convertito in km/h (metric m/s * 3.6) o mph (imperial) */
    bool  is_metric;
    struct {
        int   hour;       /* ora slot (0-23) da dt_txt OWM forecast */
        char  icon[8];
        float temp;
    } forecast[3];        /* 3 slot × 3h da OWM /forecast?cnt=4 */
    weather_day_t days[3];
    int           days_count;  /* 0 se non disponibili */
    bool    valid;
    int64_t last_update_ms;
} weather_data_t;

/* Registra handler IP_EVENT_STA_GOT_IP e avvia il task poll. */
void indicator_weather_init(void);

/* Ritorna puntatore alla struttura dati meteo corrente (thread-safe in lettura). */
const weather_data_t *indicator_weather_get(void);

/* Mappa codice icona OWM → stringa ASCII breve per display LVGL.
 * LVGL 8.x + Montserrat non supporta emoji Unicode → testo ASCII.
 * Icone PNG reali sono TODO futuro (stessa eccezione sfondo clock). */
const char *weather_icon_label(const char *icon_code);
