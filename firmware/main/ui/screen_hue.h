#pragma once
#include "lvgl/lvgl.h"

extern lv_obj_t *ui_screen_hue;

/* Lightweight: crea solo l'oggetto schermata, senza widget. */
void screen_hue_init(void);

/* Heavy: crea le 4 card (nome + switch + slider), registra callback UI.
 * Chiamata lazily al primo swipe verso questa schermata. */
void screen_hue_populate(void);
