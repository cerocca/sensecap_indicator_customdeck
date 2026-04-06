#pragma once

/*
 * Valori di default (fallback se NVS vuoto).
 * La schermata Settings li sovrascrive a runtime, salvando in NVS.
 */

/* ── Hue Bridge ─────────────────────────────────────────────── */
#define APP_CFG_HUE_BRIDGE_IP   "192.168.1.100"
#define APP_CFG_HUE_API_KEY     ""
#define APP_CFG_HUE_LIGHT_1     "Light 1"
#define APP_CFG_HUE_LIGHT_2     "Light 2"
#define APP_CFG_HUE_LIGHT_3     "Light 3"
#define APP_CFG_HUE_LIGHT_4     "Light 4"
#define APP_CFG_HUE_LIGHT_1_ID  ""
#define APP_CFG_HUE_LIGHT_2_ID  ""
#define APP_CFG_HUE_LIGHT_3_ID  ""
#define APP_CFG_HUE_LIGHT_4_ID  ""

/* ── Server LocalServer ─────────────────────────────────────── */
#define APP_CFG_SERVER_IP       "192.168.1.69"
#define APP_CFG_SERVER_PORT     "61208"
#define APP_CFG_SERVER_NAME     "LocalServer"

/* ── Proxy Mac ──────────────────────────────────────────────── */
#define APP_CFG_PROXY_IP        "192.168.1.70"
#define APP_CFG_PROXY_PORT      "8765"

/* ── Claude AI ──────────────────────────────────────────────── */
#define APP_CFG_AI_API_KEY      ""
#define APP_CFG_AI_ENDPOINT     "https://api.anthropic.com"

/* ── NVS keys (max 15 chars ciascuna) ──────────────────────── */
#define NVS_KEY_HUE_IP          "hue_ip"
#define NVS_KEY_HUE_API_KEY     "hue_api_key"
#define NVS_KEY_HUE_LIGHT_1     "hue_l1"
#define NVS_KEY_HUE_LIGHT_2     "hue_l2"
#define NVS_KEY_HUE_LIGHT_3     "hue_l3"
#define NVS_KEY_HUE_LIGHT_4     "hue_l4"
#define NVS_KEY_HUE_LIGHT_1_ID  "hue_l1_id"
#define NVS_KEY_HUE_LIGHT_2_ID  "hue_l2_id"
#define NVS_KEY_HUE_LIGHT_3_ID  "hue_l3_id"
#define NVS_KEY_HUE_LIGHT_4_ID  "hue_l4_id"

#define NVS_KEY_SERVER_IP       "srv_ip"
#define NVS_KEY_SERVER_PORT     "srv_port"
#define NVS_KEY_SERVER_NAME     "srv_name"      /* nome display — Settings/proxy */
#define NVS_KEY_GLANCES_HOST    "glances_host"  /* hostname da Glances (indicator_system.c) */

#define NVS_KEY_PROXY_IP        "proxy_ip"
#define NVS_KEY_PROXY_PORT      "proxy_port"

#define NVS_KEY_AI_API_KEY      "ai_api_key"
#define NVS_KEY_AI_ENDPOINT     "ai_endpoint"

/* ── Launcher URLs ──────────────────────────────────────────── */
#define APP_CFG_LNCH_URL_1      "https://github.com/cerocca/"
#define APP_CFG_LNCH_URL_2      "https://www.strava.com"
#define APP_CFG_LNCH_URL_3      "https://connect.garmin.com/"
#define APP_CFG_LNCH_URL_4      "https://intervals.icu/"

#define NVS_KEY_LNCH_URL_1      "lnch_url_1"
#define NVS_KEY_LNCH_URL_2      "lnch_url_2"
#define NVS_KEY_LNCH_URL_3      "lnch_url_3"
#define NVS_KEY_LNCH_URL_4      "lnch_url_4"

/* ── Launcher Names (label pulsante, separato dall'URL) ─────── */
#define APP_CFG_LNCH_NAME_1     "GitHub"
#define APP_CFG_LNCH_NAME_2     "Strava"
#define APP_CFG_LNCH_NAME_3     "Garmin"
#define APP_CFG_LNCH_NAME_4     "Intervals"

#define NVS_KEY_LNCH_NAME_1     "lnch_name_1"
#define NVS_KEY_LNCH_NAME_2     "lnch_name_2"
#define NVS_KEY_LNCH_NAME_3     "lnch_name_3"
#define NVS_KEY_LNCH_NAME_4     "lnch_name_4"

/* ── Weather (OWM) ──────────────────────────────────────────── */
/* Nota: API key, lat, lon configurati dal proxy Web UI → NVS via /config */
#define NVS_KEY_WTH_APIKEY      "wth_api_key"   /* OWM API key (32 char) */
#define NVS_KEY_WTH_LAT         "wth_lat"        /* es. "43.7711"         */
#define NVS_KEY_WTH_LON         "wth_lon"        /* es. "11.2486"         */
#define NVS_KEY_WTH_UNITS       "wth_units"      /* "metric" | "imperial" */
#define NVS_KEY_WTH_CITY        "wth_city"       /* nome città (display)  */
#define NVS_KEY_WTH_LOCATION    "wth_location"   /* es. "Firenze, IT"     */
#define DEFAULT_WTH_UNITS       "metric"

/* ── Beszel ─────────────────────────────────────────────────── */
#define NVS_KEY_BESZEL_PORT     "beszel_port"    /* porta Beszel dashboard */
#define DEFAULT_BESZEL_PORT     "8090"

/* ── Uptime Kuma ────────────────────────────────────────────── */
#define NVS_KEY_UK_PORT         "uk_port"        /* porta Uptime Kuma */
#define DEFAULT_UK_PORT         "3001"

/* ── Screen enable flags (default: 1 = enabled) ────────────── */
#define NVS_KEY_SCR_HUE_EN      "scr_hue_en"
#define NVS_KEY_SCR_SRV_EN      "scr_srv_en"
#define NVS_KEY_SCR_LNCH_EN     "scr_lnch_en"
#define NVS_KEY_SCR_AI_EN       "scr_ai_en"     /* mantenuto per compatibilità NVS */
#define NVS_KEY_SCR_WTHR        "scr_wthr_en"
#define DEFAULT_SCR_WTHR        "1"
#define NVS_KEY_SCR_DEFSENS     "scr_defsens"   /* auto-naviga a sensors al boot */
#define DEFAULT_SCR_DEFSENS     "1"
#define NVS_KEY_SCR_TRAFFIC_EN  "scr_traffic_en"
#define DEFAULT_SCR_TRAFFIC_EN  "1"

/* ── Traffic (Google Maps Distance Matrix via proxy) ────────── */
#define TRAFFIC_POLL_MS         600000   /* 10 minuti */
#define TRAFFIC_FIRST_DELAY_MS  8000     /* 8s dopo boot */

