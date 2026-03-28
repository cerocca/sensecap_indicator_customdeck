#pragma once
#include "lvgl/lvgl.h"

extern lv_obj_t *ui_screen_weather;

/* Lightweight: crea lo schermo. Chiamare al boot. */
void screen_weather_init(void);

/* Heavy: crea tutti i widget. Lazy al primo swipe. */
void screen_weather_populate(void);
