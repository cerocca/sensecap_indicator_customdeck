#include "screen_hue.h"
#include "indicator_hue.h"
#include "lvgl/lvgl.h"
#include "esp_log.h"

static const char *TAG = "screen_hue";

lv_obj_t *ui_screen_hue;

/* Widget references — validi solo dopo screen_hue_populate() */
static lv_obj_t *s_sw[HUE_LIGHT_COUNT];
static lv_obj_t *s_slider[HUE_LIGHT_COUNT];
static lv_obj_t *s_name_lbl[HUE_LIGHT_COUNT];

/* ─── UI update callback ────────────────────────────────────────────────────
 * Chiamata dal poll task (con LVGL sem già acquisito) dopo ogni risposta.
 * Aggiorna switch e slider allo stato letto dal bridge.
 * ─────────────────────────────────────────────────────────────────────────*/
static void hue_update_ui(void)
{
    for (int i = 0; i < HUE_LIGHT_COUNT; i++) {
        hue_light_t light;
        indicator_hue_get_light(i, &light);

        /* Stato switch */
        if (light.on)
            lv_obj_add_state(s_sw[i], LV_STATE_CHECKED);
        else
            lv_obj_clear_state(s_sw[i], LV_STATE_CHECKED);

        /* Valore slider */
        int bri = (int)light.brightness;
        if (bri < 1) bri = 1;
        lv_slider_set_value(s_slider[i], bri, LV_ANIM_OFF);

        /* Nome luce (può essere aggiornato via proxy config reload) */
        lv_label_set_text(s_name_lbl[i], light.name);
    }
}

/* ─── Switch event ──────────────────────────────────────────────────────── */

static void switch_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    indicator_hue_toggle(idx);
}

/* ─── Slider event ──────────────────────────────────────────────────────── */

static void slider_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_RELEASED) return;
    lv_obj_t *slider = lv_event_get_target(e);
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    indicator_hue_set_brightness(idx, (float)lv_slider_get_value(slider));
}

/* ─── Lazy populate ─────────────────────────────────────────────────────── */

/*
 * Layout 480x480:
 *   y=10  Titolo "Hue Control"
 *   y=50  Card 1   (h=98)
 *   y=155 Card 2
 *   y=260 Card 3
 *   y=365 Card 4   (bottom=463)
 *
 * Ogni card (460x98):
 *   top-left  Nome luce    (font_montserrat_16, bianco)
 *   top-right Switch ON/OFF (52x26, giallo ambra when on)
 *   bottom    Slider 1-100 (w=420, giallo ambra)
 */
void screen_hue_populate(void)
{
    /* Titolo */
    lv_obj_t *title = lv_label_create(ui_screen_hue);
    lv_label_set_text(title, "Hue Control");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    /* 4 card luci */
    for (int i = 0; i < HUE_LIGHT_COUNT; i++) {
        hue_light_t light;
        indicator_hue_get_light(i, &light);

        /* ── Card container ── */
        lv_obj_t *card = lv_obj_create(ui_screen_hue);
        lv_obj_set_size(card, 460, 98);
        lv_obj_set_pos(card, 10, 50 + i * 105);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x16213e), 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0x2d3561), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_radius(card, 10, 0);
        lv_obj_set_style_pad_all(card, 0, 0);

        /* ── Nome luce ── */
        s_name_lbl[i] = lv_label_create(card);
        lv_label_set_text(s_name_lbl[i], light.name);
        lv_obj_set_style_text_color(s_name_lbl[i], lv_color_white(), 0);
        lv_obj_set_style_text_font(s_name_lbl[i], &lv_font_montserrat_16, 0);
        lv_obj_set_pos(s_name_lbl[i], 10, 10);

        /* ── Switch ON/OFF ── */
        s_sw[i] = lv_switch_create(card);
        lv_obj_set_size(s_sw[i], 52, 26);
        lv_obj_align(s_sw[i], LV_ALIGN_TOP_RIGHT, -10, 8);
        if (light.on)
            lv_obj_add_state(s_sw[i], LV_STATE_CHECKED);
        /* Colore ambra quando acceso */
        lv_obj_set_style_bg_color(s_sw[i], lv_color_hex(0xf5a623),
                                   LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_add_event_cb(s_sw[i], switch_cb, LV_EVENT_VALUE_CHANGED,
                             (void *)(intptr_t)i);

        /* ── Slider luminosità ── */
        s_slider[i] = lv_slider_create(card);
        lv_obj_set_size(s_slider[i], 420, 16);
        lv_obj_align(s_slider[i], LV_ALIGN_BOTTOM_MID, 0, -10);
        lv_slider_set_range(s_slider[i], 1, 100);
        int bri = (int)light.brightness;
        lv_slider_set_value(s_slider[i], bri > 0 ? bri : 1, LV_ANIM_OFF);
        /* Colore ambra per indicatore e knob */
        lv_obj_set_style_bg_color(s_slider[i], lv_color_hex(0xf5a623),
                                   LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(s_slider[i], lv_color_hex(0xf5a623),
                                   LV_PART_KNOB | LV_STATE_DEFAULT);
        lv_obj_add_event_cb(s_slider[i], slider_cb, LV_EVENT_RELEASED,
                             (void *)(intptr_t)i);
    }

    /* Registra callback per aggiornamenti dal poll task */
    indicator_hue_set_update_cb(hue_update_ui);

    ESP_LOGI(TAG, "screen_hue_populate: completato");
}

/* ─── Lightweight init (al boot) ───────────────────────────────────────── */

void screen_hue_init(void)
{
    ui_screen_hue = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_screen_hue, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_screen_hue, lv_color_hex(0x1a1a2e),
                               LV_PART_MAIN | LV_STATE_DEFAULT);
}
