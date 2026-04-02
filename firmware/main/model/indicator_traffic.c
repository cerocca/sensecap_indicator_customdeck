#include "indicator_traffic.h"
#include "indicator_storage.h"
#include "app_config.h"

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

#define TRAFFIC_BUF_SIZE 512

#undef  TRAFFIC_FIRST_DELAY_MS
#define TRAFFIC_FIRST_DELAY_MS 3000   /* ridotto da 8000: traffic è leggero, 3s sufficienti */

/* ─── Global state ────────────────────────────────────────────────────────── */

traffic_data_t g_traffic = {0};

static bool          s_started             = false;
static volatile bool s_force_poll_requested = false;

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

/* ─── Parse un singolo elemento JSON route ────────────────────────────────── */

static bool parse_route(cJSON *elem, traffic_route_t *out)
{
    cJSON *name    = cJSON_GetObjectItem(elem, "name");
    cJSON *dur     = cJSON_GetObjectItem(elem, "duration_sec");
    cJSON *dur_nor = cJSON_GetObjectItem(elem, "duration_normal_sec");
    cJSON *delta   = cJSON_GetObjectItem(elem, "delta_sec");
    cJSON *dist    = cJSON_GetObjectItem(elem, "distance_m");
    cJSON *sts     = cJSON_GetObjectItem(elem, "status");

    if (!cJSON_IsNumber(dur) || !cJSON_IsNumber(dur_nor) ||
        !cJSON_IsNumber(delta) || !cJSON_IsNumber(dist) ||
        !cJSON_IsString(sts)) {
        return false;
    }

    if (cJSON_IsString(name) && name->valuestring) {
        strncpy(out->name, name->valuestring, sizeof(out->name) - 1);
        out->name[sizeof(out->name) - 1] = '\0';
    } else {
        out->name[0] = '\0';
    }
    out->duration_sec        = (int)dur->valuedouble;
    out->duration_normal_sec = (int)dur_nor->valuedouble;
    out->delta_sec           = (int)delta->valuedouble;
    out->distance_m          = (int)dist->valuedouble;
    strncpy(out->status, sts->valuestring, sizeof(out->status) - 1);
    out->status[sizeof(out->status) - 1] = '\0';
    return true;
}

/* ─── Shared poll logic ──────────────────────────────────────────────────── */

/* static: evita pressione sullo stack — CLAUDE.md regola 6.
 * traffic_poll_task (loop) e force_poll_task (one-shot) non sono mai
 * concorrenti: force_poll è avviato solo su richiesta manuale. */
static char s_proxy_ip[64];
static char s_proxy_port[8];
static char s_buf[TRAFFIC_BUF_SIZE];
static char s_url[128];

static void do_traffic_poll(void)
{
    nvs_read_str(NVS_KEY_PROXY_IP,   s_proxy_ip,   sizeof(s_proxy_ip),
                 APP_CFG_PROXY_IP);
    nvs_read_str(NVS_KEY_PROXY_PORT, s_proxy_port, sizeof(s_proxy_port),
                 APP_CFG_PROXY_PORT);

    snprintf(s_url, sizeof(s_url), "http://%s:%s/traffic",
             s_proxy_ip, s_proxy_port);
    ESP_LOGI(TAG, "polling %s", s_url);

    /* Passare costante esplicita, mai sizeof(ptr) — CLAUDE.md regola buffer */
    int len = traffic_http_get(s_url, s_buf, TRAFFIC_BUF_SIZE);
    if (len <= 0) {
        ESP_LOGW(TAG, "traffic: HTTP GET failed");
        return;
    }

    if (strstr(s_buf, "\"error\"")) {
        g_traffic.valid       = false;
        g_traffic.route_count = 0;
        ESP_LOGW(TAG, "traffic error response: %s", s_buf);
    } else {
        cJSON *root = cJSON_Parse(s_buf);
        if (root) {
            if (cJSON_IsArray(root)) {
                int count = 0;
                cJSON *elem;
                cJSON_ArrayForEach(elem, root) {
                    if (count >= 2) break;
                    if (parse_route(elem, &g_traffic.routes[count])) {
                        ESP_LOGI(TAG,
                                 "route[%d] \"%s\": %ds (normal %ds, delta %ds, %dm, %s)",
                                 count,
                                 g_traffic.routes[count].name,
                                 g_traffic.routes[count].duration_sec,
                                 g_traffic.routes[count].duration_normal_sec,
                                 g_traffic.routes[count].delta_sec,
                                 g_traffic.routes[count].distance_m,
                                 g_traffic.routes[count].status);
                        count++;
                    }
                }
                if (count > 0) {
                    g_traffic.route_count    = count;
                    g_traffic.valid          = true;
                    g_traffic.last_update_ms = esp_timer_get_time() / 1000LL;
                } else {
                    g_traffic.valid       = false;
                    g_traffic.route_count = 0;
                    ESP_LOGW(TAG, "traffic: array vuoto o parse failed");
                }
            } else {
                g_traffic.valid       = false;
                g_traffic.route_count = 0;
                ESP_LOGW(TAG, "traffic: risposta non e' un array");
            }
            cJSON_Delete(root);
        } else {
            g_traffic.valid       = false;
            g_traffic.route_count = 0;
            ESP_LOGW(TAG, "traffic: JSON parse error");
        }
    }

    for (int i = 0; i < g_traffic.route_count; i++) {
        ESP_LOGI(TAG, "route[%d] %s: %ds", i,
                 g_traffic.routes[i].name, g_traffic.routes[i].duration_sec);
    }
}

/* ─── Poll task (loop permanente ogni TRAFFIC_POLL_MS) ───────────────────── */

static void traffic_poll_task(void *arg)
{
    ESP_LOGI(TAG, "task started");
    vTaskDelay(pdMS_TO_TICKS(TRAFFIC_FIRST_DELAY_MS));
    while (1) {
        do_traffic_poll();
        for (int i = 0; i < (TRAFFIC_POLL_MS / 500); i++) {
            vTaskDelay(pdMS_TO_TICKS(500));
            if (s_force_poll_requested) break;
        }
        s_force_poll_requested = false;
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

void indicator_traffic_force_poll(void)
{
    s_force_poll_requested = true;
}
