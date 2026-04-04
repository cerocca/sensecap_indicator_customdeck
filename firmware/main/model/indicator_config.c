#include "indicator_config.h"
#include "indicator_storage.h"
#include "app_config.h"

#include "esp_http_client.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <stdio.h>

static const char *TAG = "indicator_config";

/* HTTP response buffer — 2048 bytes min (CLAUDE.md regola 5) */
#define CFG_BUF_SIZE 2048

/* Guard: fetch al boot una sola volta */
static bool s_boot_fetched = false;

/* ─── NVS helpers ──────────────────────────────────────────────────────────── */

static void nvs_save_str(const char *key, const char *value)
{
    if (!value || value[0] == '\0') return;
    indicator_storage_write((char *)key, (void *)value, strlen(value) + 1);
}

static void nvs_read_str(const char *key, char *buf, size_t buf_len,
                         const char *fallback)
{
    size_t len = buf_len;
    if (indicator_storage_read((char *)key, buf, &len) == 0 && len > 1) {
        buf[buf_len - 1] = '\0';
    } else {
        strncpy(buf, fallback, buf_len - 1);
        buf[buf_len - 1] = '\0';
    }
}

/* ─── JSON field helper ────────────────────────────────────────────────────── */

static void save_json_str(cJSON *root, const char *json_key,
                          const char *nvs_key)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, json_key);
    if (cJSON_IsString(item) && item->valuestring) {
        nvs_save_str(nvs_key, item->valuestring);
        ESP_LOGD(TAG, "  %s = %s", nvs_key, item->valuestring);
    }
}

/* ─── config_fetch_from_proxy ──────────────────────────────────────────────── */

int config_fetch_from_proxy(void)
{
    char proxy_ip[64];
    char proxy_port[8];
    nvs_read_str(NVS_KEY_PROXY_IP,   proxy_ip,   sizeof(proxy_ip),   APP_CFG_PROXY_IP);
    nvs_read_str(NVS_KEY_PROXY_PORT, proxy_port, sizeof(proxy_port), APP_CFG_PROXY_PORT);

    char url[128];
    snprintf(url, sizeof(url), "http://%s:%s/config", proxy_ip, proxy_port);
    ESP_LOGI(TAG, "Fetching config from %s", url);

    /* Allocate response buffer on heap (struct grandi in task: static o heap) */
    char *buf = (char *)malloc(CFG_BUF_SIZE);
    if (!buf) {
        ESP_LOGE(TAG, "malloc failed");
        return -1;
    }
    memset(buf, 0, CFG_BUF_SIZE);

    esp_http_client_config_t cfg = {
        .url            = url,
        .method         = HTTP_METHOD_GET,
        .timeout_ms     = 5000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        ESP_LOGE(TAG, "http client init failed");
        free(buf);
        return -1;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "http open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        free(buf);
        return -1;
    }

    int content_len = esp_http_client_fetch_headers(client);
    int http_status = esp_http_client_get_status_code(client);
    if (http_status != 200) {
        ESP_LOGE(TAG, "HTTP status %d", http_status);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        free(buf);
        return -1;
    }

    /* Leggi risposta — esp_http_client_read non fa loop automatico */
    int total = 0;
    int r;
    while ((r = esp_http_client_read(client, buf + total,
                                     CFG_BUF_SIZE - 1 - total)) > 0) {
        total += r;
        if (total >= CFG_BUF_SIZE - 1) break;
    }
    buf[total] = '\0';

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (total == 0) {
        ESP_LOGE(TAG, "empty response");
        free(buf);
        return -1;
    }

    ESP_LOGD(TAG, "response (%d bytes): %s", total, buf);

    /* Parse JSON */
    cJSON *root = cJSON_Parse(buf);
    free(buf);

    if (!root) {
        ESP_LOGE(TAG, "JSON parse error");
        return -1;
    }

    /* Salva ogni campo in NVS */
    save_json_str(root, "hue_bridge_ip",  NVS_KEY_HUE_IP);
    save_json_str(root, "hue_api_key",    NVS_KEY_HUE_API_KEY);
    save_json_str(root, "hue_light_1",    NVS_KEY_HUE_LIGHT_1);
    save_json_str(root, "hue_light_2",    NVS_KEY_HUE_LIGHT_2);
    save_json_str(root, "hue_light_3",    NVS_KEY_HUE_LIGHT_3);
    save_json_str(root, "hue_light_4",    NVS_KEY_HUE_LIGHT_4);
    save_json_str(root, "hue_light_1_id", NVS_KEY_HUE_LIGHT_1_ID);
    save_json_str(root, "hue_light_2_id", NVS_KEY_HUE_LIGHT_2_ID);
    save_json_str(root, "hue_light_3_id", NVS_KEY_HUE_LIGHT_3_ID);
    save_json_str(root, "hue_light_4_id", NVS_KEY_HUE_LIGHT_4_ID);
    save_json_str(root, "server_ip",      NVS_KEY_SERVER_IP);
    save_json_str(root, "server_port",    NVS_KEY_SERVER_PORT);
    save_json_str(root, "srv_name",       NVS_KEY_SERVER_NAME);
    save_json_str(root, "proxy_ip",       NVS_KEY_PROXY_IP);
    save_json_str(root, "proxy_port",     NVS_KEY_PROXY_PORT);
    save_json_str(root, "launcher_url_1",  NVS_KEY_LNCH_URL_1);
    save_json_str(root, "launcher_url_2",  NVS_KEY_LNCH_URL_2);
    save_json_str(root, "launcher_url_3",  NVS_KEY_LNCH_URL_3);
    save_json_str(root, "launcher_url_4",  NVS_KEY_LNCH_URL_4);
    save_json_str(root, "lnch_name_1",     NVS_KEY_LNCH_NAME_1);
    save_json_str(root, "lnch_name_2",     NVS_KEY_LNCH_NAME_2);
    save_json_str(root, "lnch_name_3",     NVS_KEY_LNCH_NAME_3);
    save_json_str(root, "lnch_name_4",     NVS_KEY_LNCH_NAME_4);

    /* Weather (OWM) — configurati dal proxy Web UI */
    save_json_str(root, "owm_api_key",     NVS_KEY_WTH_APIKEY);
    save_json_str(root, "owm_lat",         NVS_KEY_WTH_LAT);
    save_json_str(root, "owm_lon",         NVS_KEY_WTH_LON);
    save_json_str(root, "owm_units",       NVS_KEY_WTH_UNITS);
    save_json_str(root, "owm_city_name",   NVS_KEY_WTH_CITY);
    save_json_str(root, "owm_location",   NVS_KEY_WTH_LOCATION);

    /* Beszel */
    save_json_str(root, "beszel_port",     NVS_KEY_BESZEL_PORT);

    cJSON_Delete(root);
    ESP_LOGI(TAG, "Config fetched and saved to NVS");
    return 0;
}

/* ─── Boot fetch task ──────────────────────────────────────────────────────── */

#define CFG_MAX_RETRIES    3
#define CFG_RETRY_DELAY_MS 2000

static void config_boot_fetch_task(void *arg)
{
    /* Delay iniziale: lascia stabilizzare lo stack IP e diluisce le connessioni al boot */
    vTaskDelay(pdMS_TO_TICKS(3000));

    for (int attempt = 0; attempt < CFG_MAX_RETRIES; attempt++) {
        int ret = config_fetch_from_proxy();
        if (ret == 0) {
            ESP_LOGI(TAG, "config fetch OK (attempt %d)", attempt + 1);
            break;
        }
        ESP_LOGW(TAG, "config fetch failed (attempt %d/%d), retry in %dms",
                 attempt + 1, CFG_MAX_RETRIES, CFG_RETRY_DELAY_MS);
        if (attempt < CFG_MAX_RETRIES - 1)
            vTaskDelay(pdMS_TO_TICKS(CFG_RETRY_DELAY_MS));
    }

    vTaskDelete(NULL);
}

static void ip_event_handler(void *arg, esp_event_base_t base,
                              int32_t id, void *data)
{
    if (id != IP_EVENT_STA_GOT_IP) return;
    if (s_boot_fetched) return;
    s_boot_fetched = true;

    /* Fetch in task separato — non blocca l'event handler */
    xTaskCreate(config_boot_fetch_task, "cfg_fetch", 4096, NULL, 5, NULL);
}

/* ─── Public init ──────────────────────────────────────────────────────────── */

void indicator_config_init(void)
{
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                ip_event_handler, NULL);
    ESP_LOGI(TAG, "indicator_config_init: boot fetch registrato");
}
