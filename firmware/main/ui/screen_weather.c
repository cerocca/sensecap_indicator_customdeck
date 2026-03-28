#include "screen_weather.h"
#include "indicator_weather.h"
#include "indicator_storage.h"
#include "app_config.h"
#include "lvgl/lvgl.h"
#include "esp_timer.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ─── Screen object ──────────────────────────────────────────────────────── */

lv_obj_t *ui_screen_weather;

/* ─── Widget handles (validi solo dopo populate) ─────────────────────────── */

static bool       s_populated       = false;
static lv_obj_t  *lbl_city          = NULL;
static lv_obj_t  *lbl_icon          = NULL;
static lv_obj_t  *lbl_temp          = NULL;
static lv_obj_t  *lbl_feels         = NULL;
static lv_obj_t  *lbl_desc          = NULL;
static lv_obj_t  *lbl_hum_wind      = NULL;
static lv_obj_t  *lbl_fc_hour[3];
static lv_obj_t  *lbl_fc_icon[3];
static lv_obj_t  *lbl_fc_temp[3];
static lv_obj_t  *lbl_updated       = NULL;
static lv_obj_t  *lbl_error         = NULL;
static lv_timer_t *s_refresh_timer  = NULL;

/* ─── UI update from data ────────────────────────────────────────────────── */

static void weather_update_ui(void)
{
    const weather_data_t *d = indicator_weather_get();
    char buf[64];

    if (!d->valid) {
        lv_label_set_text(lbl_icon,     "---");
        lv_label_set_text(lbl_temp,     "--\xc2\xb0");
        lv_label_set_text(lbl_feels,    "");
        lv_label_set_text(lbl_desc,     "");
        lv_label_set_text(lbl_hum_wind, "");
        for (int i = 0; i < 3; i++) {
            lv_label_set_text(lbl_fc_hour[i], "");
            lv_label_set_text(lbl_fc_icon[i], "");
            lv_label_set_text(lbl_fc_temp[i], "");
        }
        lv_label_set_text(lbl_updated, "No data");
        lv_obj_clear_flag(lbl_error, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(lbl_error, "Configure OWM key via proxy");
        return;
    }

    lv_label_set_text(lbl_icon, weather_icon_label(d->icon));

    snprintf(buf, sizeof(buf), "%.0f%s",
             d->temp, d->is_metric ? "\xc2\xb0" "C" : "\xc2\xb0" "F");
    lv_label_set_text(lbl_temp, buf);

    snprintf(buf, sizeof(buf), "Feels %.0f%s",
             d->feels_like, d->is_metric ? "\xc2\xb0" "C" : "\xc2\xb0" "F");
    lv_label_set_text(lbl_feels, buf);

    lv_label_set_text(lbl_desc, d->desc);

    snprintf(buf, sizeof(buf), "H: %d%%   W: %.0f %s",
             d->humidity,
             d->wind_kph,
             d->is_metric ? "km/h" : "mph");
    lv_label_set_text(lbl_hum_wind, buf);

    for (int i = 0; i < 3; i++) {
        snprintf(buf, sizeof(buf), "%02d:00", d->forecast[i].hour);
        lv_label_set_text(lbl_fc_hour[i], buf);
        lv_label_set_text(lbl_fc_icon[i], weather_icon_label(d->forecast[i].icon));
        snprintf(buf, sizeof(buf), "%.0f%s",
                 d->forecast[i].temp, d->is_metric ? "\xc2\xb0" "C" : "\xc2\xb0" "F");
        lv_label_set_text(lbl_fc_temp[i], buf);
    }

    /* "Aggiornato HH:MM" da last_update_ms (ms dall'avvio, non ora di sistema) */
    int64_t sec = d->last_update_ms / 1000LL;
    int min_ago = (int)((esp_timer_get_time() / 1000000LL - sec) / 60);
    if (min_ago < 1)
        snprintf(buf, sizeof(buf), "Updated just now");
    else if (min_ago < 60)
        snprintf(buf, sizeof(buf), "Updated %d min ago", min_ago);
    else
        snprintf(buf, sizeof(buf), "Updated %dh ago", min_ago / 60);
    lv_label_set_text(lbl_updated, buf);

    lv_obj_add_flag(lbl_error, LV_OBJ_FLAG_HIDDEN);
}

/* ─── Refresh timer callback (ogni 30s) ─────────────────────────────────── */

static int64_t s_last_update_shown = -1;

static void weather_refresh_cb(lv_timer_t *timer)
{
    if (lv_scr_act() != ui_screen_weather) return;
    if (!s_populated) return;
    const weather_data_t *d = indicator_weather_get();
    /* Aggiorna sempre (i minuti fa cambiano anche senza nuovi dati) */
    weather_update_ui();
    s_last_update_shown = d->last_update_ms;
}

/* ─── City label update ──────────────────────────────────────────────────── */

static void update_city_label(void)
{
    if (!lbl_city) return;
    /* Priority 1: wth_location (es. "Firenze, IT") */
    char city[64];
    size_t sz = sizeof(city);
    if (indicator_storage_read((char *)NVS_KEY_WTH_LOCATION, city, &sz) == 0 && sz > 1) {
        lv_label_set_text(lbl_city, city);
        return;
    }
    /* Priority 2: wth_city (nome città da proxy) */
    sz = sizeof(city);
    if (indicator_storage_read((char *)NVS_KEY_WTH_CITY, city, &sz) == 0 && sz > 1) {
        lv_label_set_text(lbl_city, city);
        return;
    }
    /* Fallback: format lat/lon as "43.77°N 11.25°E" */
    char lat[24], lon[24];
    sz = sizeof(lat);
    if (indicator_storage_read((char *)NVS_KEY_WTH_LAT, lat, &sz) != 0 || sz <= 1) {
        lv_label_set_text(lbl_city, "");
        return;
    }
    sz = sizeof(lon);
    if (indicator_storage_read((char *)NVS_KEY_WTH_LON, lon, &sz) != 0 || sz <= 1) {
        lv_label_set_text(lbl_city, "");
        return;
    }
    float flat = strtof(lat, NULL);
    float flon = strtof(lon, NULL);
    char buf[48];
    snprintf(buf, sizeof(buf), "%.2f\xc2\xb0%c %.2f\xc2\xb0%c",
             flat < 0 ? -flat : flat, flat < 0 ? 'S' : 'N',
             flon < 0 ? -flon : flon, flon < 0 ? 'W' : 'E');
    lv_label_set_text(lbl_city, buf);
}

/* ─── Screen load event ─────────────────────────────────────────────────── */

static void on_screen_load_start(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_SCREEN_LOAD_START) return;
    if (!s_populated) return;
    update_city_label();
    weather_update_ui();
}

/* ─── Populate (heavy, lazy al primo swipe) ──────────────────────────────── */

void screen_weather_populate(void)
{
    lv_obj_t *scr = ui_screen_weather;

    /* ── Header ── */
    lv_obj_t *hdr = lv_label_create(scr);
    lv_label_set_text(hdr, "Weather");
    lv_obj_set_style_text_color(hdr, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(hdr, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 10);

    /* ── Separatore header ── */
    lv_obj_t *sep_hdr = lv_obj_create(scr);
    lv_obj_set_size(sep_hdr, 440, 1);
    lv_obj_set_pos(sep_hdr, 20, 34);
    lv_obj_set_style_bg_color(sep_hdr, lv_color_hex(0x2a2a3a), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(sep_hdr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(sep_hdr, LV_OBJ_FLAG_SCROLLABLE);

    /* ── City / coords ── */
    lbl_city = lv_label_create(scr);
    lv_label_set_text(lbl_city, "");
    lv_obj_set_style_text_color(lbl_city, lv_color_hex(0xaaaaaa), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_city, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_width(lbl_city, 440);
    lv_obj_set_style_text_align(lbl_city, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(lbl_city, LV_ALIGN_TOP_MID, 0, 57);

    /* ── Icona condizione (testo ASCII) ── */
    lbl_icon = lv_label_create(scr);
    lv_label_set_text(lbl_icon, "---");
    lv_obj_set_style_text_color(lbl_icon, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_icon, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(lbl_icon, LV_ALIGN_TOP_MID, 0, 93);

    /* ── Temperatura ── */
    lbl_temp = lv_label_create(scr);
    lv_label_set_text(lbl_temp, "--\xc2\xb0");
    lv_obj_set_style_text_color(lbl_temp, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_temp, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(lbl_temp, LV_ALIGN_TOP_MID, 0, 123);

    /* ── Feels like ── */
    lbl_feels = lv_label_create(scr);
    lv_label_set_text(lbl_feels, "");
    lv_obj_set_style_text_color(lbl_feels, lv_color_hex(0x888888), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_feels, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(lbl_feels, LV_ALIGN_TOP_MID, 0, 171);

    /* ── Descrizione ── */
    lbl_desc = lv_label_create(scr);
    lv_label_set_text(lbl_desc, "");
    lv_obj_set_style_text_color(lbl_desc, lv_color_hex(0xcccccc), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_desc, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(lbl_desc, LV_ALIGN_TOP_MID, 0, 195);

    /* ── Riga umidità + vento ── */
    lbl_hum_wind = lv_label_create(scr);
    lv_label_set_text(lbl_hum_wind, "H: --%   W: -- km/h");
    lv_obj_set_style_text_color(lbl_hum_wind, lv_color_hex(0xaaaaaa), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_hum_wind, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(lbl_hum_wind, LV_ALIGN_TOP_MID, 0, 227);

    /* ── Separatore centrale ── */
    lv_obj_t *sep = lv_obj_create(scr);
    lv_obj_set_size(sep, 440, 1);
    lv_obj_set_pos(sep, 20, 257);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0x2a2a3a), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(sep, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

    /* ── Header previsione ── */
    lv_obj_t *fc_hdr = lv_label_create(scr);
    lv_label_set_text(fc_hdr, "Next hours");
    lv_obj_set_style_text_color(fc_hdr, lv_color_hex(0x7ec8e0), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(fc_hdr, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(fc_hdr, LV_ALIGN_TOP_MID, 0, 267);

    /* ── 3 colonne previsione (160px ciascuna, 480/3=160) ── */
    static const int fc_x[3] = {0, 160, 320};
    for (int i = 0; i < 3; i++) {
        /* Ora slot */
        lbl_fc_hour[i] = lv_label_create(scr);
        lv_label_set_text(lbl_fc_hour[i], "--:00");
        lv_obj_set_style_text_color(lbl_fc_hour[i], lv_color_hex(0x888888),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_fc_hour[i], &lv_font_montserrat_14,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_width(lbl_fc_hour[i], 160);
        lv_obj_set_style_text_align(lbl_fc_hour[i], LV_TEXT_ALIGN_CENTER,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_pos(lbl_fc_hour[i], fc_x[i], 293);

        /* Icona condizione */
        lbl_fc_icon[i] = lv_label_create(scr);
        lv_label_set_text(lbl_fc_icon[i], "---");
        lv_obj_set_style_text_color(lbl_fc_icon[i], lv_color_white(),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_fc_icon[i], &lv_font_montserrat_14,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_width(lbl_fc_icon[i], 160);
        lv_obj_set_style_text_align(lbl_fc_icon[i], LV_TEXT_ALIGN_CENTER,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_pos(lbl_fc_icon[i], fc_x[i], 317);

        /* Temperatura */
        lbl_fc_temp[i] = lv_label_create(scr);
        lv_label_set_text(lbl_fc_temp[i], "--\xc2\xb0");
        lv_obj_set_style_text_color(lbl_fc_temp[i], lv_color_white(),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_fc_temp[i], &lv_font_montserrat_14,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_width(lbl_fc_temp[i], 160);
        lv_obj_set_style_text_align(lbl_fc_temp[i], LV_TEXT_ALIGN_CENTER,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_pos(lbl_fc_temp[i], fc_x[i], 339);
    }

    /* ── Separatore basso ── */
    lv_obj_t *sep2 = lv_obj_create(scr);
    lv_obj_set_size(sep2, 440, 1);
    lv_obj_set_pos(sep2, 20, 361);
    lv_obj_set_style_bg_color(sep2, lv_color_hex(0x2a2a3a), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(sep2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(sep2, LV_OBJ_FLAG_SCROLLABLE);

    /* ── Label "Aggiornato" ── */
    lbl_updated = lv_label_create(scr);
    lv_label_set_text(lbl_updated, "No data");
    lv_obj_set_style_text_color(lbl_updated, lv_color_hex(0x555555),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_updated, &lv_font_montserrat_12,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(lbl_updated, LV_ALIGN_BOTTOM_MID, 0, -64);

    /* ── Label errore ── */
    lbl_error = lv_label_create(scr);
    lv_label_set_text(lbl_error, "Configure OWM key via proxy");
    lv_obj_set_style_text_color(lbl_error, lv_color_hex(0xe07070),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_error, &lv_font_montserrat_12,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(lbl_error, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_set_width(lbl_error, 440);
    lv_obj_set_style_text_align(lbl_error, LV_TEXT_ALIGN_CENTER,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_long_mode(lbl_error, LV_LABEL_LONG_WRAP);

    /* ── Timer refresh ogni 30s ── */
    s_refresh_timer = lv_timer_create(weather_refresh_cb, 30000, NULL);

    s_populated = true;

    /* Prima populate immediata */
    update_city_label();
    weather_update_ui();
}

/* ─── Init (lightweight — solo schermo + event handler) ─────────────────── */

void screen_weather_init(void)
{
    ui_screen_weather = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_screen_weather, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_screen_weather, lv_color_hex(0x0a0f1a),
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_screen_weather, on_screen_load_start,
                        LV_EVENT_ALL, NULL);
}
