# Changelog

## [Unreleased]

### Added
- `model/indicator_glances.c/.h`: modulo polling Glances API ogni 10s — CPU (`/api/4/cpu`), RAM (`/api/4/mem`), disco (`/api/4/fs`), uptime (`/api/4/uptime`), load avg (`/api/4/load`), top 3 container Docker per RAM (`/api/4/containers`); buffer containers (32KB) allocato in PSRAM (`heap_caps_malloc MALLOC_CAP_SPIRAM`); task avviato su `IP_EVENT_STA_GOT_IP`; callback UI registrata da `screen_sibilla_populate()`
- `model/indicator_uptime_kuma.c/.h`: polling proxy `/uptime` ogni 30s; parsing array `[{name, up}]`; esclude gruppi "0-..."; calcola totale/UP/DOWN (max 6); callback con LVGL sem; task avviato su `IP_EVENT_STA_GOT_IP` (8s delay)
- `sensedeck_proxy.py`: endpoint `/uptime` aggiornato — ora ritorna array `[{name, up}]` con nomi da `monitorList`, esclude gruppi "0-"; non più `{"monitors": [{id, status}]}`
- `ui/screen_sibilla.c`: implementazione completa con lazy populate — 3 barre metriche (CPU/RAM/DSK), uptime, load avg, separatore, header "Top Docker (RAM)", 3 righe container (nome a sinistra max 32 char + MB allineato a destra in label separato), label "Servizi: X/Y UP" (verde/rosso) + 6 righe DOWN pre-allocate aggiornate da Uptime Kuma; `format_uptime()` converte stringa raw Glances in formato compatto (1d 2h 34m)
- `model/indicator_system.c/.h`: fetch hostname da Glances `/api/4/system` al boot (5s delay, dopo indicator_config); sovrascrive `srv_name` in NVS solo se vuoto o uguale al default "LocalServer" — nome personalizzato impostato dall'utente non viene mai toccato
- Settings keyboard layout keyboard-aware: tabview ridimensionato a 280px all'apertura tastiera (keyboard 200px fissi), tab panel scrollabile temporaneamente, `lv_obj_scroll_to_view_recursive` porta la textarea in vista; ripristino completo (height 480px, scroll y=0, scrollable flag) alla chiusura
- Feedback visivo tasti tastiera LVGL: `LV_PART_ITEMS | LV_STATE_PRESSED` con `bg_color #4a90d9`, `bg_opa COVER` — pressed state esplicito, visibile su touch capacitivo
- UI strings tutte in inglese: Settings labels, bottoni, tab names, messaggi di stato
- `model/indicator_hue.c/.h`: polling HTTPS ogni 5s verso Hue Bridge API v2 — GET stato luce (on/off + brightness), PUT toggle e brightness, task one-shot per comandi, aggiornamento ottimistico; avviato su `IP_EVENT_STA_GOT_IP` con 3s delay (dopo config fetch)
- `ui/screen_hue.c/.h`: 4 card con nome luce, switch ON/OFF (colore ambra), slider luminosità 1-100 (colore ambra); pattern lazy populate; callback UI registrata in `screen_hue_populate()`; `ensure_hue_populated()` in `ui_manager.c` prima delle transizioni verso screen_hue
- `sensedeck_proxy.py`: proxy Python completo (sostituisce `sibilla_proxy.py`); endpoint `/config` GET/POST e `/config/ui` Web UI dark theme per configurazione centralizzata
- `config.json`: file configurazione generato dal proxy (in `.gitignore`); include hue bridge/api/luci + ID UUID luci Hue + server + proxy + launcher URLs
- `model/indicator_config.c/.h`: fetch configurazione dal proxy al boot — handler `IP_EVENT_STA_GOT_IP` → task FreeRTOS (1.5s delay, guard `s_boot_fetched`)
- `config_fetch_from_proxy()`: GET `/config` dal proxy, parse JSON con cJSON, salva 18 campi in NVS
- Settings tab Proxy: pulsante "Ricarica config" — fetch immediato senza reboot, aggiorna label stato "OK"/"Errore"
- `app_config.h`: defaults e chiavi NVS per ID UUID luci Hue (`hue_l1_id`..`hue_l4_id`) e launcher URLs (`lnch_url_1`..`lnch_url_4`)
- Screen Settings custom (`screen_settings_custom.c`) con tabview 6 tab: Wi-Fi · Hue · Server · Proxy · AI · Schermate
- Tab "Schermate" in Settings: switch ON/OFF per Hue, LocalServer, Launcher, AI — valori salvati in NVS
- `ui_manager.c`: `next_from(idx, dir)` per navigazione circolare con skip schermate disabilitate
- Flag `g_scr_xxx_enabled` (hue/srv/lnch/ai) caricati da NVS in `ui_manager_init()`, aggiornati live dal tab Schermate
- `app_config.h`: defaults e chiavi NVS centralizzate (incluse 4 chiavi `scr_xxx_en` per enable flags)
- Schermate placeholder: screen_hue, screen_sibilla, screen_launcher, screen_ai
- Tastiera LVGL popup su tap textarea in Settings; valori salvati in NVS su Save
- Valori letti da NVS su `SCREEN_LOAD_START`, fallback a `app_config.h`
- `ui_screen_last` reso non-static in `ui.c` per permettere return da `ui_screen_wifi`

### Fixed
- `indicator_view.c`: null-check su `p_src` in `VIEW_EVENT_WIFI_ST` (bug Seeed: crash `LoadProhibited` con rssi fuori range)
- `indicator_wifi.c`: `esp_wifi_set_ps(WIFI_PS_NONE)` dopo ogni `esp_wifi_start()` — fix crash `pm_dream/ppTask`
- `ui_event_screen_time` (Seeed `ui.c`): target hardcoded `ui_screen_ai` su swipe RIGHT — sostituito con `gesture_clock()` in `ui_manager.c` via `lv_obj_remove_event_cb`

### Config
- `CONFIG_ESP_MAIN_TASK_STACK_SIZE=16384` — fix stack overflow con 5+ schermate LVGL custom
- `CONFIG_ESP_WIFI_STA_DISCONNECTED_PM_ENABLE=n` — fix crash Wi-Fi power management
- `CONFIG_SPIRAM_XIP_FROM_PSRAM=y` + `CONFIG_ESP_SYSTEM_MEMPROT_FEATURE=n` — fix `InstrFetchProhibited`
- `CONFIG_MBEDTLS_DYNAMIC_BUFFER=y` — fix heap esaurito con TLS simultanee

## [0.1.0-base]

- Firmware base da `examples/indicator_basis` Seeed Studio
- Clock, Sensors (CO2, tVOC, temp, umidità), Settings Wi-Fi originali
