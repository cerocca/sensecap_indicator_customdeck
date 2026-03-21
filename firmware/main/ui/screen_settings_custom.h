#pragma once
#include "lvgl/lvgl.h"

extern lv_obj_t *ui_screen_settings_custom;

/* Crea solo l'oggetto schermata (lightweight — chiamare al boot). */
void screen_settings_custom_init(void);

/* Aggiunge tutto il contenuto (tabview, campi, eventi).
 * Chiamare lazily prima della prima navigazione verso la schermata. */
void screen_settings_custom_populate(void);
