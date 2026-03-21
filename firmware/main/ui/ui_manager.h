#pragma once

#include "screen_settings_custom.h"
#include "screen_hue.h"
#include "screen_sibilla.h"
#include "screen_launcher.h"
#include "screen_ai.h"

/*
 * Inizializza le schermate custom e registra i gesture handler
 * per la navigazione circolare estesa.
 * Chiamare dopo indicator_view_init() (che chiama ui_init()).
 */
void ui_manager_init(void);
