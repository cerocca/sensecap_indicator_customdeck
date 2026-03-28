#include "ui_manager.h"
#include "ui_helpers.h"   /* _ui_screen_change, include ui.h → ui_screen_time, ui_screen_setting */
#include "indicator_storage.h"
#include "indicator_weather.h"
#include "app_config.h"

/* ui_screen_last è non-static in ui.c (necessario per il ritorno da ui_screen_wifi). */
extern lv_obj_t *ui_screen_last;

/* Handler originale Seeed per ui_screen_time — necessario per rimuoverlo
 * e sostituirlo con gesture_clock che usa next_from() su RIGHT. */
extern void ui_event_screen_time(lv_event_t *e);

/*
 * Navigazione circolare bidirezionale (swipe LEFT = avanti, swipe RIGHT = indietro):
 *
 *  clock ↔ sensors ↔ settings ↔ [hue] ↔ [sibilla] ↔ [launcher] ↔ [weather] ↔ (clock)
 *
 * Le schermate tra [] sono opzionali: se disabilitate vengono saltate.
 * Clock, sensors e settings sono sempre visibili.
 *
 * Guard anti-reentrant: ogni handler verifica che la schermata attiva
 * sia quella di competenza prima di eseguire qualsiasi transizione.
 *
 * Lazy init: screen_settings_custom_populate() viene chiamata la prima volta
 * che l'utente naviga verso settings_custom (da sensors o da hue).
 */

/* ─── screen enable flags ──────────────────────────────────────
 * Caricati da NVS in ui_manager_init().
 * Aggiornati live da screen_settings_custom (tab Meteo / Schermate).
 * ─────────────────────────────────────────────────────────────*/
bool g_scr_defsens_enabled = true;
bool g_scr_hue_enabled  = true;
bool g_scr_srv_enabled  = true;
bool g_scr_lnch_enabled = true;
bool g_scr_wthr_enabled = true;

/* ─── screen order for skip logic ─────────────────────────────
 * Indici: 0=clock, 1=sensors, 2=settings, 3=hue, 4=sibilla, 5=launcher, 6=weather
 * ─────────────────────────────────────────────────────────────*/
#define N_SCREENS 7

static lv_obj_t *s_scr[N_SCREENS]; /* popolato in ui_manager_init */

static bool scr_enabled(int i)
{
    switch (i) {
        case 1: return g_scr_defsens_enabled; /* sensors: skippa se switch OFF */
        case 3: return g_scr_hue_enabled;
        case 4: return g_scr_srv_enabled;
        case 5: return g_scr_lnch_enabled;
        case 6: return g_scr_wthr_enabled;
        default: return true; /* clock (0), settings (2) sempre abilitati */
    }
}

/* Ritorna la schermata abilitata più vicina nella direzione dir (+1=LEFT, -1=RIGHT). */
static lv_obj_t *next_from(int cur, int dir)
{
    for (int i = 1; i < N_SCREENS; i++) {
        int idx = ((cur + dir * i) % N_SCREENS + N_SCREENS) % N_SCREENS;
        if (scr_enabled(idx)) return s_scr[idx];
    }
    return s_scr[cur]; /* fallback: impossibile se almeno 3 schermate sempre on */
}

/* ─── NVS helper ───────────────────────────────────────────── */
static bool nvs_read_flag(const char *key)
{
    char buf[4];
    size_t len = sizeof(buf);
    if (indicator_storage_read((char *)key, buf, &len) == 0 && len >= 1) {
        return buf[0] == '1';
    }
    return true; /* default: abilitata */
}

static void load_screen_flags(void)
{
    g_scr_defsens_enabled = nvs_read_flag(NVS_KEY_SCR_DEFSENS);
    g_scr_hue_enabled     = nvs_read_flag(NVS_KEY_SCR_HUE_EN);
    g_scr_srv_enabled     = nvs_read_flag(NVS_KEY_SCR_SRV_EN);
    g_scr_lnch_enabled    = nvs_read_flag(NVS_KEY_SCR_LNCH_EN);
    g_scr_wthr_enabled    = nvs_read_flag(NVS_KEY_SCR_WTHR);
}

/* ─── lazy init settings_custom ────────────────────────────── */
static bool s_settings_populated = false;

static void ensure_settings_populated(void)
{
    if (!s_settings_populated) {
        screen_settings_custom_populate();
        s_settings_populated = true;
    }
}

/* ─── lazy init screen_hue ──────────────────────────────────── */
static bool s_hue_populated = false;

static void ensure_hue_populated(void)
{
    if (!s_hue_populated) {
        screen_hue_populate();
        s_hue_populated = true;
    }
}

/* ─── lazy init screen_launcher ─────────────────────────────── */
static bool s_launcher_populated = false;

static void ensure_launcher_populated(void)
{
    if (!s_launcher_populated) {
        screen_launcher_populate();
        s_launcher_populated = true;
    }
}

/* ─── lazy init screen_weather ──────────────────────────────── */
static bool s_weather_populated = false;

static void ensure_weather_populated(void)
{
    if (!s_weather_populated) {
        screen_weather_populate();
        s_weather_populated = true;
    }
}

/* ─── gesture handlers ─────────────────────────────────────── */

/*
 * Sostituzione dell'handler Seeed ui_event_screen_time per ui_screen_time.
 * LEFT e BOTTOM replicano il comportamento Seeed invariato.
 * RIGHT usa next_from() per saltare le schermate disabilitate anziché
 * puntare hardcoded a ui_screen_weather.
 */
static void gesture_clock(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_GESTURE) return;
    if (lv_scr_act() != ui_screen_time) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT) {
        lv_obj_t *next = next_from(0, +1);
        if (next == ui_screen_settings_custom) ensure_settings_populated();
        _ui_screen_change(next, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0);
    }
    else if (dir == LV_DIR_RIGHT)
        _ui_screen_change(next_from(0, -1), LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0);
    else if (dir == LV_DIR_BOTTOM) {
        ui_screen_last = ui_screen_time;
        _ui_screen_change(ui_screen_wifi, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 200, 0);
    }
}

/*
 * Handler su ui_screen_sensor per lazy-init settings_custom.
 * Seeed gestisce già la navigazione sensors→settings_custom;
 * qui garantiamo solo che il contenuto sia popolato prima del render.
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
    if (dir == LV_DIR_LEFT) {
        ensure_hue_populated(); /* lazy populate prima dell'animazione */
        _ui_screen_change(next_from(2, +1), LV_SCR_LOAD_ANIM_MOVE_LEFT,  200, 0);
    } else if (dir == LV_DIR_RIGHT)
        _ui_screen_change(next_from(2, -1), LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0);
}

static void gesture_hue(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_GESTURE) return;
    if (lv_scr_act() != ui_screen_hue) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT)
        _ui_screen_change(next_from(3, +1), LV_SCR_LOAD_ANIM_MOVE_LEFT,  200, 0);
    else if (dir == LV_DIR_RIGHT) {
        ensure_settings_populated(); /* difensivo: garantisce tabview presente */
        _ui_screen_change(next_from(3, -1), LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0);
    }
}

static void gesture_sibilla(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_GESTURE) return;
    if (lv_scr_act() != ui_screen_sibilla) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT) {
        ensure_launcher_populated(); /* lazy populate prima dell'animazione */
        _ui_screen_change(next_from(4, +1), LV_SCR_LOAD_ANIM_MOVE_LEFT,  200, 0);
    } else if (dir == LV_DIR_RIGHT) {
        ensure_hue_populated(); /* lazy populate prima dell'animazione */
        _ui_screen_change(next_from(4, -1), LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0);
    }
}

static void gesture_launcher(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_GESTURE) return;
    if (lv_scr_act() != ui_screen_launcher) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT) {
        ensure_weather_populated(); /* lazy populate prima dell'animazione */
        _ui_screen_change(next_from(5, +1), LV_SCR_LOAD_ANIM_MOVE_LEFT,  200, 0);
    } else if (dir == LV_DIR_RIGHT)
        _ui_screen_change(next_from(5, -1), LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0);
}

static void gesture_weather(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_GESTURE) return;
    if (lv_scr_act() != ui_screen_weather) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT)
        _ui_screen_change(next_from(6, +1), LV_SCR_LOAD_ANIM_MOVE_LEFT,  200, 0);
    else if (dir == LV_DIR_RIGHT)
        _ui_screen_change(next_from(6, -1), LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0);
}

/* ─── auto-navigate to sensors at boot ─────────────────────── */
static void auto_nav_sensors_cb(lv_timer_t *t)
{
    (void)t;
    if (g_scr_defsens_enabled && lv_scr_act() == ui_screen_time)
        _ui_screen_change(ui_screen_sensor, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0);
}

void ui_manager_init(void)
{
    /* Carica i flag di abilitazione schermate da NVS. */
    load_screen_flags();

    /* Lightweight: crea solo l'oggetto schermata, senza tabview né widget.
     * La populate avviene lazily al primo swipe verso settings_custom. */
    screen_settings_custom_init();
    screen_hue_init();
    screen_sibilla_init();
    screen_launcher_init();
    screen_weather_init();

    /* Avvia il modello meteo (registra handler IP_EVENT_STA_GOT_IP). */
    indicator_weather_init();

    /* Popola la tabella degli indici schermata per next_from(). */
    s_scr[0] = ui_screen_time;
    s_scr[1] = ui_screen_sensor;
    s_scr[2] = ui_screen_settings_custom;
    s_scr[3] = ui_screen_hue;
    s_scr[4] = ui_screen_sibilla;
    s_scr[5] = ui_screen_launcher;
    s_scr[6] = ui_screen_weather;

    /* Sostituisce l'handler Seeed ui_event_screen_time con gesture_clock,
     * che usa next_from() su RIGHT invece del target hardcoded ui_screen_ai. */
    lv_obj_remove_event_cb(ui_screen_time, ui_event_screen_time);
    lv_obj_add_event_cb(ui_screen_time, gesture_clock, LV_EVENT_ALL, NULL);

    /* gesture_sensor_lazy_settings: popola settings_custom al primo swipe LEFT
     * da sensors. Registrato dopo il handler Seeed, ma la populate avviene
     * nello stesso event dispatch → prima del render dell'animazione. */
    lv_obj_add_event_cb(ui_screen_sensor,          gesture_sensor_lazy_settings, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_screen_settings_custom, gesture_settings_custom,      LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_screen_hue,             gesture_hue,                  LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_screen_sibilla,         gesture_sibilla,              LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_screen_launcher,        gesture_launcher,             LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_screen_weather,         gesture_weather,              LV_EVENT_ALL, NULL);

    /* Auto-navigate to sensors 3 s after boot if flag enabled (one-shot). */
    lv_timer_t *t = lv_timer_create(auto_nav_sensors_cb, 3000, NULL);
    lv_timer_set_repeat_count(t, 1);
}
