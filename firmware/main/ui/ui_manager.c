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

/* Handler originale Seeed per ui_screen_sensor — necessario per rimuoverlo:
 * aveva LEFT hardcoded su ui_screen_settings_custom (ora fuori rotazione). */
extern void ui_event_screen_sensor(lv_event_t *e);

/* Handler originale Seeed per ui_screen_setting — necessario per rimuoverlo
 * ed evitare doppia chiamata a _ui_screen_change su LV_DIR_TOP. */
extern void ui_event_screen_setting(lv_event_t *e);

/*
 * Navigazione:
 *
 * Orizzontale (swipe LEFT = avanti, swipe RIGHT = indietro):
 *  clock(0) ↔ sensors(1) ↔ [traffic(2,disab.)] ↔ [hue(3)] ↔ [sibilla(4)] ↔ [launcher(5)] ↔ [weather(6)] ↔ (clock)
 *
 * Verticale dal clock:
 *  swipe UP   → screen_settings_custom  (MOVE_TOP)
 *  swipe DOWN → ui_screen_setting Seeed (MOVE_BOTTOM)
 *
 * settings_custom è fuori dalla rotazione orizzontale; raggiungibile solo via swipe UP dal clock.
 * Guard anti-reentrant: ogni handler verifica che la schermata attiva sia quella di competenza.
 * Lazy init: screen_settings_custom_populate() al primo swipe UP dal clock.
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
 * Indici: 0=clock, 1=sensors, 2=traffic(riservato/disab.), 3=hue, 4=sibilla, 5=launcher, 6=weather
 * settings_custom è fuori dalla rotazione orizzontale.
 * ─────────────────────────────────────────────────────────────*/
#define N_SCREENS 7

static lv_obj_t *s_scr[N_SCREENS]; /* popolato in ui_manager_init */

static bool scr_enabled(int i)
{
    switch (i) {
        case 1: return g_scr_defsens_enabled; /* sensors: skippa se switch OFF */
        case 2: return false;                 /* traffic: placeholder, non ancora implementato */
        case 3: return g_scr_hue_enabled;
        case 4: return g_scr_srv_enabled;
        case 5: return g_scr_lnch_enabled;
        case 6: return g_scr_wthr_enabled;
        default: return true; /* clock (0) sempre abilitato */
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
 * LEFT/RIGHT: rotazione orizzontale con next_from().
 * UP:   → screen_settings_custom (fuori rotazione, accesso verticale).
 * DOWN: → ui_screen_setting Seeed (accesso verticale).
 */
static void gesture_clock(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_GESTURE) return;
    if (lv_scr_act() != ui_screen_time) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT)
        _ui_screen_change(next_from(0, +1), LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0);
    else if (dir == LV_DIR_RIGHT)
        _ui_screen_change(next_from(0, -1), LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0);
    else if (dir == LV_DIR_TOP) {
        ensure_settings_populated();
        _ui_screen_change(ui_screen_settings_custom, LV_SCR_LOAD_ANIM_MOVE_TOP, 200, 0);
    }
    else if (dir == LV_DIR_BOTTOM)
        _ui_screen_change(ui_screen_setting, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 200, 0);
}

/*
 * Sostituzione dell'handler Seeed ui_event_screen_sensor.
 * Il Seeed aveva LEFT hardcoded su ui_screen_settings_custom (ora fuori rotazione).
 * LEFT ora usa next_from(1, +1) saltando il placeholder traffic (slot 2).
 * RIGHT e TOP replicano il comportamento Seeed (→ clock).
 */
static void gesture_sensor(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_GESTURE) return;
    if (lv_scr_act() != ui_screen_sensor) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT) {
        ensure_hue_populated();
        _ui_screen_change(next_from(1, +1), LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0);
    } else if (dir == LV_DIR_RIGHT)
        _ui_screen_change(next_from(1, -1), LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0);
    else if (dir == LV_DIR_TOP)
        _ui_screen_change(ui_screen_time, LV_SCR_LOAD_ANIM_MOVE_TOP, 200, 0);
}

/*
 * Handler per screen_settings_custom: solo swipe DOWN torna al clock.
 * LEFT/RIGHT non navigano (settings è fuori dalla rotazione orizzontale).
 */
static void gesture_settings_custom(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_GESTURE) return;
    if (lv_scr_act() != ui_screen_settings_custom) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_BOTTOM)
        _ui_screen_change(ui_screen_time, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 200, 0);
}

/*
 * Handler aggiunto su ui_screen_setting (Seeed, NON modificata direttamente).
 * Swipe UP torna al clock.
 */
static void gesture_seeed_setting(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_GESTURE) return;
    if (lv_scr_act() != ui_screen_setting) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_TOP)
        _ui_screen_change(ui_screen_time, LV_SCR_LOAD_ANIM_MOVE_TOP, 200, 0);
}

static void gesture_hue(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_GESTURE) return;
    if (lv_scr_act() != ui_screen_hue) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT)
        _ui_screen_change(next_from(3, +1), LV_SCR_LOAD_ANIM_MOVE_LEFT,  200, 0);
    else if (dir == LV_DIR_RIGHT)
        _ui_screen_change(next_from(3, -1), LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0);
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

    /* Popola la tabella degli indici schermata per next_from().
     * Slot 2 riservato a screen_traffic (non ancora implementato, disabled). */
    s_scr[0] = ui_screen_time;
    s_scr[1] = ui_screen_sensor;
    s_scr[2] = NULL;              /* traffic placeholder */
    s_scr[3] = ui_screen_hue;
    s_scr[4] = ui_screen_sibilla;
    s_scr[5] = ui_screen_launcher;
    s_scr[6] = ui_screen_weather;

    /* Sostituisce l'handler Seeed ui_event_screen_time con gesture_clock
     * che usa next_from() e gestisce UP/DOWN per navigazione verticale. */
    lv_obj_remove_event_cb(ui_screen_time, ui_event_screen_time);
    lv_obj_add_event_cb(ui_screen_time, gesture_clock, LV_EVENT_ALL, NULL);

    /* Sostituisce l'handler Seeed ui_event_screen_sensor: aveva LEFT hardcoded
     * su ui_screen_settings_custom, ora fuori rotazione. */
    lv_obj_remove_event_cb(ui_screen_sensor, ui_event_screen_sensor);
    lv_obj_add_event_cb(ui_screen_sensor, gesture_sensor, LV_EVENT_ALL, NULL);

    /* settings_custom: solo swipe DOWN torna al clock (fuori dalla rotazione orizzontale). */
    lv_obj_add_event_cb(ui_screen_settings_custom, gesture_settings_custom, LV_EVENT_ALL, NULL);

    /* ui_screen_setting (Seeed): rimuove l'handler Seeed per evitare doppia chiamata
     * a _ui_screen_change su LV_DIR_TOP (stesso pattern di gesture_clock / ui_event_screen_time). */
    lv_obj_remove_event_cb(ui_screen_setting, ui_event_screen_setting);
    lv_obj_add_event_cb(ui_screen_setting, gesture_seeed_setting, LV_EVENT_ALL, NULL);

    /* Nasconde elementi non necessari su ui_screen_sensor (NON si modifica ui.c).
     * Nascondere il container nasconde automaticamente tutti i figli. */
    lv_obj_add_flag(ui_wifi__st_button_2, LV_OBJ_FLAG_HIDDEN); /* bottone status Wi-Fi (top-right) + icona figlia */
    lv_obj_add_flag(ui_scrolldots2,       LV_OBJ_FLAG_HIDDEN); /* container 3 puntini navigazione */

    /* Nasconde elementi non necessari su ui_screen_setting (NON si modifica ui.c).
     * Wi-Fi button e icona restano visibili e funzionanti. */
    lv_obj_add_flag(ui_setting_title, LV_OBJ_FLAG_HIDDEN); /* label "Setting" */
    lv_obj_add_flag(ui_setting_icon,  LV_OBJ_FLAG_HIDDEN); /* icona ingranaggio */
    lv_obj_add_flag(ui_scrolldots3,   LV_OBJ_FLAG_HIDDEN); /* container 3 puntini navigazione */

    lv_obj_add_event_cb(ui_screen_hue,      gesture_hue,      LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_screen_sibilla,  gesture_sibilla,  LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_screen_launcher, gesture_launcher, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_screen_weather,  gesture_weather,  LV_EVENT_ALL, NULL);
}
