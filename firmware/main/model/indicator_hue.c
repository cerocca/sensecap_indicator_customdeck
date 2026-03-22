#include "indicator_hue.h"
#include "indicator_storage.h"
#include "app_config.h"
#include "lv_port.h"        /* lv_port_sem_take / lv_port_sem_give */

#include "esp_http_client.h"
#include "esp_event.h"
#include "esp_wifi.h"       /* IP_EVENT, IP_EVENT_STA_GOT_IP */
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "indicator_hue";

/* HTTP response buffer (min 2048 — CLAUDE.md regola 5) */
#define HUE_BUF_SIZE 2048

/* ─── Internal state ──────────────────────────────────────────────────────── */

static hue_light_t    s_lights[HUE_LIGHT_COUNT];
static char           s_bridge_ip[64];
static char           s_api_key[128];
static bool           s_poll_started = false;
static hue_update_cb_t s_update_cb   = NULL;

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

/* ─── Config load (chiamato all'inizio di ogni ciclo poll) ────────────────── */

static void hue_read_config(void)
{
    nvs_read_str(NVS_KEY_HUE_IP,      s_bridge_ip, sizeof(s_bridge_ip),
                 APP_CFG_HUE_BRIDGE_IP);
    nvs_read_str(NVS_KEY_HUE_API_KEY, s_api_key,   sizeof(s_api_key),
                 APP_CFG_HUE_API_KEY);

    static const char *name_keys[HUE_LIGHT_COUNT] = {
        NVS_KEY_HUE_LIGHT_1, NVS_KEY_HUE_LIGHT_2,
        NVS_KEY_HUE_LIGHT_3, NVS_KEY_HUE_LIGHT_4,
    };
    static const char *id_keys[HUE_LIGHT_COUNT] = {
        NVS_KEY_HUE_LIGHT_1_ID, NVS_KEY_HUE_LIGHT_2_ID,
        NVS_KEY_HUE_LIGHT_3_ID, NVS_KEY_HUE_LIGHT_4_ID,
    };
    static const char *name_defaults[HUE_LIGHT_COUNT] = {
        APP_CFG_HUE_LIGHT_1, APP_CFG_HUE_LIGHT_2,
        APP_CFG_HUE_LIGHT_3, APP_CFG_HUE_LIGHT_4,
    };

    for (int i = 0; i < HUE_LIGHT_COUNT; i++) {
        nvs_read_str(name_keys[i], s_lights[i].name,
                     sizeof(s_lights[i].name), name_defaults[i]);
        nvs_read_str(id_keys[i], s_lights[i].id,
                     sizeof(s_lights[i].id), "");
    }
}

/* ─── HTTPS client factory ────────────────────────────────────────────────── */

/*
 * Hue Bridge usa HTTPS con certificato self-signed.
 * Con cert_pem=NULL, use_global_ca_store=false, crt_bundle_attach=NULL,
 * ESP-TLS imposta MBEDTLS_SSL_VERIFY_NONE → nessuna verifica certificato.
 */
static esp_http_client_handle_t hue_client_open(const char *url,
                                                 esp_http_client_method_t method,
                                                 int content_len)
{
    esp_http_client_config_t cfg = {
        .url                        = url,
        .method                     = method,
        .timeout_ms                 = 5000,
        .skip_cert_common_name_check = true,
        .use_global_ca_store        = false,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return NULL;

    esp_http_client_set_header(client, "hue-application-key", s_api_key);

    if (content_len > 0)
        esp_http_client_set_header(client, "Content-Type", "application/json");

    esp_err_t err = esp_http_client_open(client, content_len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return NULL;
    }
    return client;
}

/* ─── GET — lettura stato singola luce ────────────────────────────────────── */

static void hue_get_light_state(int idx)
{
    if (s_lights[idx].id[0] == '\0') return;

    char url[200];
    snprintf(url, sizeof(url), "https://%s/clip/v2/resource/light/%s",
             s_bridge_ip, s_lights[idx].id);

    /* static: evita pressione sullo stack del task — CLAUDE.md regola 6 */
    static char buf[HUE_BUF_SIZE];
    memset(buf, 0, sizeof(buf));

    esp_http_client_handle_t client =
        hue_client_open(url, HTTP_METHOD_GET, 0);
    if (!client) return;

    esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        ESP_LOGW(TAG, "GET light[%d] HTTP %d", idx, status);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return;
    }

    /* Leggi risposta — esp_http_client_read non fa loop automatico */
    int total = 0, r;
    while ((r = esp_http_client_read(client, buf + total,
                                     HUE_BUF_SIZE - 1 - total)) > 0) {
        total += r;
        if (total >= HUE_BUF_SIZE - 1) break;
    }
    buf[total] = '\0';

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (total == 0) return;

    cJSON *root = cJSON_Parse(buf);
    if (!root) return;

    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (cJSON_IsArray(data) && cJSON_GetArraySize(data) > 0) {
        cJSON *item = cJSON_GetArrayItem(data, 0);

        cJSON *on_obj = cJSON_GetObjectItem(item, "on");
        if (on_obj) {
            cJSON *on_val = cJSON_GetObjectItem(on_obj, "on");
            if (cJSON_IsBool(on_val))
                s_lights[idx].on = cJSON_IsTrue(on_val);
        }

        cJSON *dimming = cJSON_GetObjectItem(item, "dimming");
        if (dimming) {
            cJSON *bri = cJSON_GetObjectItem(dimming, "brightness");
            if (cJSON_IsNumber(bri))
                s_lights[idx].brightness = (float)bri->valuedouble;
        }

        s_lights[idx].reachable = true;
    }

    cJSON_Delete(root);
}

/* ─── PUT — toggle o brightness ───────────────────────────────────────────── */

static void hue_put(int idx, const char *body)
{
    if (s_lights[idx].id[0] == '\0') return;

    char url[200];
    snprintf(url, sizeof(url), "https://%s/clip/v2/resource/light/%s",
             s_bridge_ip, s_lights[idx].id);

    int body_len = strlen(body);
    esp_http_client_handle_t client =
        hue_client_open(url, HTTP_METHOD_PUT, body_len);
    if (!client) return;

    esp_http_client_write(client, body, body_len);
    esp_http_client_fetch_headers(client);

    int status = esp_http_client_get_status_code(client);
    if (status != 200)
        ESP_LOGW(TAG, "PUT light[%d] HTTP %d", idx, status);

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
}

/* ─── Command task (one-shot) ─────────────────────────────────────────────── */

typedef enum { HUE_CMD_TOGGLE, HUE_CMD_BRIGHTNESS } hue_cmd_type_t;

typedef struct {
    int             idx;
    hue_cmd_type_t  type;
    bool            new_on;
    float           brightness;
} hue_cmd_t;

static void hue_cmd_task(void *arg)
{
    hue_cmd_t *cmd = (hue_cmd_t *)arg;
    char body[64];

    if (cmd->type == HUE_CMD_TOGGLE) {
        snprintf(body, sizeof(body), "{\"on\":{\"on\":%s}}",
                 cmd->new_on ? "true" : "false");
        hue_put(cmd->idx, body);
        /* Aggiornamento ottimistico — il poll successivo riconcilia */
        s_lights[cmd->idx].on = cmd->new_on;
    } else {
        snprintf(body, sizeof(body), "{\"dimming\":{\"brightness\":%.1f}}",
                 cmd->brightness);
        hue_put(cmd->idx, body);
        s_lights[cmd->idx].brightness = cmd->brightness;
    }

    free(cmd);
    vTaskDelete(NULL);
}

/* ─── Poll task (loop permanente ogni 5 s) ────────────────────────────────── */

static void hue_poll_task(void *arg)
{
    /* Attende il completamento del config fetch (1.5 s) prima di leggere NVS */
    vTaskDelay(pdMS_TO_TICKS(3000));

    while (1) {
        hue_read_config();

        if (s_bridge_ip[0] != '\0' && s_api_key[0] != '\0') {
            for (int i = 0; i < HUE_LIGHT_COUNT; i++)
                hue_get_light_state(i);

            /* Notifica UI — deve avvenire con LVGL sem acquisito */
            if (s_update_cb) {
                lv_port_sem_take();
                s_update_cb();
                lv_port_sem_give();
            }
        } else {
            ESP_LOGD(TAG, "Config Hue non ancora disponibile, skip poll");
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/* ─── IP event → avvio poll ───────────────────────────────────────────────── */

static void ip_event_handler(void *arg, esp_event_base_t base,
                              int32_t id, void *data)
{
    if (id != IP_EVENT_STA_GOT_IP) return;
    if (s_poll_started) return;
    s_poll_started = true;
    xTaskCreate(hue_poll_task, "hue_poll", 6144, NULL, 4, NULL);
    ESP_LOGI(TAG, "hue_poll_task avviato");
}

/* ─── Public API ──────────────────────────────────────────────────────────── */

void indicator_hue_set_update_cb(hue_update_cb_t cb)
{
    s_update_cb = cb;
}

void indicator_hue_get_light(int idx, hue_light_t *out)
{
    if (idx < 0 || idx >= HUE_LIGHT_COUNT || !out) return;
    *out = s_lights[idx];
}

void indicator_hue_toggle(int idx)
{
    if (idx < 0 || idx >= HUE_LIGHT_COUNT) return;
    hue_cmd_t *cmd = malloc(sizeof(hue_cmd_t));
    if (!cmd) return;
    cmd->idx    = idx;
    cmd->type   = HUE_CMD_TOGGLE;
    cmd->new_on = !s_lights[idx].on;
    xTaskCreate(hue_cmd_task, "hue_cmd", 4096, cmd, 5, NULL);
}

void indicator_hue_set_brightness(int idx, float brightness)
{
    if (idx < 0 || idx >= HUE_LIGHT_COUNT) return;
    hue_cmd_t *cmd = malloc(sizeof(hue_cmd_t));
    if (!cmd) return;
    cmd->idx        = idx;
    cmd->type       = HUE_CMD_BRIGHTNESS;
    cmd->brightness = brightness;
    xTaskCreate(hue_cmd_task, "hue_cmd", 4096, cmd, 5, NULL);
}

void indicator_hue_init(void)
{
    hue_read_config();
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                ip_event_handler, NULL);
    ESP_LOGI(TAG, "indicator_hue_init: handler IP registrato");
}
