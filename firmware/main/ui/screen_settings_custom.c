#include "screen_settings_custom.h"
#include "ui_helpers.h"           /* _ui_screen_change */
#include "indicator_storage.h"
#include "app_config.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "settings_custom";

/* ─── navigation externs ────────────────────────────────────── */
extern lv_obj_t *ui_screen_wifi;   /* Seeed Wi-Fi screen          */
extern lv_obj_t *ui_screen_last;   /* back-target for wifi screen  */

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

/* ─── Proxy tab textareas ───────────────────────────────────── */
static lv_obj_t *ta_proxy_ip;
static lv_obj_t *ta_proxy_port;

/* ─── AI tab textareas ──────────────────────────────────────── */
static lv_obj_t *ta_ai_key;
static lv_obj_t *ta_ai_endpoint;

/***********************************************************
 * helpers
 ***********************************************************/

/* Read string from NVS; on failure, use fallback. */
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

static void kb_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        if (s_kb != NULL) {
            lv_obj_del(s_kb);
            s_kb = NULL;
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
    }
    lv_keyboard_set_textarea(s_kb, ta);
}

/***********************************************************
 * Wi-Fi tab
 ***********************************************************/

static void on_wifi_btn(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    ui_screen_last = ui_screen_settings_custom;
    _ui_screen_change(ui_screen_wifi, LV_SCR_LOAD_ANIM_OVER_BOTTOM, 200, 0);
}

static lv_obj_t *build_tab_wifi(lv_obj_t *tabview)
{
    lv_obj_t *tab = lv_tabview_add_tab(tabview, "Wi-Fi");
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(tab);
    lv_label_set_text(lbl, "Gestisci la connessione Wi-Fi");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xcccccc), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_pos(lbl, 40, 20);

    lv_obj_t *btn = lv_btn_create(tab);
    lv_obj_set_size(btn, 280, 60);
    lv_obj_set_pos(btn, 100, 80);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2c5aa0), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(btn, on_wifi_btn, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, "Configura Wi-Fi");
    lv_obj_center(btn_lbl);
    lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);

    return tab;
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
    make_label(tab, y, "Luce 1");         y += 18;
    ta_hue_l1  = make_ta(tab, y, "Light 1");    lv_obj_add_event_cb(ta_hue_l1,  ta_focused_cb, LV_EVENT_CLICKED, NULL); y += 46;
    make_label(tab, y, "Luce 2");         y += 18;
    ta_hue_l2  = make_ta(tab, y, "Light 2");    lv_obj_add_event_cb(ta_hue_l2,  ta_focused_cb, LV_EVENT_CLICKED, NULL); y += 46;
    make_label(tab, y, "Luce 3");         y += 18;
    ta_hue_l3  = make_ta(tab, y, "Light 3");    lv_obj_add_event_cb(ta_hue_l3,  ta_focused_cb, LV_EVENT_CLICKED, NULL); y += 46;
    make_label(tab, y, "Luce 4");         y += 18;
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
    ESP_LOGI(TAG, "Server config saved");
}

static lv_obj_t *build_tab_server(lv_obj_t *tabview)
{
    lv_obj_t *tab = lv_tabview_add_tab(tabview, "Server");
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);

    int y = 20;
    make_label(tab, y, "IP Sibilla");   y += 18;
    ta_srv_ip   = make_ta(tab, y, "192.168.1.x"); lv_obj_add_event_cb(ta_srv_ip,   ta_focused_cb, LV_EVENT_CLICKED, NULL); y += 54;
    make_label(tab, y, "Porta Glances"); y += 18;
    ta_srv_port = make_ta(tab, y, "61208");        lv_obj_add_event_cb(ta_srv_port, ta_focused_cb, LV_EVENT_CLICKED, NULL); y += 54;
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

static lv_obj_t *build_tab_proxy(lv_obj_t *tabview)
{
    lv_obj_t *tab = lv_tabview_add_tab(tabview, "Proxy");
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);

    int y = 20;
    make_label(tab, y, "IP Proxy Mac");  y += 18;
    ta_proxy_ip   = make_ta(tab, y, "192.168.1.x"); lv_obj_add_event_cb(ta_proxy_ip,   ta_focused_cb, LV_EVENT_CLICKED, NULL); y += 54;
    make_label(tab, y, "Porta");          y += 18;
    ta_proxy_port = make_ta(tab, y, "8765");         lv_obj_add_event_cb(ta_proxy_port, ta_focused_cb, LV_EVENT_CLICKED, NULL); y += 54;
    make_save_btn(tab, y, on_save_proxy);

    return tab;
}

/***********************************************************
 * AI tab
 ***********************************************************/

static void on_save_ai(lv_event_t *e)
{
    nvs_write_ta(NVS_KEY_AI_API_KEY,  ta_ai_key);
    nvs_write_ta(NVS_KEY_AI_ENDPOINT, ta_ai_endpoint);
    ESP_LOGI(TAG, "AI config saved");
}

static lv_obj_t *build_tab_ai(lv_obj_t *tabview)
{
    lv_obj_t *tab = lv_tabview_add_tab(tabview, "AI");
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);

    int y = 20;
    make_label(tab, y, "Claude API Key");  y += 18;
    ta_ai_key      = make_ta(tab, y, "sk-ant-...");  lv_obj_add_event_cb(ta_ai_key,      ta_focused_cb, LV_EVENT_CLICKED, NULL); y += 54;
    make_label(tab, y, "Endpoint");         y += 18;
    ta_ai_endpoint = make_ta(tab, y, "https://api.anthropic.com"); lv_obj_add_event_cb(ta_ai_endpoint, ta_focused_cb, LV_EVENT_CLICKED, NULL); y += 54;
    make_save_btn(tab, y, on_save_ai);

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

    nvs_read_str(NVS_KEY_SERVER_IP,   ta_srv_ip,        APP_CFG_SERVER_IP);
    nvs_read_str(NVS_KEY_SERVER_PORT, ta_srv_port,      APP_CFG_SERVER_PORT);

    nvs_read_str(NVS_KEY_PROXY_IP,    ta_proxy_ip,      APP_CFG_PROXY_IP);
    nvs_read_str(NVS_KEY_PROXY_PORT,  ta_proxy_port,    APP_CFG_PROXY_PORT);

    nvs_read_str(NVS_KEY_AI_API_KEY,  ta_ai_key,        APP_CFG_AI_API_KEY);
    nvs_read_str(NVS_KEY_AI_ENDPOINT, ta_ai_endpoint,   APP_CFG_AI_ENDPOINT);
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

    /* build tabs (order matters: tab index 0..4) */
    build_tab_wifi(tv);
    build_tab_hue(tv);
    build_tab_server(tv);
    build_tab_proxy(tv);
    build_tab_ai(tv);

    /* populate fields on screen load */
    lv_obj_add_event_cb(ui_screen_settings_custom, on_screen_load_start,
                        LV_EVENT_SCREEN_LOAD_START, NULL);
}
