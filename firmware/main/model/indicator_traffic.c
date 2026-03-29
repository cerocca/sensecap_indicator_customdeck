#include "indicator_traffic.h"
#include "screen_traffic.h"
#include "indicator_storage.h"
#include "app_config.h"
#include "lv_port.h"        /* lv_port_sem_take / lv_port_sem_give */

#include "esp_http_client.h"
#include "esp_event.h"
#include "esp_wifi.h"       /* IP_EVENT, IP_EVENT_STA_GOT_IP */
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <stdio.h>

static const char *TAG = "TRAFFIC";

#define TRAFFIC_BUF_SIZE 256

/* ─── Global state ────────────────────────────────────────────────────────── */

traffic_data_t g_traffic = {0};

static bool s_started = false;

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

/* ─── HTTP GET helper ─────────────────────────────────────────────────────── */

static int traffic_http_get(const char *url, char *buf, int buf_size)
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

    /* esp_http_client_read non fa loop automatico — CLAUDE.md nota critica */
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

/* ─── Poll task (loop permanente ogni TRAFFIC_POLL_MS) ───────────────────── */

static void traffic_poll_task(void *arg)
{
    ESP_LOGI(TAG, "task started");

    /* static: evita pressione sullo stack del task — CLAUDE.md regola 6 */
    static char s_proxy_ip[64];
    static char s_proxy_port[8];
    static char s_buf[TRAFFIC_BUF_SIZE];
    static char s_url[128];

    vTaskDelay(pdMS_TO_TICKS(TRAFFIC_FIRST_DELAY_MS));

    while (1) {
        nvs_read_str(NVS_KEY_PROXY_IP,   s_proxy_ip,   sizeof(s_proxy_ip),
                     APP_CFG_PROXY_IP);
        nvs_read_str(NVS_KEY_PROXY_PORT, s_proxy_port, sizeof(s_proxy_port),
                     APP_CFG_PROXY_PORT);

        snprintf(s_url, sizeof(s_url), "http://%s:%s/traffic",
                 s_proxy_ip, s_proxy_port);

        ESP_LOGI(TAG, "polling %s", s_url);

        /* Passare costante esplicita, mai sizeof(ptr) — CLAUDE.md regola buffer */
        int len = traffic_http_get(s_url, s_buf, TRAFFIC_BUF_SIZE);
        if (len > 0) {
            if (strstr(s_buf, "\"error\"")) {
                g_traffic.valid = false;
                ESP_LOGW(TAG, "traffic error response: %s", s_buf);
            } else {
                cJSON *root = cJSON_Parse(s_buf);
                if (root) {
                    cJSON *dur     = cJSON_GetObjectItem(root, "duration_sec");
                    cJSON *dur_nor = cJSON_GetObjectItem(root, "duration_normal_sec");
                    cJSON *delta   = cJSON_GetObjectItem(root, "delta_sec");
                    cJSON *dist    = cJSON_GetObjectItem(root, "distance_m");
                    cJSON *sts     = cJSON_GetObjectItem(root, "status");

                    if (cJSON_IsNumber(dur)     && cJSON_IsNumber(dur_nor) &&
                        cJSON_IsNumber(delta)   && cJSON_IsNumber(dist)    &&
                        cJSON_IsString(sts)) {
                        g_traffic.duration_sec        = (int)dur->valuedouble;
                        g_traffic.duration_normal_sec = (int)dur_nor->valuedouble;
                        g_traffic.delta_sec           = (int)delta->valuedouble;
                        g_traffic.distance_m          = (int)dist->valuedouble;
                        strncpy(g_traffic.status, sts->valuestring,
                                sizeof(g_traffic.status) - 1);
                        g_traffic.status[sizeof(g_traffic.status) - 1] = '\0';
                        g_traffic.valid          = true;
                        g_traffic.last_update_ms = esp_timer_get_time() / 1000LL;
                        ESP_LOGI(TAG,
                                 "traffic: %ds (normal %ds, delta %ds, dist %dm, %s)",
                                 g_traffic.duration_sec,
                                 g_traffic.duration_normal_sec,
                                 g_traffic.delta_sec,
                                 g_traffic.distance_m,
                                 g_traffic.status);
                    } else {
                        g_traffic.valid = false;
                        ESP_LOGW(TAG, "traffic: incomplete JSON fields");
                    }
                    cJSON_Delete(root);
                } else {
                    g_traffic.valid = false;
                    ESP_LOGW(TAG, "traffic: JSON parse error");
                }
            }

            /* Aggiornamento UI — sempre con lv_port_sem_take/give — CLAUDE.md regola 4 */
            lv_port_sem_take();
            screen_traffic_update();
            lv_port_sem_give();
        } else {
            ESP_LOGW(TAG, "traffic: HTTP GET failed");
        }

        vTaskDelay(pdMS_TO_TICKS(TRAFFIC_POLL_MS));
    }
}

/* ─── IP event → avvio task (una sola volta) ─────────────────────────────── */

static void on_got_ip(void *arg, esp_event_base_t base,
                      int32_t id, void *data)
{
    ESP_LOGI(TAG, "got IP, starting task");
    if (s_started) return;
    s_started = true;
    xTaskCreate(traffic_poll_task, "traffic_poll", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "traffic_poll_task avviato");
}

/* ─── Public API ──────────────────────────────────────────────────────────── */

void indicator_traffic_init(void)
{
    ESP_LOGI(TAG, "init called");
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_got_ip, NULL);
}
