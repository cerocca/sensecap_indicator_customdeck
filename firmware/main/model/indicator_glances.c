#include "indicator_glances.h"
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
#include <inttypes.h>

static const char *TAG = "GLANCES";

/* HTTP response buffer (min 2048 — CLAUDE.md regola 5) */
#define GLANCES_BUF_SIZE  2048
/* Buffer dedicato per /containers — risposta ~4 KB, margine a 16 KB */
#define GLANCES_CONT_BUF_SIZE 32768
#define GLANCES_POLL_MS   10000

/* ─── Internal state ──────────────────────────────────────────────────────── */

static char              s_server_ip[64];
static char              s_server_port[8];
static char              s_proxy_ip[64];
static char              s_proxy_port[8];
static bool              s_task_started  = false;
static volatile bool     s_poll_running  = false;
static glances_data_cb_t s_data_cb       = NULL;

/* Buffer containers: allocato in PSRAM per non consumare DRAM statica */
static char *s_cont_buf = NULL;

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

static void glances_read_config(void)
{
    nvs_read_str(NVS_KEY_SERVER_IP,   s_server_ip,   sizeof(s_server_ip),
                 APP_CFG_SERVER_IP);
    nvs_read_str(NVS_KEY_SERVER_PORT, s_server_port, sizeof(s_server_port),
                 APP_CFG_SERVER_PORT);
    nvs_read_str(NVS_KEY_PROXY_IP,   s_proxy_ip,   sizeof(s_proxy_ip),
                 APP_CFG_PROXY_IP);
    nvs_read_str(NVS_KEY_PROXY_PORT, s_proxy_port, sizeof(s_proxy_port),
                 APP_CFG_PROXY_PORT);
}

/* ─── HTTP GET helper ─────────────────────────────────────────────────────── */

static int glances_http_get(const char *url, char *buf, int buf_size)
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

    /* Leggi risposta — esp_http_client_read non fa loop automatico */
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

/* ─── Parse helpers ───────────────────────────────────────────────────────── */

static float parse_float_key(const char *json, const char *key)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) return -1.0f;
    cJSON *val = cJSON_GetObjectItem(root, key);
    float result = -1.0f;
    if (cJSON_IsNumber(val)) result = (float)val->valuedouble;
    cJSON_Delete(root);
    return result;
}

/*
 * Parsa la risposta di GET /docker dal proxy Mac.
 * Formato: array JSON già ordinato per mem_mb desc:
 *   [{"name": "uptime-kuma", "mem_mb": 203.73}, ...]
 * Prende i primi 5 elementi (proxy pre-ordina e pre-filtra).
 */
static void parse_top_docker(const char *json, glances_proc_t top[5])
{
    memset(top, 0, 5 * sizeof(glances_proc_t));

    cJSON *root = cJSON_Parse(json);
    if (!root || !cJSON_IsArray(root)) {
        if (root) cJSON_Delete(root);
        return;
    }

    int n = cJSON_GetArraySize(root);
    int filled = 0;
    for (int i = 0; i < n && filled < 5; i++) {
        cJSON *item   = cJSON_GetArrayItem(root, i);
        if (!item) continue;
        cJSON *name   = cJSON_GetObjectItem(item, "name");
        cJSON *mem_mb = cJSON_GetObjectItem(item, "mem_mb");
        if (!cJSON_IsString(name) || !cJSON_IsNumber(mem_mb)) continue;
        strncpy(top[filled].name, name->valuestring,
                sizeof(top[filled].name) - 1);
        top[filled].name[sizeof(top[filled].name) - 1] = '\0';
        top[filled].mem_mb = (uint32_t)mem_mb->valuedouble;
        filled++;
    }

    cJSON_Delete(root);
}

/* ─── Poll task (loop permanente ogni 10 s) ───────────────────────────────── */

static void glances_poll_task(void *arg)
{
    /* Alloca buffer containers in PSRAM — 32KB sarebbero troppi in DRAM statica */
    if (!s_cont_buf) {
        s_cont_buf = heap_caps_malloc(GLANCES_CONT_BUF_SIZE,
                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_cont_buf) {
            ESP_LOGE(TAG, "heap_caps_malloc cont_buf fallito — task terminato");
            vTaskDelete(NULL);
            return;
        }
    }

    /* Attende stabilizzazione stack IP e config fetch — CLAUDE.md pattern */
    vTaskDelay(pdMS_TO_TICKS(3000));

    /* static: evita pressione sullo stack del task — CLAUDE.md regola 6 */
    static char           s_buf[GLANCES_BUF_SIZE];
    static char           s_url[128];
    static char           s_uptime[64];
    static glances_proc_t s_top_docker[5];

    while (1) {
        if (!s_poll_running) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        glances_read_config();

        if (s_server_ip[0] == '\0') {
            ESP_LOGD(TAG, "SERVER_IP non configurato, skip poll");
            vTaskDelay(pdMS_TO_TICKS(GLANCES_POLL_MS));
            continue;
        }

        float cpu   = -1.0f;
        float ram   = -1.0f;
        float dsk   = -1.0f;
        float load1 = -1.0f, load5 = -1.0f, load15 = -1.0f;
        s_uptime[0] = '\0';
        memset(s_top_docker, 0, sizeof(s_top_docker));

        /* ── CPU ── */
        snprintf(s_url, sizeof(s_url), "http://%s:%s/api/4/cpu",
                 s_server_ip, s_server_port);
        if (glances_http_get(s_url, s_buf, sizeof(s_buf)) > 0)
            cpu = parse_float_key(s_buf, "total");

        /* ── RAM ── */
        snprintf(s_url, sizeof(s_url), "http://%s:%s/api/4/mem",
                 s_server_ip, s_server_port);
        if (glances_http_get(s_url, s_buf, sizeof(s_buf)) > 0)
            ram = parse_float_key(s_buf, "percent");

        /* ── Disk — primo elemento dell'array ── */
        snprintf(s_url, sizeof(s_url), "http://%s:%s/api/4/fs",
                 s_server_ip, s_server_port);
        if (glances_http_get(s_url, s_buf, sizeof(s_buf)) > 0) {
            cJSON *root = cJSON_Parse(s_buf);
            if (root) {
                cJSON *first = cJSON_IsArray(root)
                               ? cJSON_GetArrayItem(root, 0) : NULL;
                if (first) {
                    cJSON *pct = cJSON_GetObjectItem(first, "percent");
                    if (cJSON_IsNumber(pct))
                        dsk = (float)pct->valuedouble;
                }
                cJSON_Delete(root);
            }
        }

        /* ── Uptime — risposta è una stringa JSON: "1 day, 2:34:56" ── */
        snprintf(s_url, sizeof(s_url), "http://%s:%s/api/4/uptime",
                 s_server_ip, s_server_port);
        if (glances_http_get(s_url, s_buf, sizeof(s_buf)) > 0) {
            cJSON *root = cJSON_Parse(s_buf);
            if (root) {
                if (cJSON_IsString(root)) {
                    strncpy(s_uptime, root->valuestring, sizeof(s_uptime) - 1);
                    s_uptime[sizeof(s_uptime) - 1] = '\0';
                }
                cJSON_Delete(root);
            }
        }

        /* ── Load avg ── */
        snprintf(s_url, sizeof(s_url), "http://%s:%s/api/4/load",
                 s_server_ip, s_server_port);
        if (glances_http_get(s_url, s_buf, sizeof(s_buf)) > 0) {
            cJSON *root = cJSON_Parse(s_buf);
            if (root) {
                cJSON *m1  = cJSON_GetObjectItem(root, "min1");
                cJSON *m5  = cJSON_GetObjectItem(root, "min5");
                cJSON *m15 = cJSON_GetObjectItem(root, "min15");
                if (cJSON_IsNumber(m1))  load1  = (float)m1->valuedouble;
                if (cJSON_IsNumber(m5))  load5  = (float)m5->valuedouble;
                if (cJSON_IsNumber(m15)) load15 = (float)m15->valuedouble;
                cJSON_Delete(root);
            }
        }

        /* ── Containers — top 3 via proxy Mac /docker (Beszel) ── */
        snprintf(s_url, sizeof(s_url), "http://%s:%s/docker",
                 s_proxy_ip, s_proxy_port);
        int cont_len = glances_http_get(s_url, s_cont_buf, GLANCES_CONT_BUF_SIZE);
        ESP_LOGI(TAG, "/docker fetch: len=%d url=%s", cont_len, s_url);
        if (cont_len > 0)
            parse_top_docker(s_cont_buf, s_top_docker);

        ESP_LOGI(TAG, "CPU=%.1f RAM=%.1f DSK=%.1f load=%.2f/%.2f/%.2f uptime=%s",
                 cpu, ram, dsk, load1, load5, load15, s_uptime);
        ESP_LOGI(TAG, "docker[0]=%s %.0fMB  [1]=%s %.0fMB  [2]=%s %.0fMB  [3]=%s %.0fMB  [4]=%s %.0fMB",
                 s_top_docker[0].name[0] ? s_top_docker[0].name : "(empty)", (float)s_top_docker[0].mem_mb,
                 s_top_docker[1].name[0] ? s_top_docker[1].name : "(empty)", (float)s_top_docker[1].mem_mb,
                 s_top_docker[2].name[0] ? s_top_docker[2].name : "(empty)", (float)s_top_docker[2].mem_mb,
                 s_top_docker[3].name[0] ? s_top_docker[3].name : "(empty)", (float)s_top_docker[3].mem_mb,
                 s_top_docker[4].name[0] ? s_top_docker[4].name : "(empty)", (float)s_top_docker[4].mem_mb);

        if (s_data_cb) {
            lv_port_sem_take();
            s_data_cb(cpu, ram, dsk, s_uptime, load1, load5, load15, s_top_docker);
            lv_port_sem_give();
        }

        vTaskDelay(pdMS_TO_TICKS(GLANCES_POLL_MS));
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
    xTaskCreate(glances_poll_task, "glances_poll", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "glances_poll_task avviato");
}

/* ─── Public API ──────────────────────────────────────────────────────────── */

void indicator_glances_set_callback(glances_data_cb_t cb)
{
    s_data_cb = cb;
}

void indicator_glances_start(void)
{
    s_poll_running = true;
}

void indicator_glances_stop(void)
{
    s_poll_running = false;
}

void indicator_glances_init(void)
{
    glances_read_config();
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                ip_event_handler, NULL);
    ESP_LOGI(TAG, "indicator_glances_init: handler IP registrato");
}
