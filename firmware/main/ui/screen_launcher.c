#include "screen_launcher.h"
#include "indicator_storage.h"
#include "app_config.h"

#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

static const char *TAG = "LAUNCHER";

/* ─── Screen object ───────────────────────────────────────────────────────── */

lv_obj_t *ui_screen_launcher;

/* ─── Widget handles (validi solo dopo populate) ─────────────────────────── */

static lv_obj_t *s_btn[4];
static lv_obj_t *s_lbl[4];
static bool      s_populated = false;

/* ─── NVS helper ──────────────────────────────────────────────────────────── */

static void nvs_read_str(const char *key, char *buf, size_t len,
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

/* ─── Label refresh (on every SCREEN_LOAD_START) ─────────────────────────── */

static const char *const s_nvs_keys[4] = {
    NVS_KEY_LNCH_NAME_1, NVS_KEY_LNCH_NAME_2,
    NVS_KEY_LNCH_NAME_3, NVS_KEY_LNCH_NAME_4,
};
static const char *const s_defaults[4] = {
    APP_CFG_LNCH_NAME_1, APP_CFG_LNCH_NAME_2,
    APP_CFG_LNCH_NAME_3, APP_CFG_LNCH_NAME_4,
};

static void refresh_labels(void)
{
    char buf[128];
    for (int i = 0; i < 4; i++) {
        nvs_read_str(s_nvs_keys[i], buf, sizeof(buf), s_defaults[i]);
        for (int j = 0; buf[j]; j++) buf[j] = (char)toupper((unsigned char)buf[j]);
        lv_label_set_text(s_lbl[i], buf);
    }
}

/* ─── HTTP fire-and-forget ────────────────────────────────────────────────── */

typedef struct {
    int  btn_num;
    char proxy_ip[64];
    char proxy_port[8];
} launcher_open_args_t;

static void open_url_task(void *arg)
{
    launcher_open_args_t *a = (launcher_open_args_t *)arg;

    char url[128];
    snprintf(url, sizeof(url), "http://%s:%s/open/%d",
             a->proxy_ip, a->proxy_port, a->btn_num);

    free(a);

    esp_http_client_config_t cfg = {
        .url        = url,
        .method     = HTTP_METHOD_GET,
        .timeout_ms = 3000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        ESP_LOGW(TAG, "HTTP init failed for %s", url);
        vTaskDelete(NULL);
        return;
    }
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "%s failed: %s", url, esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
    vTaskDelete(NULL);
}

/* ─── Button event handler ────────────────────────────────────────────────── */

static void btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t       *btn  = lv_event_get_target(e);

    if (code == LV_EVENT_PRESSED) {
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x4a90d9),
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        return;
    }
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1e1e2e),
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (code == LV_EVENT_RELEASED) {
        int idx = (int)(intptr_t)lv_event_get_user_data(e);

        launcher_open_args_t *args = malloc(sizeof(*args));
        if (!args) {
            ESP_LOGW(TAG, "malloc failed, skip /open/%d", idx + 1);
            return;
        }
        args->btn_num = idx + 1;
        nvs_read_str(NVS_KEY_PROXY_IP,   args->proxy_ip,
                     sizeof(args->proxy_ip),   APP_CFG_PROXY_IP);
        nvs_read_str(NVS_KEY_PROXY_PORT, args->proxy_port,
                     sizeof(args->proxy_port), APP_CFG_PROXY_PORT);

        xTaskCreate(open_url_task, "lnch_open", 3072, args,
                    tskIDLE_PRIORITY + 1, NULL);
    }
}

/* ─── Screen load event ───────────────────────────────────────────────────── */

static void on_screen_load_start(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_SCREEN_LOAD_START) return;
    if (!s_populated)
        screen_launcher_populate();
    refresh_labels();
}

/* ─── Populate (heavy, lazy al primo SCREEN_LOAD_START) ──────────────────── */

void screen_launcher_populate(void)
{
    lv_obj_t *scr = ui_screen_launcher;

    /*
     * Griglia 2×2: pulsanti 200×170 px su schermo 480×480.
     * Col 0: x=20,  Col 1: x=260  → margini 20px, gap 40px orizzontale
     * Row 0: y=55,  Row 1: y=255  → margini 55px, gap 30px verticale
     */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Launcher");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    static const int xs[2] = {20, 260};
    static const int ys[2] = {55, 255};

    for (int i = 0; i < 4; i++) {
        int col = i % 2;
        int row = i / 2;

        lv_obj_t *btn = lv_obj_create(scr);
        lv_obj_set_size(btn, 200, 170);
        lv_obj_set_pos(btn, xs[col], ys[row]);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1e1e2e),
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x3a3a5c),
                                       LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(btn, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(btn, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_ALL,
                             (void *)(intptr_t)i);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(lbl, 180);
        lv_label_set_text(lbl, "...");
        lv_obj_set_style_text_color(lbl, lv_color_white(),
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER,
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

        s_btn[i] = btn;
        s_lbl[i] = lbl;
    }

    s_populated = true;
}

/* ─── Init (lightweight — solo schermo + event handler) ──────────────────── */

void screen_launcher_init(void)
{
    ui_screen_launcher = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_screen_launcher, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_screen_launcher, lv_color_hex(0x0d1117),
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    /* Lazy populate + refresh labels al caricamento schermata */
    lv_obj_add_event_cb(ui_screen_launcher, on_screen_load_start,
                        LV_EVENT_SCREEN_LOAD_START, NULL);
}
