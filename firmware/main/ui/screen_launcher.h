#pragma once
#include "lvgl/lvgl.h"

extern lv_obj_t *ui_screen_launcher;

/* Lightweight: crea lo schermo. Chiamare al boot. */
void screen_launcher_init(void);

/* Heavy: crea i 4 pulsanti. Lazy al primo SCREEN_LOAD_START. */
void screen_launcher_populate(void);
