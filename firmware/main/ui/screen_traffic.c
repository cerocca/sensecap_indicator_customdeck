#include "screen_traffic.h"
#include "indicator_traffic.h"
#include "app_config.h"
#include "lvgl/lvgl.h"
#include "esp_timer.h"
#include "esp_log.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "SCR_TRAFFIC";

/* ─── Screen object ──────────────────────────────────────────────────────── */

lv_obj_t *ui_screen_traffic;

/* ─── Widget state ───────────────────────────────────────────────────────── */

static bool        s_populated           = false;
static bool        s_first_update_done   = false;
static lv_timer_t *s_refresh_timer       = NULL;

/* Common widgets (always visible) */
static lv_obj_t *lbl_updated;
static lv_obj_t *lbl_error;
static lv_obj_t *lbl_configure;

/* Single-route layout (route_count <= 1) */
static lv_obj_t *s1_name;
static lv_obj_t *s1_status;
static lv_obj_t *s1_duration;
static lv_obj_t *s1_delta;
static lv_obj_t *s1_distance;
static lv_obj_t *s1_sep;

/* Dual-route layout (route_count == 2) */
static lv_obj_t *s2_name[2];
static lv_obj_t *s2_status[2];
static lv_obj_t *s2_duration[2];
static lv_obj_t *s2_delta[2];
static lv_obj_t *s2_distance[2];
static lv_obj_t *s2_sep;

/* ─── Helpers ────────────────────────────────────────────────────────────── */

static lv_obj_t *make_label(lv_obj_t *scr,
                             int x, int y, int w,
                             const lv_font_t *font, lv_color_t color,
                             const char *text, bool hidden)
{
    lv_obj_t *lbl = lv_label_create(scr);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl,  font,  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_width(lbl, w);
    lv_obj_set_pos(lbl, x, y);
    if (hidden) lv_obj_add_flag(lbl, LV_OBJ_FLAG_HIDDEN);
    return lbl;
}

static lv_obj_t *make_sep(lv_obj_t *scr, int y, bool hidden)
{
    lv_obj_t *sep = lv_obj_create(scr);
    lv_obj_set_size(sep, 440, 1);
    lv_obj_set_pos(sep, 20, y);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0x2a2a3a),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(sep, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);
    if (hidden) lv_obj_add_flag(sep, LV_OBJ_FLAG_HIDDEN);
    return sep;
}

/* Aggiorna un set di 5 widget route da traffic_route_t */
static void fill_route(lv_obj_t *wname,  lv_obj_t *wstatus,
                        lv_obj_t *wdur,   lv_obj_t *wdelta,
                        lv_obj_t *wdist,  const traffic_route_t *r)
{
    char buf[64];

    /* Nome route */
    lv_label_set_text(wname, r->name[0] ? r->name : "Route");

    /* Stato — no ● */
    lv_color_t sc;
    const char *st;
    if (strcmp(r->status, "ok") == 0) {
        sc = lv_color_hex(0x4caf50);
        st = "OK";
    } else if (strcmp(r->status, "slow") == 0) {
        sc = lv_color_hex(0xff9800);
        st = "SLOW";
    } else {
        sc = lv_color_hex(0xf44336);
        st = "HEAVY";
    }
    lv_label_set_text(wstatus, st);
    lv_obj_set_style_text_color(wstatus, sc, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* Durata */
    snprintf(buf, sizeof(buf), "%d min", r->duration_sec / 60);
    lv_label_set_text(wdur, buf);

    /* Delta */
    int dm = r->delta_sec / 60;
    if (dm <= 0) {
        lv_label_set_text(wdelta, "On time");
        lv_obj_set_style_text_color(wdelta, lv_color_hex(0x4caf50),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        snprintf(buf, sizeof(buf), "+%d min vs normal", dm);
        lv_label_set_text(wdelta, buf);
        lv_obj_set_style_text_color(wdelta, sc, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    /* Distanza */
    if (r->distance_m >= 1000)
        snprintf(buf, sizeof(buf), "%.1f km", (float)r->distance_m / 1000.0f);
    else
        snprintf(buf, sizeof(buf), "%d m", r->distance_m);
    lv_label_set_text(wdist, buf);
}

static void show_single_layout(bool show)
{
    if (show) {
        lv_obj_clear_flag(s1_name,     LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s1_status,   LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s1_duration, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s1_delta,    LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s1_distance, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s1_sep,      LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s1_name,     LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s1_status,   LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s1_duration, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s1_delta,    LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s1_distance, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s1_sep,      LV_OBJ_FLAG_HIDDEN);
    }
}

static void show_dual_layout(bool show)
{
    if (show) {
        lv_obj_clear_flag(s2_sep, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < 2; i++) {
            lv_obj_clear_flag(s2_name[i],     LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s2_status[i],   LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s2_duration[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s2_delta[i],    LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s2_distance[i], LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        lv_obj_add_flag(s2_sep, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < 2; i++) {
            lv_obj_add_flag(s2_name[i],     LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s2_status[i],   LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s2_duration[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s2_delta[i],    LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s2_distance[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/* ─── UI update from g_traffic ───────────────────────────────────────────── */

void screen_traffic_update(void)
{
    if (!s_populated) return;

    char buf[64];

    /* Timestamp "Updated X min ago" */
    if (g_traffic.valid) {
        int64_t now_ms  = esp_timer_get_time() / 1000LL;
        int     min_ago = (int)((now_ms - g_traffic.last_update_ms) / 60000LL);
        if (min_ago < 1)
            snprintf(buf, sizeof(buf), "Updated just now");
        else if (min_ago < 60)
            snprintf(buf, sizeof(buf), "Updated %d min ago", min_ago);
        else
            snprintf(buf, sizeof(buf), "Updated %dh ago", min_ago / 60);
        lv_label_set_text(lbl_updated, buf);
    } else {
        lv_label_set_text(lbl_updated, "No data");
    }

    /* Nessun dato — layout singolo con placeholder */
    if (!g_traffic.valid) {
        show_dual_layout(false);
        show_single_layout(true);

        lv_label_set_text(s1_name,   "");
        lv_label_set_text(s1_status, "NO DATA");
        lv_obj_set_style_text_color(s1_status, lv_color_hex(0x888888),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(s1_duration, "--");
        lv_label_set_text(s1_delta,    "");
        lv_label_set_text(s1_distance, "");

        lv_obj_clear_flag(lbl_configure, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_error, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    /* Dati validi — nascondi configure e error */
    lv_obj_add_flag(lbl_configure, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_error,     LV_OBJ_FLAG_HIDDEN);

    if (g_traffic.route_count == 2) {
        /* ── Layout doppio ── */
        show_single_layout(false);
        show_dual_layout(true);
        fill_route(s2_name[0], s2_status[0], s2_duration[0],
                   s2_delta[0], s2_distance[0], &g_traffic.routes[0]);
        fill_route(s2_name[1], s2_status[1], s2_duration[1],
                   s2_delta[1], s2_distance[1], &g_traffic.routes[1]);
    } else {
        /* ── Layout singolo (route_count == 1) ── */
        show_dual_layout(false);
        show_single_layout(true);
        fill_route(s1_name, s1_status, s1_duration,
                   s1_delta, s1_distance, &g_traffic.routes[0]);
    }
}

/* ─── Refresh timer callback (ogni 30s) ─────────────────────────────────── */

static void traffic_refresh_cb(lv_timer_t *timer)
{
    if (lv_scr_act() != ui_screen_traffic) return;
    if (!s_populated) return;
    screen_traffic_update();
}

/* ─── Recurring timer: attende g_traffic.valid, poi fa il primo update ──── */

static void traffic_wait_data_cb(lv_timer_t *t)
{
    if (g_traffic.valid && !s_first_update_done) {
        s_first_update_done = true;
        screen_traffic_update();
        lv_timer_del(t);
    }
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

#define LBL(out, x, y, w, font, color, text, hidden) \
    (out) = make_label(scr, (x), (y), (w), (font), lv_color_hex(color), (text), (hidden))

    /* ── Header ── */
    lv_obj_t *hdr = lv_label_create(scr);
    lv_label_set_text(hdr, "Traffic");
    lv_obj_set_style_text_color(hdr, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(hdr, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 10);

    /* ── Separatore header (y=55) ── */
    make_sep(scr, 55, false);

    /* ── Single-route layout — nome font20 (sottolineato) > stato font16 ───── */

    LBL(s1_name,     20, 110, 440, &lv_font_montserrat_20, 0xaaaaaa, "",    false);
    lv_obj_set_style_text_decor(s1_name, LV_TEXT_DECOR_UNDERLINE,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    LBL(s1_status,   20, 155, 440, &lv_font_montserrat_16, 0x888888, "--",  false);
    LBL(s1_duration, 20, 200, 440, &lv_font_montserrat_20, 0xffffff, "--",  false);
    LBL(s1_delta,    20, 245, 440, &lv_font_montserrat_16, 0xaaaaaa, "",    false);
    LBL(s1_distance, 20, 285, 440, &lv_font_montserrat_14, 0xaaaaaa, "",    false);
    s1_sep = make_sep(scr, 320, false);

    /* "Configure via proxy" — visibile solo quando !valid */
    LBL(lbl_configure, 20, 345, 440, &lv_font_montserrat_14, 0xaaaaaa,
        "Configure route via proxy Web UI", false);
    lv_label_set_long_mode(lbl_configure, LV_LABEL_LONG_DOT);

    /* ── Dual-route layout (hidden al populate) ───────────────────────────── */
    /* nome font20 sottolineato > stato font16; duration font18 > font16;
     * y ricalcolate per respiro dopo header e font duration più grande */

    /* Route 0 — top half */
    LBL(s2_name[0],     20,  72, 440, &lv_font_montserrat_20, 0xaaaaaa, "", true);
    lv_obj_set_style_text_decor(s2_name[0], LV_TEXT_DECOR_UNDERLINE,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    LBL(s2_status[0],   20, 104, 440, &lv_font_montserrat_16, 0x888888, "", true);
    LBL(s2_duration[0], 20, 132, 440, &lv_font_montserrat_18, 0xffffff, "", true);
    LBL(s2_delta[0],    20, 162, 440, &lv_font_montserrat_14, 0xaaaaaa, "", true);
    LBL(s2_distance[0], 20, 186, 440, &lv_font_montserrat_12, 0xaaaaaa, "", true);

    /* Separatore centrale (y=212) */
    s2_sep = make_sep(scr, 212, true);

    /* Route 1 — bottom half */
    LBL(s2_name[1],     20, 224, 440, &lv_font_montserrat_20, 0xaaaaaa, "", true);
    lv_obj_set_style_text_decor(s2_name[1], LV_TEXT_DECOR_UNDERLINE,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    LBL(s2_status[1],   20, 256, 440, &lv_font_montserrat_16, 0x888888, "", true);
    LBL(s2_duration[1], 20, 284, 440, &lv_font_montserrat_18, 0xffffff, "", true);
    LBL(s2_delta[1],    20, 314, 440, &lv_font_montserrat_14, 0xaaaaaa, "", true);
    LBL(s2_distance[1], 20, 338, 440, &lv_font_montserrat_12, 0xaaaaaa, "", true);

#undef LBL

    /* ── Bottom labels ── */
    lbl_updated = lv_label_create(scr);
    lv_label_set_text(lbl_updated, "No data");
    lv_obj_set_style_text_color(lbl_updated, lv_color_hex(0x555555),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_updated, &lv_font_montserrat_12,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(lbl_updated, LV_ALIGN_BOTTOM_MID, 0, -50);

    lbl_error = lv_label_create(scr);
    lv_label_set_text(lbl_error, "");
    lv_obj_set_style_text_color(lbl_error, lv_color_hex(0xe07070),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_error, &lv_font_montserrat_12,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_width(lbl_error, 440);
    lv_obj_set_style_text_align(lbl_error, LV_TEXT_ALIGN_CENTER,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_long_mode(lbl_error, LV_LABEL_LONG_WRAP);
    lv_obj_align(lbl_error, LV_ALIGN_BOTTOM_MID, 0, -25);
    lv_obj_add_flag(lbl_error, LV_OBJ_FLAG_HIDDEN);

    /* ── Timer refresh ogni 30s ── */
    s_refresh_timer = lv_timer_create(traffic_refresh_cb, 30000, NULL);

    s_populated = true;

    ESP_LOGI(TAG, "populate done: valid=%d route_count=%d",
             g_traffic.valid, g_traffic.route_count);

    /* Recurring 1000ms: controlla g_traffic.valid ogni secondo, chiama
     * screen_traffic_update() alla prima occorrenza, poi si auto-cancella.
     * populate() è chiamata prima che l'animazione avvii (schermo non visibile);
     * attendere valid==true evita schermata nera al primo swipe. */
    s_first_update_done = false;
    lv_timer_create(traffic_wait_data_cb, 1000, NULL);
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
