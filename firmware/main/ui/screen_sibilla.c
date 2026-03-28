#include "screen_sibilla.h"
#include "indicator_glances.h"
#include "indicator_uptime_kuma.h"
#include "indicator_storage.h"
#include "app_config.h"
#include "lvgl/lvgl.h"

#include <string.h>
#include <stdio.h>
#include <inttypes.h>

/* ─── Screen object ───────────────────────────────────────────────────────── */

lv_obj_t *ui_screen_sibilla;

/* ─── Widget handles (validi solo dopo populate) ─────────────────────────── */

static lv_obj_t *s_header_lbl;

static lv_obj_t *s_cpu_bar,  *s_cpu_pct_lbl;
static lv_obj_t *s_ram_bar,  *s_ram_pct_lbl;
static lv_obj_t *s_dsk_bar,  *s_dsk_pct_lbl;

static lv_obj_t *s_uptime_lbl;
static lv_obj_t *s_load_lbl;

static lv_obj_t *s_services_lbl;
static lv_obj_t *s_down_lbls[6];

static lv_obj_t *s_topram_hdr_lbl;
static lv_obj_t *s_topram_name_lbls[3];
static lv_obj_t *s_topram_mb_lbls[3];

static bool s_populated = false;

/* ─── Helpers ─────────────────────────────────────────────────────────────── */

static void refresh_header(void)
{
    char name[32];
    size_t len = sizeof(name);
    if (indicator_storage_read((char *)NVS_KEY_SERVER_NAME, name, &len) == 0
        && len > 1) {
        name[sizeof(name) - 1] = '\0';
    } else {
        strncpy(name, APP_CFG_SERVER_NAME, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
    }
    lv_label_set_text(s_header_lbl, name);
}

/*
 * Converte la stringa uptime di Glances (es. "1 day, 2:34:56" oppure "2:34:56")
 * nel formato compatto "Up: 1d 2h 34m".
 */
static void format_uptime(const char *raw, char *buf, size_t buf_size)
{
    int days = 0, h = 0, m = 0, s = 0;
    if (strstr(raw, "day")) {
        sscanf(raw, "%d day", &days);
        const char *tp = strchr(raw, ',');
        if (tp) sscanf(tp + 2, "%d:%d:%d", &h, &m, &s);
    } else {
        sscanf(raw, "%d:%d:%d", &h, &m, &s);
    }
    if (days > 0)
        snprintf(buf, buf_size, "Up: %dd %dh %dm", days, h, m);
    else if (h > 0)
        snprintf(buf, buf_size, "Up: %dh %dm", h, m);
    else
        snprintf(buf, buf_size, "Up: %dm", m);
}

/*
 * Crea una riga metrica: label nome (sinistra) + lv_bar (centro) + label % (destra).
 *
 *  x=10  nome (35px)
 *  x=55  bar  (345×16px)
 *  x=408 percentuale
 */
static void make_metric_row(lv_obj_t *parent, int y, const char *name,
                             lv_obj_t **bar_out, lv_obj_t **pct_out)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, name);
    lv_obj_set_style_text_color(lbl, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_pos(lbl, 10, y);

    lv_obj_t *bar = lv_bar_create(parent);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_obj_set_size(bar, 345, 16);
    lv_obj_set_pos(bar, 55, y + 2);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x2a3f5f),
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER,
                             LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x7ec8a0),
                               LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER,
                             LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(bar, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(bar, 4, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    *bar_out = bar;

    lv_obj_t *pct = lv_label_create(parent);
    lv_label_set_text(pct, "--");
    lv_obj_set_style_text_color(pct, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(pct, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_pos(pct, 408, y);
    *pct_out = pct;
}

/* ─── Glances callback (chiamata con LVGL sem già acquisito) ──────────────── */

static void on_glances_data(float cpu, float ram, float dsk,
                             const char *uptime,
                             float load1, float load5, float load15,
                             const glances_proc_t top_ram[3])
{
    if (!s_populated) return;

    char buf[48];

    /* CPU */
    if (cpu >= 0.0f) {
        lv_bar_set_value(s_cpu_bar, (int)cpu, LV_ANIM_OFF);
        snprintf(buf, sizeof(buf), "%.0f%%", cpu);
        lv_label_set_text(s_cpu_pct_lbl, buf);
    }

    /* RAM */
    if (ram >= 0.0f) {
        lv_bar_set_value(s_ram_bar, (int)ram, LV_ANIM_OFF);
        snprintf(buf, sizeof(buf), "%.0f%%", ram);
        lv_label_set_text(s_ram_pct_lbl, buf);
    }

    /* Disco */
    if (dsk >= 0.0f) {
        lv_bar_set_value(s_dsk_bar, (int)dsk, LV_ANIM_OFF);
        snprintf(buf, sizeof(buf), "%.0f%%", dsk);
        lv_label_set_text(s_dsk_pct_lbl, buf);
    }

    /* Uptime */
    if (uptime && uptime[0] != '\0') {
        char up_buf[40];
        format_uptime(uptime, up_buf, sizeof(up_buf));
        lv_label_set_text(s_uptime_lbl, up_buf);
    }

    /* Load avg */
    if (load1 >= 0.0f) {
        snprintf(buf, sizeof(buf), "Load: 1min %.2f | 5min %.2f | 15min %.2f",
                 load1, load5, load15);
        lv_label_set_text(s_load_lbl, buf);
    }

    /* Top Docker — reset prima di sovrascrivere, così gli slot vuoti si puliscono */
    for (int i = 0; i < 3; i++) {
        lv_label_set_text(s_topram_name_lbls[i], "--");
        lv_label_set_text(s_topram_mb_lbls[i],   "");
    }
    for (int i = 0; i < 3; i++) {
        if (top_ram[i].name[0] != '\0') {
            lv_label_set_text(s_topram_name_lbls[i], top_ram[i].name);
            snprintf(buf, sizeof(buf), "%"PRIu32" MB", top_ram[i].mem_mb);
            lv_label_set_text(s_topram_mb_lbls[i], buf);
        }
    }
}

/* ─── Uptime Kuma callback (chiamata con LVGL sem già acquisito) ──────────── */

static void on_uptime_data(int total, int up,
                            const char names_down[UK_MAX_DOWN][32],
                            int n_down)
{
    if (!s_populated) return;

    char buf[48];

    /* Label "Monitored Services: X/Y UP" */
    snprintf(buf, sizeof(buf), "Monitored Services: %d/%d UP", up, total);
    lv_label_set_text(s_services_lbl, buf);
    lv_color_t col = (n_down == 0)
                     ? lv_color_hex(0x7ec8a0)   /* verde = tutti UP */
                     : lv_color_hex(0xe07070);   /* rosso = qualcuno DOWN */
    lv_obj_set_style_text_color(s_services_lbl, col,
                                 LV_PART_MAIN | LV_STATE_DEFAULT);

    /* Righe DOWN: mostra quelle usate, nasconde le altre */
    for (int i = 0; i < 6; i++) {
        if (i < n_down) {
            lv_label_set_text(s_down_lbls[i], names_down[i]);
            lv_obj_clear_flag(s_down_lbls[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_label_set_text(s_down_lbls[i], "");
            lv_obj_add_flag(s_down_lbls[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/* ─── Populate (lazy — prima del primo render) ────────────────────────────── */

void screen_sibilla_populate(void)
{
    lv_obj_t *scr = ui_screen_sibilla;

    /* ── Righe metriche Glances ── */
    make_metric_row(scr,  65, "CPU", &s_cpu_bar, &s_cpu_pct_lbl);
    make_metric_row(scr, 105, "RAM", &s_ram_bar, &s_ram_pct_lbl);
    make_metric_row(scr, 145, "DSK", &s_dsk_bar, &s_dsk_pct_lbl);

    /* ── Uptime ── */
    s_uptime_lbl = lv_label_create(scr);
    lv_label_set_text(s_uptime_lbl, "Up: --");
    lv_obj_set_style_text_color(s_uptime_lbl, lv_color_white(),
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(s_uptime_lbl, &lv_font_montserrat_16,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_pos(s_uptime_lbl, 10, 185);

    /* ── Load avg ── */
    s_load_lbl = lv_label_create(scr);
    lv_label_set_text(s_load_lbl, "Load: 1min -- | 5min -- | 15min --");
    lv_obj_set_style_text_color(s_load_lbl, lv_color_white(),
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(s_load_lbl, &lv_font_montserrat_16,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_pos(s_load_lbl, 10, 215);

    /* ── Separatore orizzontale (y=242, larghezza 440px) ── */
    static lv_point_t sep_pts[2] = {{0, 0}, {440, 0}};
    lv_obj_t *sep = lv_line_create(scr);
    lv_line_set_points(sep, sep_pts, 2);
    lv_obj_set_pos(sep, 20, 242);
    lv_obj_set_style_line_color(sep, lv_color_hex(0x2a3f5f), 0);
    lv_obj_set_style_line_width(sep, 2, 0);

    /* ── Header Top Docker (y=254, subito sotto il separatore) ── */
    s_topram_hdr_lbl = lv_label_create(scr);
    lv_label_set_text(s_topram_hdr_lbl, "Top Docker (RAM):");
    lv_obj_set_style_text_color(s_topram_hdr_lbl, lv_color_hex(0x7ec8a0),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(s_topram_hdr_lbl, &lv_font_montserrat_16,
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_pos(s_topram_hdr_lbl, 10, 254);

    /* ── 3 righe container Docker: nome (sx, max 32 char) + MB (dx allineato) ── */
    for (int i = 0; i < 3; i++) {
        int y = 276 + i * 22;

        s_topram_name_lbls[i] = lv_label_create(scr);
        lv_label_set_text(s_topram_name_lbls[i], "--");
        lv_obj_set_style_text_color(s_topram_name_lbls[i], lv_color_hex(0xb0c8e0),
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(s_topram_name_lbls[i], &lv_font_montserrat_16,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_long_mode(s_topram_name_lbls[i], LV_LABEL_LONG_DOT);
        lv_obj_set_width(s_topram_name_lbls[i], 330);
        lv_obj_set_pos(s_topram_name_lbls[i], 10, y);

        s_topram_mb_lbls[i] = lv_label_create(scr);
        lv_label_set_text(s_topram_mb_lbls[i], "--");
        lv_obj_set_style_text_color(s_topram_mb_lbls[i], lv_color_hex(0xb0c8e0),
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(s_topram_mb_lbls[i], &lv_font_montserrat_16,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(s_topram_mb_lbls[i], LV_TEXT_ALIGN_RIGHT,
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_width(s_topram_mb_lbls[i], 100);
        lv_obj_set_pos(s_topram_mb_lbls[i], 350, y);
    }

    /* ── Label servizi Uptime Kuma (placeholder) — 2 righe sotto il terzo container ── */
    s_services_lbl = lv_label_create(scr);
    lv_label_set_text(s_services_lbl, "Monitored Services: --/-- UP");
    lv_obj_set_style_text_color(s_services_lbl, lv_color_white(),
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(s_services_lbl, &lv_font_montserrat_16,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_pos(s_services_lbl, 10, 366);

    /* ── 6 label DOWN pre-allocate (nascoste) — y=388+i*18 ── */
    for (int i = 0; i < 6; i++) {
        s_down_lbls[i] = lv_label_create(scr);
        lv_label_set_text(s_down_lbls[i], "");
        lv_obj_set_style_text_color(s_down_lbls[i], lv_color_hex(0xe07070),
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(s_down_lbls[i], &lv_font_montserrat_14,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_pos(s_down_lbls[i], 10, 388 + i * 16);
        lv_obj_add_flag(s_down_lbls[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* Registra le callback: Glances e Uptime Kuma aggiornano i widget */
    indicator_glances_set_callback(on_glances_data);
    indicator_uptime_kuma_set_callback(on_uptime_data);

    s_populated = true;
}

/* ─── Event handler ───────────────────────────────────────────────────────── */

static void on_screen_load_start(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_SCREEN_LOAD_START) return;
    if (!s_populated)
        screen_sibilla_populate();
    refresh_header();
}

/* ─── Init (lightweight — solo schermo + header) ─────────────────────────── */

void screen_sibilla_init(void)
{
    ui_screen_sibilla = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_screen_sibilla, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_screen_sibilla, lv_color_hex(0x0d1b2a),
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    /* Header: nome server — centrato verticalmente nei 55px superiori */
    s_header_lbl = lv_label_create(ui_screen_sibilla);
    lv_label_set_text(s_header_lbl, APP_CFG_SERVER_NAME);
    lv_obj_set_style_text_color(s_header_lbl, lv_color_white(),
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(s_header_lbl, &lv_font_montserrat_20,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(s_header_lbl, LV_ALIGN_TOP_MID, 0, 10);

    /* Lazy populate + refresh header nome al caricamento schermata */
    lv_obj_add_event_cb(ui_screen_sibilla, on_screen_load_start,
                        LV_EVENT_ALL, NULL);
}
