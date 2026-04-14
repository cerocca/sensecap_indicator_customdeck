#include "indicator_uptime_kuma.h"
#include "indicator_storage.h"
#include "app_config.h"
#include "lv_port.h"        /* lv_port_sem_take / lv_port_sem_give */

#include "esp_http_client.h"
#include "esp_event.h"
#include "esp_wifi.h"       /* IP_EVENT, IP_EVENT_STA_GOT_IP */
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <stdio.h>

static const char *TAG = "UPTIME";

#define UK_BUF_SIZE  8192   /* /uptime può restituire >2KB con molti monitor */
#define UK_POLL_MS   30000

/* ─── Internal state ──────────────────────────────────────────────────────── */

static char              s_proxy_ip[64];
static char              s_proxy_port[8];
static bool              s_task_started = false;
static volatile bool     s_poll_running = false;
static uptime_kuma_cb_t  s_cb           = NULL;

/* Buffer /uptime allocato in PSRAM — risposta può superare 2KB con molti monitor */
static char *s_uk_buf = NULL;

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

static void uk_read_config(void)
{
    nvs_read_str(NVS_KEY_PROXY_IP,   s_proxy_ip,   sizeof(s_proxy_ip),
                 APP_CFG_PROXY_IP);
    nvs_read_str(NVS_KEY_PROXY_PORT, s_proxy_port, sizeof(s_proxy_port),
                 APP_CFG_PROXY_PORT);
}

/* ─── HTTP GET helper ─────────────────────────────────────────────────────── */

static int uk_http_get(const char *url, char *buf, int buf_size)
{
    esp_http_client_config_t cfg = {
        .url        = url,
        .method     = HTTP_METHOD_GET,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return -1;

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "open failed %s: %s", url, esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return -1;
    }

    esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        ESP_LOGW(TAG, "GET %s HTTP %d", url, status);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return -1;
    }

    int total = 0, r;
    while ((r = esp_http_client_read(client, buf + total,
                                     buf_size - 1 - total)) > 0) {
        total += r;
        if (total >= buf_size - 1) break;
    }
    buf[total] = '\0';

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return total;
}

/* ─── Poll task (loop permanente ogni 30 s) ───────────────────────────────── */

static void uk_poll_task(void *arg)
{
    /* Alloca buffer in PSRAM — 8KB evita pressione su DRAM statica */
    if (!s_uk_buf) {
        s_uk_buf = heap_caps_malloc(UK_BUF_SIZE,
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_uk_buf) {
            ESP_LOGE(TAG, "heap_caps_malloc uk_buf fallito — task terminato");
            vTaskDelete(NULL);
            return;
        }
    }

    /* Attende stabilizzazione: indicator_config (1.5s) + glances (3s) + margine */
    vTaskDelay(pdMS_TO_TICKS(8000));

    static char s_url[128];
    static char s_names_down[UK_MAX_DOWN][32];

    while (1) {
        ESP_LOGI(TAG, "uk_poll_task: loop vivo, running=%d proxy=%s:%s",
                 s_poll_running, s_proxy_ip, s_proxy_port);

        if (!s_poll_running) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        uk_read_config();

        if (s_proxy_ip[0] == '\0') {
            ESP_LOGD(TAG, "PROXY_IP non configurato, skip poll");
            vTaskDelay(pdMS_TO_TICKS(UK_POLL_MS));
            continue;
        }

        snprintf(s_url, sizeof(s_url), "http://%s:%s/uptime",
                 s_proxy_ip, s_proxy_port);

        int len = uk_http_get(s_url, s_uk_buf, UK_BUF_SIZE);

        int total = 0, up = 0, n_down = 0;
        memset(s_names_down, 0, sizeof(s_names_down));

        if (len > 0) {
            cJSON *root = cJSON_Parse(s_uk_buf);
            if (root && cJSON_IsArray(root)) {
                int n = cJSON_GetArraySize(root);
                for (int i = 0; i < n; i++) {
                    cJSON *item  = cJSON_GetArrayItem(root, i);
                    if (!item) continue;
                    cJSON *name  = cJSON_GetObjectItem(item, "name");
                    cJSON *up_j  = cJSON_GetObjectItem(item, "up");
                    if (!cJSON_IsString(name) || !cJSON_IsBool(up_j)) continue;
                    /* Escludi intestazioni gruppo (es. "0-Infra") */
                    if (strncmp(name->valuestring, "0-", 2) == 0) continue;
                    total++;
                    if (cJSON_IsTrue(up_j)) {
                        up++;
                    } else if (n_down < UK_MAX_DOWN) {
                        strncpy(s_names_down[n_down], name->valuestring, 31);
                        s_names_down[n_down][31] = '\0';
                        n_down++;
                    }
                }
                cJSON_Delete(root);
            } else {
                if (root) cJSON_Delete(root);
                ESP_LOGW(TAG, "risposta non è un array JSON valido (len=%d)", len);
            }
        }

        ESP_LOGI(TAG, "%d/%d UP, %d DOWN", up, total, n_down);

        if (s_cb) {
            lv_port_sem_take();
            s_cb(total, up, (const char (*)[32])s_names_down, n_down);
            lv_port_sem_give();
        }

        vTaskDelay(pdMS_TO_TICKS(UK_POLL_MS));
    }
}

/* ─── IP event → avvio task ───────────────────────────────────────────────── */

static void ip_event_handler(void *arg, esp_event_base_t base,
                              int32_t id, void *data)
{
    if (id != IP_EVENT_STA_GOT_IP) return;
    if (s_task_started) return;
    s_task_started = true;
    s_poll_running = true;
    xTaskCreate(uk_poll_task, "uptime_kuma", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "uptime_kuma task avviato");
}

/* ─── Public API ──────────────────────────────────────────────────────────── */

void indicator_uptime_kuma_set_callback(uptime_kuma_cb_t cb)
{
    s_cb = cb;
}

void indicator_uptime_kuma_start(void)
{
    s_poll_running = true;
}

void indicator_uptime_kuma_stop(void)
{
    s_poll_running = false;
}

void indicator_uptime_kuma_init(void)
{
    uk_read_config();
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                ip_event_handler, NULL);
    ESP_LOGI(TAG, "indicator_uptime_kuma_init: handler IP registrato");
}
