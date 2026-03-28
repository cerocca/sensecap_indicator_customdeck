#include "indicator_weather.h"
#include "indicator_storage.h"
#include "app_config.h"

#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"      /* esp_timer_get_time */
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "WEATHER";

/* Risposta OWM current: ~700 byte; forecast cnt=4: ~1500 byte → 2048 ok */
#define WEATHER_BUF_SIZE  2048
/* Intervallo poll: 10 minuti */
#define WEATHER_POLL_MS   600000
/* Prima poll dopo boot: 8s (lascia tempo al Wi-Fi di connettersi) */
#define WEATHER_FIRST_DELAY_MS 8000

/* ─── Internal state ──────────────────────────────────────────────────────── */

static weather_data_t s_weather;           /* static — regola #6 CLAUDE.md */

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

/* ─── Icon code → ASCII short text ──────────────────────────────────────── */

const char *weather_icon_label(const char *code)
{
    if (!code || code[0] == '\0') return "---";
    /* OWM code: 2 chars + 'd'/'n', es. "01d", "10n" */
    int n = (int)((code[0] - '0') * 10 + (code[1] - '0'));
    switch (n) {
        case  1: return (code[2] == 'n') ? "MOON" : "SUN";
        case  2: return "P.CLO";
        case  3: return "CLO";
        case  4: return "COV";
        case  9: return "RAIN";
        case 10: return "RAIN";
        case 11: return "STOR";
        case 13: return "SNOW";
        case 50: return "FOG";
        default: return "---";
    }
}

/* ─── Parse helpers ──────────────────────────────────────────────────────── */

/*
 * Estrae l'ora (0-23) dalla stringa dt_txt OWM: "YYYY-MM-DD HH:MM:SS".
 * Se il formato non corrisponde ritorna 0.
 */
static int parse_dt_hour(const char *dt_txt)
{
    if (!dt_txt || strlen(dt_txt) < 13) return 0;
    int h = (dt_txt[11] - '0') * 10 + (dt_txt[12] - '0');
    return (h >= 0 && h <= 23) ? h : 0;
}

/* ─── HTTPS GET helper ───────────────────────────────────────────────────── */

static int weather_https_get(const char *url, char *buf, int buf_size)
{
    esp_http_client_config_t cfg = {
        .url                     = url,
        .method                  = HTTP_METHOD_GET,
        .timeout_ms              = 8000,
        /* Nessuna verifica certificato — stesso pattern indicator_hue.c */
        .skip_cert_common_name_check = true,
        .use_global_ca_store         = false,
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

/* ─── Poll task ──────────────────────────────────────────────────────────── */

static void weather_poll_task(void *arg)
{
    /* Attende stabilizzazione stack IP e config boot fetch */
    vTaskDelay(pdMS_TO_TICKS(WEATHER_FIRST_DELAY_MS));

    /* static: evita pressione sullo stack del task — CLAUDE.md regola 6 */
    static char s_buf[WEATHER_BUF_SIZE];
    static char s_url[256];
    static char s_apikey[48];   /* OWM key: 32 char */
    static char s_lat[24];
    static char s_lon[24];
    static char s_units[12];

    while (1) {
        /* Leggi config da NVS ad ogni ciclo */
        nvs_read_str(NVS_KEY_WTH_APIKEY, s_apikey, sizeof(s_apikey), "");
        nvs_read_str(NVS_KEY_WTH_LAT,    s_lat,    sizeof(s_lat),    "");
        nvs_read_str(NVS_KEY_WTH_LON,    s_lon,    sizeof(s_lon),    "");
        nvs_read_str(NVS_KEY_WTH_UNITS,  s_units,  sizeof(s_units),  DEFAULT_WTH_UNITS);

        if (s_apikey[0] == '\0' || s_lat[0] == '\0' || s_lon[0] == '\0') {
            ESP_LOGD(TAG, "OWM API key o coordinate non configurate, skip poll");
            s_weather.valid = false;
            vTaskDelay(pdMS_TO_TICKS(WEATHER_POLL_MS));
            continue;
        }

        bool is_metric = (strcmp(s_units, "imperial") != 0);

        /* ── Current weather ── */
        snprintf(s_url, sizeof(s_url),
                 "https://api.openweathermap.org/data/2.5/weather"
                 "?lat=%s&lon=%s&appid=%s&units=%s",
                 s_lat, s_lon, s_apikey, s_units);

        bool current_ok = false;
        if (weather_https_get(s_url, s_buf, WEATHER_BUF_SIZE) > 0) {
            cJSON *root = cJSON_Parse(s_buf);
            if (root) {
                /* Verifica campo "cod" per errore OWM */
                cJSON *cod = cJSON_GetObjectItem(root, "cod");
                if (!cod || (cJSON_IsNumber(cod) && (int)cod->valuedouble == 200)
                    || (cJSON_IsString(cod) && strcmp(cod->valuestring, "200") == 0)) {

                    cJSON *main_obj = cJSON_GetObjectItem(root, "main");
                    cJSON *weather  = cJSON_GetArrayItem(cJSON_GetObjectItem(root, "weather"), 0);
                    cJSON *wind_obj = cJSON_GetObjectItem(root, "wind");

                    if (main_obj && weather) {
                        cJSON *temp   = cJSON_GetObjectItem(main_obj, "temp");
                        cJSON *feels  = cJSON_GetObjectItem(main_obj, "feels_like");
                        cJSON *hum    = cJSON_GetObjectItem(main_obj, "humidity");
                        cJSON *desc   = cJSON_GetObjectItem(weather, "description");
                        cJSON *icon   = cJSON_GetObjectItem(weather, "icon");
                        cJSON *wspeed = cJSON_GetObjectItem(wind_obj, "speed");

                        if (cJSON_IsNumber(temp))  s_weather.temp       = (float)temp->valuedouble;
                        if (cJSON_IsNumber(feels)) s_weather.feels_like = (float)feels->valuedouble;
                        if (cJSON_IsNumber(hum))   s_weather.humidity   = (int)hum->valuedouble;
                        if (cJSON_IsString(desc)) {
                            strncpy(s_weather.desc, desc->valuestring, sizeof(s_weather.desc) - 1);
                            s_weather.desc[sizeof(s_weather.desc) - 1] = '\0';
                            /* Prima lettera maiuscola */
                            if (s_weather.desc[0] >= 'a' && s_weather.desc[0] <= 'z')
                                s_weather.desc[0] = (char)(s_weather.desc[0] - 32);
                        }
                        if (cJSON_IsString(icon)) {
                            strncpy(s_weather.icon, icon->valuestring, sizeof(s_weather.icon) - 1);
                            s_weather.icon[sizeof(s_weather.icon) - 1] = '\0';
                        }
                        if (cJSON_IsNumber(wspeed)) {
                            float spd = (float)wspeed->valuedouble;
                            s_weather.wind_kph = is_metric ? spd * 3.6f : spd; /* m/s→km/h o mph */
                        }
                        s_weather.is_metric = is_metric;
                        current_ok = true;
                    }
                } else {
                    ESP_LOGW(TAG, "OWM current error: %s",
                             cJSON_IsString(cod) ? cod->valuestring : "?");
                }
                cJSON_Delete(root);
            }
        }

        /* ── Forecast (cnt=4 slot × 3h) ── */
        bool forecast_ok = false;
        snprintf(s_url, sizeof(s_url),
                 "https://api.openweathermap.org/data/2.5/forecast"
                 "?lat=%s&lon=%s&appid=%s&units=%s&cnt=4",
                 s_lat, s_lon, s_apikey, s_units);

        if (weather_https_get(s_url, s_buf, WEATHER_BUF_SIZE) > 0) {
            cJSON *root = cJSON_Parse(s_buf);
            if (root) {
                cJSON *list = cJSON_GetObjectItem(root, "list");
                int n = list ? cJSON_GetArraySize(list) : 0;
                int filled = 0;
                for (int i = 0; i < n && filled < 3; i++) {
                    cJSON *slot    = cJSON_GetArrayItem(list, i);
                    if (!slot) continue;
                    cJSON *dt_txt  = cJSON_GetObjectItem(slot, "dt_txt");
                    cJSON *main_o  = cJSON_GetObjectItem(slot, "main");
                    cJSON *weather = cJSON_GetArrayItem(
                                        cJSON_GetObjectItem(slot, "weather"), 0);
                    if (!main_o || !weather) continue;
                    cJSON *temp    = cJSON_GetObjectItem(main_o, "temp");
                    cJSON *icon    = cJSON_GetObjectItem(weather, "icon");
                    if (!cJSON_IsNumber(temp) || !cJSON_IsString(icon)) continue;

                    s_weather.forecast[filled].hour = cJSON_IsString(dt_txt)
                        ? parse_dt_hour(dt_txt->valuestring) : 0;
                    s_weather.forecast[filled].temp = (float)temp->valuedouble;
                    strncpy(s_weather.forecast[filled].icon,
                            icon->valuestring,
                            sizeof(s_weather.forecast[filled].icon) - 1);
                    s_weather.forecast[filled].icon[
                        sizeof(s_weather.forecast[filled].icon) - 1] = '\0';
                    filled++;
                }
                forecast_ok = (filled > 0);
                cJSON_Delete(root);
            }
        }

        if (current_ok || forecast_ok) {
            s_weather.last_update_ms = esp_timer_get_time() / 1000LL;
            s_weather.valid = current_ok;
            ESP_LOGI(TAG, "meteo: %.1f%s H:%d W:%.1f%s %s",
                     s_weather.temp, s_weather.is_metric ? "°C" : "°F",
                     s_weather.humidity,
                     s_weather.wind_kph, s_weather.is_metric ? "km/h" : "mph",
                     s_weather.desc);
        } else {
            s_weather.valid = false;
            ESP_LOGW(TAG, "fetch fallito — dati non aggiornati");
        }

        vTaskDelay(pdMS_TO_TICKS(WEATHER_POLL_MS));
    }
}

/* ─── Public API ─────────────────────────────────────────────────────────── */

void indicator_weather_init(void)
{
    memset(&s_weather, 0, sizeof(s_weather));
    xTaskCreate(weather_poll_task, "weather_poll", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "indicator_weather_init: weather_poll_task avviato");
}

const weather_data_t *indicator_weather_get(void)
{
    return &s_weather;
}
