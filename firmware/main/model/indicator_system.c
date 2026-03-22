#include "indicator_system.h"
#include "indicator_storage.h"
#include "app_config.h"

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

static const char *TAG = "indicator_system";

#define SYS_BUF_SIZE 2048

static bool s_fetched = false;

/* ─── NVS helpers ─────────────────────────────────────────────────────────── */

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

/* ─── Fetch task ──────────────────────────────────────────────────────────── */

static void system_fetch_task(void *arg)
{
    /* Attende che indicator_config abbia già completato il fetch config (~1.5s)
     * e scritto in NVS i valori di SERVER_IP e SERVER_PORT. */
    vTaskDelay(pdMS_TO_TICKS(5000));

    /* Legge server IP e porta Glances da NVS */
    char srv_ip[64];
    char srv_port[8];
    nvs_read_str(NVS_KEY_SERVER_IP,   srv_ip,   sizeof(srv_ip),   APP_CFG_SERVER_IP);
    nvs_read_str(NVS_KEY_SERVER_PORT, srv_port, sizeof(srv_port), APP_CFG_SERVER_PORT);

    if (srv_ip[0] == '\0') {
        ESP_LOGW(TAG, "server IP non configurato, skip");
        vTaskDelete(NULL);
        return;
    }

    char url[128];
    snprintf(url, sizeof(url), "http://%s:%s/api/4/system", srv_ip, srv_port);
    ESP_LOGI(TAG, "GET %s", url);

    /* Buffer sulla heap — CLAUDE.md regola 5 (min 2048) */
    char *buf = malloc(SYS_BUF_SIZE);
    if (!buf) {
        ESP_LOGE(TAG, "malloc failed");
        vTaskDelete(NULL);
        return;
    }
    memset(buf, 0, SYS_BUF_SIZE);

    esp_http_client_config_t cfg = {
        .url        = url,
        .method     = HTTP_METHOD_GET,
        .timeout_ms = 5000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        free(buf);
        vTaskDelete(NULL);
        return;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        free(buf);
        vTaskDelete(NULL);
        return;
    }

    esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        ESP_LOGW(TAG, "HTTP %d", status);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        free(buf);
        vTaskDelete(NULL);
        return;
    }

    int total = 0, r;
    while ((r = esp_http_client_read(client, buf + total,
                                     SYS_BUF_SIZE - 1 - total)) > 0) {
        total += r;
        if (total >= SYS_BUF_SIZE - 1) break;
    }
    buf[total] = '\0';

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (total == 0) {
        ESP_LOGW(TAG, "risposta vuota");
        free(buf);
        vTaskDelete(NULL);
        return;
    }

    /* Parsa hostname */
    cJSON *root = cJSON_Parse(buf);
    free(buf);

    if (!root) {
        ESP_LOGE(TAG, "JSON parse error");
        vTaskDelete(NULL);
        return;
    }

    cJSON *hostname = cJSON_GetObjectItem(root, "hostname");
    if (cJSON_IsString(hostname) && hostname->valuestring &&
        hostname->valuestring[0] != '\0') {

        /* Sovrascrive solo se NVS è vuoto o contiene il default "LocalServer".
         * Un nome personalizzato impostato dall'utente non viene mai toccato. */
        char current[32] = {0};
        size_t cur_len = sizeof(current);
        bool is_default = true;
        if (indicator_storage_read((char *)NVS_KEY_SERVER_NAME, current, &cur_len) == 0
                && cur_len > 1) {
            current[sizeof(current) - 1] = '\0';
            is_default = (strcmp(current, APP_CFG_SERVER_NAME) == 0);
        }

        if (is_default) {
            indicator_storage_write((char *)NVS_KEY_SERVER_NAME,
                                    hostname->valuestring,
                                    strlen(hostname->valuestring) + 1);
            ESP_LOGI(TAG, "hostname salvato in NVS: %s", hostname->valuestring);
        } else {
            ESP_LOGI(TAG, "nome server personalizzato (%s), hostname Glances ignorato", current);
        }
    } else {
        ESP_LOGD(TAG, "hostname vuoto o assente, NVS invariato");
    }

    cJSON_Delete(root);
    vTaskDelete(NULL);
}

/* ─── IP event ────────────────────────────────────────────────────────────── */

static void ip_event_handler(void *arg, esp_event_base_t base,
                              int32_t id, void *data)
{
    if (id != IP_EVENT_STA_GOT_IP) return;
    if (s_fetched) return;
    s_fetched = true;
    xTaskCreate(system_fetch_task, "sys_fetch", 4096, NULL, 4, NULL);
    ESP_LOGI(TAG, "system_fetch_task avviato");
}

/* ─── Public init ─────────────────────────────────────────────────────────── */

void indicator_system_init(void)
{
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                ip_event_handler, NULL);
    ESP_LOGI(TAG, "indicator_system_init: handler IP registrato");
}
