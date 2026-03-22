# Changelog

## [Unreleased]

### Added
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
