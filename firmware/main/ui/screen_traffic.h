#pragma once
#include "lvgl/lvgl.h"

extern lv_obj_t *ui_screen_traffic;

/* Lightweight: crea lo schermo. Chiamare al boot. */
void screen_traffic_init(void);

/* Heavy: crea tutti i widget. Lazy al primo swipe. */
void screen_traffic_populate(void);

/* Ritorna il puntatore allo schermo (per s_scr[2] in ui_manager). */
lv_obj_t *screen_traffic_get_screen(void);
