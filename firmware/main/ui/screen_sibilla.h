#pragma once
#include "lvgl/lvgl.h"

extern lv_obj_t *ui_screen_sibilla;

/* Lightweight: crea lo schermo e l'header. Chiamare al boot. */
void screen_sibilla_init(void);

/* Heavy: crea tutti i widget metriche. Lazy al primo SCREEN_LOAD_START. */
void screen_sibilla_populate(void);
