#include "screen_traffic.h"
#include "indicator_traffic.h"
#include "app_config.h"
#include "lvgl/lvgl.h"
#include "esp_timer.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ─── Screen object ──────────────────────────────────────────────────────── */

lv_obj_t *ui_screen_traffic;

/* ─── Widget handles (validi solo dopo populate) ─────────────────────────── */

static bool       s_populated      = false;
static lv_obj_t  *lbl_status       = NULL;
static lv_obj_t  *lbl_duration     = NULL;
static lv_obj_t  *lbl_delta        = NULL;
static lv_obj_t  *lbl_distance     = NULL;
static lv_obj_t  *lbl_updated      = NULL;
static lv_obj_t  *lbl_error        = NULL;
static lv_timer_t *s_refresh_timer = NULL;

/* ─── UI update from g_traffic ───────────────────────────────────────────── */

void screen_traffic_update(void)
{
    if (!s_populated) return;

    char buf[64];

    if (!g_traffic.valid) {
        lv_label_set_text(lbl_status,   "NO DATA");
        lv_obj_set_style_text_color(lbl_status, lv_color_hex(0x888888),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(lbl_duration, "--");
        lv_label_set_text(lbl_delta,    "");
        lv_label_set_text(lbl_distance, "");
        lv_label_set_text(lbl_updated,  "No data");
        lv_obj_clear_flag(lbl_error, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(lbl_error, "Configure proxy: /traffic");
        return;
    }

    /* Indicatore stato */
    lv_color_t status_color;
    const char *status_text;
    if (strcmp(g_traffic.status, "ok") == 0) {
        status_color = lv_color_hex(0x4caf50);
        status_text  = "● OK";
    } else if (strcmp(g_traffic.status, "slow") == 0) {
        status_color = lv_color_hex(0xff9800);
        status_text  = "● SLOW";
    } else {
        status_color = lv_color_hex(0xf44336);
        status_text  = "● HEAVY";
    }
    lv_label_set_text(lbl_status, status_text);
    lv_obj_set_style_text_color(lbl_status, status_color,
                                LV_PART_MAIN | LV_STATE_DEFAULT);

    /* Tempo stimato */
    int dur_min = g_traffic.duration_sec / 60;
    snprintf(buf, sizeof(buf), "%d min", dur_min);
    lv_label_set_text(lbl_duration, buf);

    /* Delta vs normale */
    int delta_min = g_traffic.delta_sec / 60;
    if (delta_min <= 0) {
        lv_label_set_text(lbl_delta, "On time");
        lv_obj_set_style_text_color(lbl_delta, lv_color_hex(0x4caf50),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        snprintf(buf, sizeof(buf), "+%d min vs normal", delta_min);
        lv_label_set_text(lbl_delta, buf);
        lv_obj_set_style_text_color(lbl_delta, status_color,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    /* Distanza */
    if (g_traffic.distance_m >= 1000) {
        snprintf(buf, sizeof(buf), "%.1f km",
                 (float)g_traffic.distance_m / 1000.0f);
    } else {
        snprintf(buf, sizeof(buf), "%d m", g_traffic.distance_m);
    }
    lv_label_set_text(lbl_distance, buf);

    /* "Aggiornato X min fa" */
    int64_t now_ms  = esp_timer_get_time() / 1000LL;
    int     min_ago = (int)((now_ms - g_traffic.last_update_ms) / 60000LL);
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

static void traffic_refresh_cb(lv_timer_t *timer)
{
    if (lv_scr_act() != ui_screen_traffic) return;
    if (!s_populated) return;
    screen_traffic_update();
}

/* ─── Screen load event ─────────────────────────────────────────────────── */

static void on_screen_load_start(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_SCREEN_LOAD_START) return;
    if (!s_populated) return;
    screen_traffic_update();
}

/* ─── Populate (heavy, lazy al primo swipe) ──────────────────────────────── */

void screen_traffic_populate(void)
{
    lv_obj_t *scr = ui_screen_traffic;

    /* ── Header ── */
    lv_obj_t *hdr = lv_label_create(scr);
    lv_label_set_text(hdr, "Traffic");
    lv_obj_set_style_text_color(hdr, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(hdr, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 10);

    /* ── Separatore header (y≈55) ── */
    lv_obj_t *sep_hdr = lv_obj_create(scr);
    lv_obj_set_size(sep_hdr, 440, 1);
    lv_obj_set_pos(sep_hdr, 20, 55);
    lv_obj_set_style_bg_color(sep_hdr, lv_color_hex(0x2a2a3a), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(sep_hdr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(sep_hdr, LV_OBJ_FLAG_SCROLLABLE);

    /* ── Indicatore stato ● (y=90) ── */
    lbl_status = lv_label_create(scr);
    lv_label_set_text(lbl_status, "● --");
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_20,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_status, lv_color_hex(0x888888),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_width(lbl_status, 440);
    lv_obj_set_style_text_align(lbl_status, LV_TEXT_ALIGN_CENTER,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_pos(lbl_status, 20, 90);

    /* ── Tempo stimato (y=130) ── */
    lbl_duration = lv_label_create(scr);
    lv_label_set_text(lbl_duration, "--");
    lv_obj_set_style_text_color(lbl_duration, lv_color_white(),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_duration, &lv_font_montserrat_20,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_width(lbl_duration, 440);
    lv_obj_set_style_text_align(lbl_duration, LV_TEXT_ALIGN_CENTER,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_pos(lbl_duration, 20, 130);

    /* ── Delta vs normale (y=170) ── */
    lbl_delta = lv_label_create(scr);
    lv_label_set_text(lbl_delta, "");
    lv_obj_set_style_text_color(lbl_delta, lv_color_hex(0xaaaaaa),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_delta, &lv_font_montserrat_16,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_width(lbl_delta, 440);
    lv_obj_set_style_text_align(lbl_delta, LV_TEXT_ALIGN_CENTER,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_pos(lbl_delta, 20, 170);

    /* ── Distanza (y=215) ── */
    lbl_distance = lv_label_create(scr);
    lv_label_set_text(lbl_distance, "");
    lv_obj_set_style_text_color(lbl_distance, lv_color_hex(0xaaaaaa),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_distance, &lv_font_montserrat_14,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_width(lbl_distance, 440);
    lv_obj_set_style_text_align(lbl_distance, LV_TEXT_ALIGN_CENTER,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_pos(lbl_distance, 20, 215);

    /* ── Separatore centrale (y=255) ── */
    lv_obj_t *sep = lv_obj_create(scr);
    lv_obj_set_size(sep, 440, 1);
    lv_obj_set_pos(sep, 20, 255);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0x2a2a3a), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(sep, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

    /* ── Label "Configure via proxy" — posizione y=275 ── */
    lv_obj_t *lbl_route = lv_label_create(scr);
    lv_label_set_text(lbl_route, "Configure route via proxy Web UI");
    lv_obj_set_style_text_color(lbl_route, lv_color_hex(0xaaaaaa),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_route, &lv_font_montserrat_14,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_width(lbl_route, 440);
    lv_obj_set_style_text_align(lbl_route, LV_TEXT_ALIGN_CENTER,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_long_mode(lbl_route, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(lbl_route, 20, 275);

    /* ── Label "Aggiornato X min fa" — bottom_mid, bottom a y=450 ── */
    lbl_updated = lv_label_create(scr);
    lv_label_set_text(lbl_updated, "No data");
    lv_obj_set_style_text_color(lbl_updated, lv_color_hex(0x555555),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_updated, &lv_font_montserrat_12,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(lbl_updated, LV_ALIGN_BOTTOM_MID, 0, -30);

    /* ── Label errore — bottom_mid, sotto updated ── */
    lbl_error = lv_label_create(scr);
    lv_label_set_text(lbl_error, "Configure proxy: /traffic");
    lv_obj_set_style_text_color(lbl_error, lv_color_hex(0xe07070),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_error, &lv_font_montserrat_12,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(lbl_error, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_width(lbl_error, 440);
    lv_obj_set_style_text_align(lbl_error, LV_TEXT_ALIGN_CENTER,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_long_mode(lbl_error, LV_LABEL_LONG_WRAP);

    /* ── Timer refresh ogni 30s ── */
    s_refresh_timer = lv_timer_create(traffic_refresh_cb, 30000, NULL);

    s_populated = true;

    /* Prima populate immediata */
    screen_traffic_update();
}

/* ─── Init (lightweight — solo schermo + event handler) ─────────────────── */

void screen_traffic_init(void)
{
    ui_screen_traffic = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_screen_traffic, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_screen_traffic, lv_color_hex(0x0a0f1a),
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_screen_traffic, on_screen_load_start,
                        LV_EVENT_ALL, NULL);
}

/* ─── Getter ─────────────────────────────────────────────────────────────── */

lv_obj_t *screen_traffic_get_screen(void)
{
    return ui_screen_traffic;
}
