#pragma once

#include "screen_settings_custom.h"
#include "screen_hue.h"
#include "screen_server.h"
#include "screen_launcher.h"
#include "screen_weather.h"
#include "screen_traffic.h"
#include <stdbool.h>

/*
 * Inizializza le schermate custom e registra i gesture handler
 * per la navigazione circolare estesa.
 * Chiamare dopo indicator_view_init() (che chiama ui_init()).
 */
void ui_manager_init(void);

/*
 * Flag di abilitazione schermate custom.
 * Caricati da NVS in ui_manager_init().
 * Aggiornati da screen_settings_custom quando l'utente cambia uno switch.
 * Default: true (abilitata).
 */
extern bool g_scr_defsens_enabled;
extern bool g_scr_hue_enabled;
extern bool g_scr_srv_enabled;
extern bool g_scr_lnch_enabled;
extern bool g_scr_wthr_enabled;
extern bool g_scr_traffic_enabled;
