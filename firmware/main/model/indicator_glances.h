#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Singolo container Docker per la sezione Top Docker. */
typedef struct {
    char     name[33];  /* fino a 32 caratteri + null */
    uint32_t mem_mb;    /* memoria usata in MB */
} glances_proc_t;

/* Callback chiamata dal poll task (con LVGL sem già acquisito)
 * dopo ogni ciclo di fetch completo.
 * cpu, ram, dsk : percentuali 0–100 (-1.0 se errore di rete)
 * uptime        : stringa raw Glances (es. "1 day, 2:34:56") — buffer interno, copiare se necessario
 * load1/5/15    : load average 1/5/15 min (-1.0 se errore)
 * top_ram       : top 5 container Docker per mem_mb, ordinati decrescente */
typedef void (*glances_data_cb_t)(float cpu, float ram, float dsk,
                                  const char *uptime,
                                  float load1, float load5, float load15,
                                  const glances_proc_t top_ram[5]);

/* Registra handler IP_EVENT_STA_GOT_IP e legge config iniziale da NVS. */
void indicator_glances_init(void);

/* Registra la callback UI (chiamare da screen_sibilla_populate). */
void indicator_glances_set_callback(glances_data_cb_t cb);

/* Avvia / sospende il polling (il task rimane in vita). */
void indicator_glances_start(void);
void indicator_glances_stop(void);
