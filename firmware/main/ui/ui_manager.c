#include "ui_manager.h"
#include "ui_helpers.h"   /* _ui_screen_change, include ui.h → ui_screen_time, ui_screen_setting */

/*
 * Navigazione circolare bidirezionale (swipe LEFT = avanti, swipe RIGHT = indietro):
 *
 *  clock ↔ sensors ↔ settings ↔ hue ↔ sibilla ↔ launcher ↔ ai ↔ (clock)
 *
 * Guard anti-reentrant: ogni handler verifica che la schermata attiva
 * sia quella di competenza prima di eseguire qualsiasi transizione.
 * Questo previene il doppio-swipe durante l'animazione.
 *
 * Lazy init: screen_settings_custom_populate() viene chiamata la prima volta
 * che l'utente naviga verso settings_custom (da sensors o da hue).
 * Al boot viene creato solo l'oggetto schermata (lightweight) per garantire
 * che il puntatore sia valido prima di qualsiasi _ui_screen_change.
 */

static bool s_settings_populated = false;

static void ensure_settings_populated(void)
{
    if (!s_settings_populated) {
        screen_settings_custom_populate();
        s_settings_populated = true;
    }
}

/*
 * Handler aggiunto su ui_screen_sensor per lazy-init settings_custom.
 * Viene registrato DOPO il handler Seeed (che naviga), ma la populate
 * avviene nello stesso tick dell'event dispatch — prima che lv_timer_handler
 * esegua l'animazione. Il contenuto è quindi pronto quando il frame viene renderizzato.
 */
static void gesture_sensor_lazy_settings(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_GESTURE) return;
    if (lv_scr_act() != ui_screen_sensor) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT)
        ensure_settings_populated();
}

static void gesture_settings_custom(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_GESTURE) return;
    if (lv_scr_act() != ui_screen_settings_custom) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT)
        _ui_screen_change(ui_screen_hue,      LV_SCR_LOAD_ANIM_MOVE_LEFT,  200, 0);
    else if (dir == LV_DIR_RIGHT)
        _ui_screen_change(ui_screen_sensor,   LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0);
}

static void gesture_hue(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_GESTURE) return;
    if (lv_scr_act() != ui_screen_hue) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT)
        _ui_screen_change(ui_screen_sibilla,          LV_SCR_LOAD_ANIM_MOVE_LEFT,  200, 0);
    else if (dir == LV_DIR_RIGHT) {
        ensure_settings_populated();
        _ui_screen_change(ui_screen_settings_custom,  LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0);
    }
}

static void gesture_sibilla(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_GESTURE) return;
    if (lv_scr_act() != ui_screen_sibilla) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT)
        _ui_screen_change(ui_screen_launcher, LV_SCR_LOAD_ANIM_MOVE_LEFT,  200, 0);
    else if (dir == LV_DIR_RIGHT)
        _ui_screen_change(ui_screen_hue,      LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0);
}

static void gesture_launcher(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_GESTURE) return;
    if (lv_scr_act() != ui_screen_launcher) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT)
        _ui_screen_change(ui_screen_ai,       LV_SCR_LOAD_ANIM_MOVE_LEFT,  200, 0);
    else if (dir == LV_DIR_RIGHT)
        _ui_screen_change(ui_screen_sibilla,  LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0);
}

static void gesture_ai(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_GESTURE) return;
    if (lv_scr_act() != ui_screen_ai) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT)
        _ui_screen_change(ui_screen_time,     LV_SCR_LOAD_ANIM_MOVE_LEFT,  200, 0);
    else if (dir == LV_DIR_RIGHT)
        _ui_screen_change(ui_screen_launcher, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0);
}

void ui_manager_init(void)
{
    /* Lightweight: crea solo l'oggetto schermata, senza tabview né widget.
     * La populate avviene lazily al primo swipe verso settings_custom. */
    screen_settings_custom_init();
    screen_hue_init();
    screen_sibilla_init();
    screen_launcher_init();
    screen_ai_init();

    /* gesture_sensor_lazy_settings: popola settings_custom al primo swipe LEFT
     * da sensors. Registrato dopo il handler Seeed, ma la populate avviene
     * nello stesso event dispatch → prima del render dell'animazione. */
    lv_obj_add_event_cb(ui_screen_sensor,          gesture_sensor_lazy_settings, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_screen_settings_custom, gesture_settings_custom,      LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_screen_hue,             gesture_hue,                  LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_screen_sibilla,         gesture_sibilla,              LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_screen_launcher,        gesture_launcher,             LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_screen_ai,              gesture_ai,                   LV_EVENT_ALL, NULL);
}
