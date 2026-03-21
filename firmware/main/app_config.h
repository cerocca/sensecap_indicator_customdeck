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

/* ── Server Sibilla ─────────────────────────────────────────── */
#define APP_CFG_SERVER_IP       "192.168.1.69"
#define APP_CFG_SERVER_PORT     "61208"

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

#define NVS_KEY_SERVER_IP       "srv_ip"
#define NVS_KEY_SERVER_PORT     "srv_port"

#define NVS_KEY_PROXY_IP        "proxy_ip"
#define NVS_KEY_PROXY_PORT      "proxy_port"

#define NVS_KEY_AI_API_KEY      "ai_api_key"
#define NVS_KEY_AI_ENDPOINT     "ai_endpoint"
