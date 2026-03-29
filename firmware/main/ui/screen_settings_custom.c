#include "screen_settings_custom.h"
#include "ui_helpers.h"           /* _ui_screen_change */
#include "ui_manager.h"           /* g_scr_xxx_enabled */
#include "indicator_storage.h"
#include "indicator_config.h"     /* config_fetch_from_proxy */
#include "app_config.h"
#include "lv_port.h"              /* lv_port_sem_take/give */
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "settings_custom";

/* ─── screen root ───────────────────────────────────────────── */
lv_obj_t *ui_screen_settings_custom = NULL;

/* ─── keyboard (shared, one at a time) ─────────────────────── */
static lv_obj_t *s_kb = NULL;

/* ─── Hue tab textareas ─────────────────────────────────────── */
static lv_obj_t *ta_hue_ip;
static lv_obj_t *ta_hue_key;
static lv_obj_t *ta_hue_l1, *ta_hue_l2, *ta_hue_l3, *ta_hue_l4;

/* ─── Server tab textareas ──────────────────────────────────── */
static lv_obj_t *ta_srv_ip;
static lv_obj_t *ta_srv_port;
static lv_obj_t *ta_srv_name;
static lv_obj_t *ta_beszel_port;
static lv_obj_t *ta_uk_port;

/* ─── Proxy tab textareas + feedback labels ──────────────────── */
static lv_obj_t *ta_proxy_ip;
static lv_obj_t *ta_proxy_port;
static lv_obj_t *lbl_cfg_status;
static lv_obj_t *lbl_proxy_url;  /* Config UI URL (dinamico) */

/* ─── Weather tab ───────────────────────────────────────────── */
static lv_obj_t *lbl_meteo_url;   /* URL proxy Web UI (dinamico) */

/* ─── Traffic tab ────────────────────────────────────────────── */
static lv_obj_t *sw_traffic;
static lv_obj_t *lbl_traffic_url;  /* URL proxy Web UI (dinamico) */

/* ─── Screens tab switches ───────────────────────────────────── */
static lv_obj_t *sw_defsens;
static lv_obj_t *sw_hue;
static lv_obj_t *sw_srv;
static lv_obj_t *sw_lnch;
static lv_obj_t *sw_wthr;

/***********************************************************
 * helpers
 ***********************************************************/

/* Read string from NVS into textarea; on failure, use fallback. */
static void nvs_read_str(const char *key, lv_obj_t *ta, const char *fallback)
{
    char buf[128];
    size_t len = sizeof(buf);
    if (indicator_storage_read((char *)key, buf, &len) == 0 && len > 1) {
        lv_textarea_set_text(ta, buf);
    } else {
        lv_textarea_set_text(ta, fallback);
    }
}

/* Read string from NVS into a char buffer. */
static void nvs_read_str_buf(const char *key, char *buf, size_t len,
                              const char *fallback)
{
    size_t sz = len;
    if (indicator_storage_read((char *)key, buf, &sz) == 0 && sz > 1) {
        buf[len - 1] = '\0';
    } else {
        strncpy(buf, fallback, len - 1);
        buf[len - 1] = '\0';
    }
}

/* Write string from textarea to NVS. */
static void nvs_write_ta(const char *key, lv_obj_t *ta)
{
    const char *v = lv_textarea_get_text(ta);
    if (v) {
        indicator_storage_write((char *)key, (void *)v, strlen(v) + 1);
    }
}

/* Styled single-line textarea. */
static lv_obj_t *make_ta(lv_obj_t *parent, int y, const char *placeholder)
{
    lv_obj_t *ta = lv_textarea_create(parent);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_placeholder_text(ta, placeholder);
    lv_obj_set_width(ta, 400);
    lv_obj_set_height(ta, 38);
    lv_obj_set_pos(ta, 40, y);
    lv_obj_set_style_bg_color(ta, lv_color_hex(0x2a2a2a), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    return ta;
}

/* Small label above a field. */
static void make_label(lv_obj_t *parent, int y, const char *text)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_pos(lbl, 40, y);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xaaaaaa), LV_PART_MAIN | LV_STATE_DEFAULT);
}

/* Save button. */
static lv_obj_t *make_save_btn(lv_obj_t *parent, int y, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 120, 40);
    lv_obj_set_pos(btn, 180, y);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x529d53), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "Save");
    lv_obj_center(lbl);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    return btn;
}

/***********************************************************
 * keyboard management
 ***********************************************************/

/* Tabview reference — saved at populate time, used for resize on kb open/close. */
static lv_obj_t *s_tv               = NULL;
static lv_obj_t *s_kb_tab_panel     = NULL;  /* tab panel active when kb opened  */
static bool      s_kb_tab_scrollable = false; /* was it scrollable before kb open */

static void kb_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        if (s_kb != NULL) {
            lv_obj_del(s_kb);
            s_kb = NULL;
        }
        /* Restore tabview to full screen height */
        if (s_tv) lv_obj_set_height(s_tv, 480);
        /* Restore tab scrollability and scroll back to top */
        if (s_kb_tab_panel) {
            if (!s_kb_tab_scrollable)
                lv_obj_clear_flag(s_kb_tab_panel, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_scroll_to_y(s_kb_tab_panel, 0, LV_ANIM_OFF);
            s_kb_tab_panel = NULL;
        }
    }
}

static void ta_focused_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    lv_obj_t *ta = lv_event_get_target(e);

    if (s_kb == NULL) {
        s_kb = lv_keyboard_create(ui_screen_settings_custom);
        lv_obj_add_event_cb(s_kb, kb_event_cb, LV_EVENT_ALL, NULL);
        lv_obj_set_style_text_font(s_kb, &lv_font_montserrat_14,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        /* Fix Bug 2: explicit pressed-key highlight (#4a90d9) */
        lv_obj_set_style_bg_color(s_kb, lv_color_hex(0x4a90d9),
                                  LV_PART_ITEMS | LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(s_kb, LV_OPA_COVER,
                                LV_PART_ITEMS | LV_STATE_PRESSED);
        /* Fix Bug 1: cap keyboard at 200px so tab content remains visible */
        lv_obj_set_height(s_kb, 200);
        /* Shrink tabview to 280px (480 - 200) so nothing hides behind keyboard */
        if (s_tv) lv_obj_set_height(s_tv, 280);
    }
    lv_keyboard_set_textarea(s_kb, ta);

    /* Temporarily enable scroll on the active tab and bring textarea into view */
    s_kb_tab_panel = lv_obj_get_parent(ta);
    s_kb_tab_scrollable = lv_obj_has_flag(s_kb_tab_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_kb_tab_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_scroll_to_view_recursive(ta, LV_ANIM_ON);
}

/***********************************************************
 * Hue tab
 ***********************************************************/

static void on_save_hue(lv_event_t *e)
{
    nvs_write_ta(NVS_KEY_HUE_IP,      ta_hue_ip);
    nvs_write_ta(NVS_KEY_HUE_API_KEY, ta_hue_key);
    nvs_write_ta(NVS_KEY_HUE_LIGHT_1, ta_hue_l1);
    nvs_write_ta(NVS_KEY_HUE_LIGHT_2, ta_hue_l2);
    nvs_write_ta(NVS_KEY_HUE_LIGHT_3, ta_hue_l3);
    nvs_write_ta(NVS_KEY_HUE_LIGHT_4, ta_hue_l4);
    ESP_LOGI(TAG, "Hue config saved");
}

static lv_obj_t *build_tab_hue(lv_obj_t *tabview)
{
    lv_obj_t *tab = lv_tabview_add_tab(tabview, "Hue");
    /* scrollable – 6 fields don't fit without scrolling */

    int y = 4;
    make_label(tab, y, "IP Bridge");      y += 18;
    ta_hue_ip  = make_ta(tab, y, "192.168.1.x"); lv_obj_add_event_cb(ta_hue_ip,  ta_focused_cb, LV_EVENT_CLICKED, NULL); y += 46;
    make_label(tab, y, "API Key");        y += 18;
    ta_hue_key = make_ta(tab, y, "");            lv_obj_add_event_cb(ta_hue_key, ta_focused_cb, LV_EVENT_CLICKED, NULL); y += 46;
    make_label(tab, y, "Light 1");        y += 18;
    ta_hue_l1  = make_ta(tab, y, "Light 1");    lv_obj_add_event_cb(ta_hue_l1,  ta_focused_cb, LV_EVENT_CLICKED, NULL); y += 46;
    make_label(tab, y, "Light 2");        y += 18;
    ta_hue_l2  = make_ta(tab, y, "Light 2");    lv_obj_add_event_cb(ta_hue_l2,  ta_focused_cb, LV_EVENT_CLICKED, NULL); y += 46;
    make_label(tab, y, "Light 3");        y += 18;
    ta_hue_l3  = make_ta(tab, y, "Light 3");    lv_obj_add_event_cb(ta_hue_l3,  ta_focused_cb, LV_EVENT_CLICKED, NULL); y += 46;
    make_label(tab, y, "Light 4");        y += 18;
    ta_hue_l4  = make_ta(tab, y, "Light 4");    lv_obj_add_event_cb(ta_hue_l4,  ta_focused_cb, LV_EVENT_CLICKED, NULL); y += 54;
    make_save_btn(tab, y, on_save_hue);

    return tab;
}

/***********************************************************
 * Server tab
 ***********************************************************/

static void on_save_server(lv_event_t *e)
{
    nvs_write_ta(NVS_KEY_SERVER_IP,   ta_srv_ip);
    nvs_write_ta(NVS_KEY_SERVER_PORT, ta_srv_port);
    nvs_write_ta(NVS_KEY_SERVER_NAME, ta_srv_name);
    nvs_write_ta(NVS_KEY_BESZEL_PORT, ta_beszel_port);
    nvs_write_ta(NVS_KEY_UK_PORT,     ta_uk_port);
    ESP_LOGI(TAG, "Server config saved");
}

static lv_obj_t *build_tab_server(lv_obj_t *tabview)
{
    lv_obj_t *tab = lv_tabview_add_tab(tabview, "Server");
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);

    int y = 20;
    make_label(tab, y, "Server IP");      y += 18;
    ta_srv_ip   = make_ta(tab, y, "192.168.1.x"); lv_obj_add_event_cb(ta_srv_ip,   ta_focused_cb, LV_EVENT_CLICKED, NULL); y += 54;
    make_label(tab, y, "Server Name");    y += 18;
    ta_srv_name = make_ta(tab, y, "LocalServer");  lv_obj_add_event_cb(ta_srv_name, ta_focused_cb, LV_EVENT_CLICKED, NULL);
    lv_textarea_set_max_length(ta_srv_name, 15);   y += 54;
    make_label(tab, y, "Glances Port");   y += 18;
    ta_srv_port = make_ta(tab, y, "61208");        lv_obj_add_event_cb(ta_srv_port, ta_focused_cb, LV_EVENT_CLICKED, NULL); y += 54;
    make_label(tab, y, "Beszel Port");    y += 18;
    ta_beszel_port = make_ta(tab, y, DEFAULT_BESZEL_PORT); lv_obj_add_event_cb(ta_beszel_port, ta_focused_cb, LV_EVENT_CLICKED, NULL); y += 54;
    make_label(tab, y, "Uptime Kuma Port"); y += 18;
    ta_uk_port  = make_ta(tab, y, DEFAULT_UK_PORT); lv_obj_add_event_cb(ta_uk_port,  ta_focused_cb, LV_EVENT_CLICKED, NULL); y += 54;
    make_save_btn(tab, y, on_save_server);

    return tab;
}

/***********************************************************
 * Proxy tab
 ***********************************************************/

static void on_save_proxy(lv_event_t *e)
{
    nvs_write_ta(NVS_KEY_PROXY_IP,   ta_proxy_ip);
    nvs_write_ta(NVS_KEY_PROXY_PORT, ta_proxy_port);
    ESP_LOGI(TAG, "Proxy config saved");
}

/* Task async per config_fetch_from_proxy() — non blocca il task LVGL. */
static void config_reload_task(void *arg)
{
    int ret = config_fetch_from_proxy();

    /* Aggiorna il label di stato con il lock LVGL */
    lv_port_sem_take();
    if (lbl_cfg_status != NULL) {
        if (ret == 0) {
            lv_label_set_text(lbl_cfg_status, "OK");
            lv_obj_set_style_text_color(lbl_cfg_status, lv_color_hex(0x7ec8a0),
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            lv_label_set_text(lbl_cfg_status, "Error");
            lv_obj_set_style_text_color(lbl_cfg_status, lv_color_hex(0xe07070),
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
    lv_port_sem_give();

    vTaskDelete(NULL);
}

static void on_reload_config(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    /* Salva prima i valori correnti del tab Proxy in NVS, così il fetch usa IP/porta aggiornati */
    nvs_write_ta(NVS_KEY_PROXY_IP,   ta_proxy_ip);
    nvs_write_ta(NVS_KEY_PROXY_PORT, ta_proxy_port);

    /* Feedback immediato */
    if (lbl_cfg_status != NULL) {
        lv_label_set_text(lbl_cfg_status, "Loading...");
        lv_obj_set_style_text_color(lbl_cfg_status, lv_color_hex(0xaaaaaa),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    /* Stack 4KB — config_fetch_from_proxy usa malloc per il buffer HTTP */
    xTaskCreate(config_reload_task, "cfg_reload", 4096, NULL, 5, NULL);
}

static lv_obj_t *build_tab_proxy(lv_obj_t *tabview)
{
    lv_obj_t *tab = lv_tabview_add_tab(tabview, "Proxy");
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);

    int y = 20;
    make_label(tab, y, "Mac Proxy IP");  y += 18;
    ta_proxy_ip   = make_ta(tab, y, "192.168.1.x"); lv_obj_add_event_cb(ta_proxy_ip,   ta_focused_cb, LV_EVENT_CLICKED, NULL); y += 54;
    make_label(tab, y, "Port");           y += 18;
    ta_proxy_port = make_ta(tab, y, "8765");         lv_obj_add_event_cb(ta_proxy_port, ta_focused_cb, LV_EVENT_CLICKED, NULL); y += 54;
    make_save_btn(tab, y, on_save_proxy); y += 60;

    /* Pulsante Ricarica config */
    lv_obj_t *btn_reload = lv_btn_create(tab);
    lv_obj_set_size(btn_reload, 180, 40);
    lv_obj_set_pos(btn_reload, 40, y);
    lv_obj_set_style_bg_color(btn_reload, lv_color_hex(0x2c5aa0),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(btn_reload, on_reload_config, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_lbl = lv_label_create(btn_reload);
    lv_label_set_text(btn_lbl, "Reload config");
    lv_obj_center(btn_lbl);
    lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_14,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    y += 50;

    /* Label feedback stato */
    lbl_cfg_status = lv_label_create(tab);
    lv_label_set_text(lbl_cfg_status, "");
    lv_obj_set_pos(lbl_cfg_status, 40, y);
    lv_obj_set_style_text_font(lbl_cfg_status, &lv_font_montserrat_14,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    y += 28;

    /* URL Config UI — recolor: "Config UI:" bianco + URL in #7ec8e0, centrata */
    lbl_proxy_url = lv_label_create(tab);
    lv_label_set_recolor(lbl_proxy_url, true);
    lv_label_set_text(lbl_proxy_url, "Config UI: #7ec8e0 http://--/config/ui#");
    lv_obj_set_width(lbl_proxy_url, 440);
    lv_label_set_long_mode(lbl_proxy_url, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(lbl_proxy_url, LV_TEXT_ALIGN_CENTER,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_proxy_url, &lv_font_montserrat_14,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_proxy_url, lv_color_white(),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(lbl_proxy_url, LV_ALIGN_TOP_MID, 0, y);

    return tab;
}

/***********************************************************
 * Meteo tab (sostituisce AI tab)
 ***********************************************************/

/* Write bool flag to NVS as "1" or "0". */
static void nvs_write_flag(const char *key, bool value)
{
    const char *v = value ? "1" : "0";
    indicator_storage_write((char *)key, (void *)v, 2); /* "1\0" or "0\0" */
}

static void on_sw_wthr(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    g_scr_wthr_enabled = lv_obj_has_state(sw_wthr, LV_STATE_CHECKED);
    nvs_write_flag(NVS_KEY_SCR_WTHR, g_scr_wthr_enabled);
    ESP_LOGI(TAG, "scr_wthr_enabled = %d", g_scr_wthr_enabled);
}

static lv_obj_t *build_tab_weather(lv_obj_t *tabview)
{
    lv_obj_t *tab = lv_tabview_add_tab(tabview, "Weather");
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);

    /* Info: configurazione via proxy */
    lv_obj_t *lbl_info = lv_label_create(tab);
    lv_label_set_text(lbl_info, "Configure via proxy Web UI:");
    lv_obj_set_pos(lbl_info, 40, 20);
    lv_obj_set_style_text_font(lbl_info, &lv_font_montserrat_14,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_info, lv_color_hex(0xcccccc),
                                LV_PART_MAIN | LV_STATE_DEFAULT);

    /* URL proxy dinamica (aggiornata in on_screen_load_start) */
    lbl_meteo_url = lv_label_create(tab);
    lv_label_set_text(lbl_meteo_url, "http://--/config/ui");
    lv_obj_set_pos(lbl_meteo_url, 40, 46);
    lv_obj_set_width(lbl_meteo_url, 400);
    lv_label_set_long_mode(lbl_meteo_url, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(lbl_meteo_url, &lv_font_montserrat_12,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_meteo_url, lv_color_hex(0x7ec8e0),
                                LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *lbl_hint = lv_label_create(tab);
    lv_label_set_text(lbl_hint,
                      "Set OWM API key, lat/lon, units\n"
                      "and city name from the proxy Web\n"
                      "UI. Synced to device on \"Reload\".");
    lv_obj_set_pos(lbl_hint, 40, 76);
    lv_obj_set_width(lbl_hint, 400);
    lv_label_set_long_mode(lbl_hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(lbl_hint, &lv_font_montserrat_12,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_hint, lv_color_hex(0x666666),
                                LV_PART_MAIN | LV_STATE_DEFAULT);

    return tab;
}

/***********************************************************
 * Schermate tab
 ***********************************************************/

/* Row: label on left, switch on right. Returns the switch object. */
static lv_obj_t *make_switch_row(lv_obj_t *parent, int y, const char *label_text)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_pos(lbl, 40, y + 4);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xdddddd), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *sw = lv_switch_create(parent);
    lv_obj_set_size(sw, 60, 30);
    lv_obj_set_pos(sw, 360, y);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0x529d53), LV_PART_INDICATOR | LV_STATE_CHECKED);
    return sw;
}

static void on_sw_defsens(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    g_scr_defsens_enabled = lv_obj_has_state(sw_defsens, LV_STATE_CHECKED);
    nvs_write_flag(NVS_KEY_SCR_DEFSENS, g_scr_defsens_enabled);
    ESP_LOGI(TAG, "scr_defsens_enabled = %d", g_scr_defsens_enabled);
}

static void on_sw_hue(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    g_scr_hue_enabled = lv_obj_has_state(sw_hue, LV_STATE_CHECKED);
    nvs_write_flag(NVS_KEY_SCR_HUE_EN, g_scr_hue_enabled);
    ESP_LOGI(TAG, "scr_hue_enabled = %d", g_scr_hue_enabled);
}

static void on_sw_srv(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    g_scr_srv_enabled = lv_obj_has_state(sw_srv, LV_STATE_CHECKED);
    nvs_write_flag(NVS_KEY_SCR_SRV_EN, g_scr_srv_enabled);
    ESP_LOGI(TAG, "scr_srv_enabled = %d", g_scr_srv_enabled);
}

static void on_sw_lnch(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    g_scr_lnch_enabled = lv_obj_has_state(sw_lnch, LV_STATE_CHECKED);
    nvs_write_flag(NVS_KEY_SCR_LNCH_EN, g_scr_lnch_enabled);
    ESP_LOGI(TAG, "scr_lnch_enabled = %d", g_scr_lnch_enabled);
}

static lv_obj_t *build_tab_screens(lv_obj_t *tabview)
{
    lv_obj_t *tab = lv_tabview_add_tab(tabview, "Screens");
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *hdr = lv_label_create(tab);
    lv_label_set_text(hdr, "Enable / disable screens");
    lv_obj_set_pos(hdr, 40, 10);
    lv_obj_set_style_text_font(hdr, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(hdr, lv_color_hex(0x888888), LV_PART_MAIN | LV_STATE_DEFAULT);

    int y = 50;
    sw_defsens = make_switch_row(tab, y, "Default sensor screen");
    lv_obj_add_event_cb(sw_defsens, on_sw_defsens, LV_EVENT_VALUE_CHANGED, NULL);
    y += 60;
    sw_hue  = make_switch_row(tab, y, "Hue Control");
    lv_obj_add_event_cb(sw_hue,  on_sw_hue,  LV_EVENT_VALUE_CHANGED, NULL);
    y += 60;
    sw_srv  = make_switch_row(tab, y, "LocalServer");
    lv_obj_add_event_cb(sw_srv,  on_sw_srv,  LV_EVENT_VALUE_CHANGED, NULL);
    y += 60;
    sw_lnch = make_switch_row(tab, y, "Launcher");
    lv_obj_add_event_cb(sw_lnch, on_sw_lnch, LV_EVENT_VALUE_CHANGED, NULL);
    y += 60;
    sw_wthr = make_switch_row(tab, y, "Weather");
    lv_obj_add_event_cb(sw_wthr, on_sw_wthr, LV_EVENT_VALUE_CHANGED, NULL);

    return tab;
}

/***********************************************************
 * Traffic tab
 ***********************************************************/

static void on_sw_traffic(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    g_scr_traffic_enabled = lv_obj_has_state(sw_traffic, LV_STATE_CHECKED);
    nvs_write_flag(NVS_KEY_SCR_TRAFFIC_EN, g_scr_traffic_enabled);
    ESP_LOGI(TAG, "scr_traffic_enabled = %d", g_scr_traffic_enabled);
}

static lv_obj_t *build_tab_traffic(lv_obj_t *tabview)
{
    lv_obj_t *tab = lv_tabview_add_tab(tabview, "Traffic");
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);

    /* Switch ON/OFF schermata */
    int y = 20;
    sw_traffic = make_switch_row(tab, y, "Traffic screen");
    lv_obj_add_event_cb(sw_traffic, on_sw_traffic, LV_EVENT_VALUE_CHANGED, NULL);
    y += 70;

    /* Info label */
    lv_obj_t *lbl_info = lv_label_create(tab);
    lv_label_set_text(lbl_info, "Configure origin/destination at:");
    lv_obj_set_pos(lbl_info, 40, y);
    lv_obj_set_style_text_font(lbl_info, &lv_font_montserrat_14,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_info, lv_color_hex(0xcccccc),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    y += 26;

    /* URL proxy dinamica (aggiornata in on_screen_load_start) */
    lbl_traffic_url = lv_label_create(tab);
    lv_label_set_text(lbl_traffic_url, "http://--/config/ui");
    lv_obj_set_pos(lbl_traffic_url, 40, y);
    lv_obj_set_width(lbl_traffic_url, 400);
    lv_label_set_long_mode(lbl_traffic_url, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(lbl_traffic_url, &lv_font_montserrat_12,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_traffic_url, lv_color_hex(0x7ec8e0),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    y += 30;

    lv_obj_t *lbl_hint = lv_label_create(tab);
    lv_label_set_text(lbl_hint,
                      "Set Google Maps API key, origin,\n"
                      "destination and mode from the\n"
                      "proxy Web UI. Refresh via \"Reload\"\n"
                      "in the Proxy tab.");
    lv_obj_set_pos(lbl_hint, 40, y);
    lv_obj_set_width(lbl_hint, 400);
    lv_label_set_long_mode(lbl_hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(lbl_hint, &lv_font_montserrat_12,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_hint, lv_color_hex(0x666666),
                                LV_PART_MAIN | LV_STATE_DEFAULT);

    return tab;
}

/***********************************************************
 * SCREEN_LOAD_START → populate fields from NVS
 ***********************************************************/

static void on_screen_load_start(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_SCREEN_LOAD_START) return;

    nvs_read_str(NVS_KEY_HUE_IP,      ta_hue_ip,       APP_CFG_HUE_BRIDGE_IP);
    nvs_read_str(NVS_KEY_HUE_API_KEY, ta_hue_key,      APP_CFG_HUE_API_KEY);
    nvs_read_str(NVS_KEY_HUE_LIGHT_1, ta_hue_l1,       APP_CFG_HUE_LIGHT_1);
    nvs_read_str(NVS_KEY_HUE_LIGHT_2, ta_hue_l2,       APP_CFG_HUE_LIGHT_2);
    nvs_read_str(NVS_KEY_HUE_LIGHT_3, ta_hue_l3,       APP_CFG_HUE_LIGHT_3);
    nvs_read_str(NVS_KEY_HUE_LIGHT_4, ta_hue_l4,       APP_CFG_HUE_LIGHT_4);

    nvs_read_str(NVS_KEY_SERVER_IP,   ta_srv_ip,         APP_CFG_SERVER_IP);
    nvs_read_str(NVS_KEY_SERVER_PORT, ta_srv_port,      APP_CFG_SERVER_PORT);
    nvs_read_str(NVS_KEY_SERVER_NAME, ta_srv_name,      APP_CFG_SERVER_NAME);
    nvs_read_str(NVS_KEY_BESZEL_PORT, ta_beszel_port,   DEFAULT_BESZEL_PORT);
    nvs_read_str(NVS_KEY_UK_PORT,     ta_uk_port,       DEFAULT_UK_PORT);

    nvs_read_str(NVS_KEY_PROXY_IP,    ta_proxy_ip,      APP_CFG_PROXY_IP);
    nvs_read_str(NVS_KEY_PROXY_PORT,  ta_proxy_port,    APP_CFG_PROXY_PORT);

    /* Aggiorna URL proxy nel tab Proxy, Weather e Traffic */
    {
        char proxy_ip[64], proxy_port[8], url_text[128];
        nvs_read_str_buf(NVS_KEY_PROXY_IP,   proxy_ip,   sizeof(proxy_ip),   APP_CFG_PROXY_IP);
        nvs_read_str_buf(NVS_KEY_PROXY_PORT, proxy_port, sizeof(proxy_port), APP_CFG_PROXY_PORT);
        snprintf(url_text, sizeof(url_text),
                 "http://%s:%s/config/ui", proxy_ip, proxy_port);
        lv_label_set_text(lbl_meteo_url,    url_text);
        lv_label_set_text(lbl_traffic_url,  url_text);
        snprintf(url_text, sizeof(url_text),
                 "Config UI: #7ec8e0 http://%s:%s/config/ui#", proxy_ip, proxy_port);
        lv_label_set_text(lbl_proxy_url, url_text);
    }

    /* Schermate switches: riflette i flag globali (già caricati da NVS al boot). */
    if (g_scr_defsens_enabled)  lv_obj_add_state(sw_defsens,  LV_STATE_CHECKED);
    else                         lv_obj_clear_state(sw_defsens,  LV_STATE_CHECKED);
    if (g_scr_hue_enabled)      lv_obj_add_state(sw_hue,       LV_STATE_CHECKED);
    else                         lv_obj_clear_state(sw_hue,       LV_STATE_CHECKED);
    if (g_scr_srv_enabled)      lv_obj_add_state(sw_srv,       LV_STATE_CHECKED);
    else                         lv_obj_clear_state(sw_srv,       LV_STATE_CHECKED);
    if (g_scr_lnch_enabled)     lv_obj_add_state(sw_lnch,      LV_STATE_CHECKED);
    else                         lv_obj_clear_state(sw_lnch,      LV_STATE_CHECKED);
    if (g_scr_wthr_enabled)     lv_obj_add_state(sw_wthr,      LV_STATE_CHECKED);
    else                         lv_obj_clear_state(sw_wthr,      LV_STATE_CHECKED);
    if (g_scr_traffic_enabled)  lv_obj_add_state(sw_traffic,   LV_STATE_CHECKED);
    else                         lv_obj_clear_state(sw_traffic,   LV_STATE_CHECKED);
}

/***********************************************************
 * public init
 ***********************************************************/

/* Lightweight: crea solo l'oggetto schermata. Nessun widget.
 * Garantisce che il puntatore ui_screen_settings_custom sia valido
 * anche prima della populate lazily, così _ui_screen_change non crasha. */
void screen_settings_custom_init(void)
{
    ui_screen_settings_custom = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_screen_settings_custom, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_screen_settings_custom, lv_color_hex(0x101418),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
}

/* Heavy: aggiunge tabview, campi e gestori eventi.
 * Chiamato lazily dal gesture handler la prima volta che l'utente naviga qui.
 * Deve essere chiamato con il lock LVGL già acquisito (dentro lv_timer_handler). */
void screen_settings_custom_populate(void)
{
    /* tabview — 44px tab bar height */
    lv_obj_t *tv = lv_tabview_create(ui_screen_settings_custom, LV_DIR_TOP, 44);
    s_tv = tv;
    lv_obj_set_size(tv, 480, 480);
    lv_obj_set_pos(tv, 0, 0);
    lv_obj_set_style_bg_color(tv, lv_color_hex(0x101418), LV_PART_MAIN | LV_STATE_DEFAULT);

    /* tab bar styling */
    lv_obj_t *tab_bar = lv_tabview_get_tab_btns(tv);
    lv_obj_set_style_bg_color(tab_bar, lv_color_hex(0x1e1e2e), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(tab_bar, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(tab_bar, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(tab_bar, lv_color_hex(0x529d53),
                                LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_side(tab_bar, LV_BORDER_SIDE_BOTTOM,
                                 LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(tab_bar, lv_color_hex(0x529d53),
                                  LV_PART_ITEMS | LV_STATE_CHECKED);

    /* build tabs (order matters: tab index 0..5) */
    build_tab_hue(tv);
    build_tab_server(tv);
    build_tab_proxy(tv);
    build_tab_weather(tv);
    build_tab_screens(tv);
    build_tab_traffic(tv);

    /* populate fields on screen load */
    lv_obj_add_event_cb(ui_screen_settings_custom, on_screen_load_start,
                        LV_EVENT_SCREEN_LOAD_START, NULL);
}
