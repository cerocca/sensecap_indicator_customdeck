# Changelog

## [Unreleased]

### Added
- Screen Settings custom (`screen_settings_custom.c`) con tabview 5 tab: Wi-Fi · Hue · Server · Proxy · AI
- `app_config.h`: defaults e chiavi NVS centralizzate
- `ui_manager.c/.h`: navigazione circolare bidirezionale completa tra 7 schermate
- Schermate placeholder: screen_hue, screen_sibilla, screen_launcher, screen_ai
- Tastiera LVGL popup su tap textarea in Settings; valori salvati in NVS su Save
- Valori letti da NVS su `SCREEN_LOAD_START`, fallback a `app_config.h`
- `ui_screen_last` reso non-static in `ui.c` per permettere return da `ui_screen_wifi`

### Fixed
- `indicator_view.c`: null-check su `p_src` in `VIEW_EVENT_WIFI_ST` (bug Seeed: crash `LoadProhibited` con rssi fuori range)
- `indicator_wifi.c`: `esp_wifi_set_ps(WIFI_PS_NONE)` dopo ogni `esp_wifi_start()` — fix crash `pm_dream/ppTask`

### Config
- `CONFIG_ESP_MAIN_TASK_STACK_SIZE=16384` — fix stack overflow con 5+ schermate LVGL custom
- `CONFIG_ESP_WIFI_STA_DISCONNECTED_PM_ENABLE=n` — fix crash Wi-Fi power management
- `CONFIG_SPIRAM_XIP_FROM_PSRAM=y` + `CONFIG_ESP_SYSTEM_MEMPROT_FEATURE=n` — fix `InstrFetchProhibited`
- `CONFIG_MBEDTLS_DYNAMIC_BUFFER=y` — fix heap esaurito con TLS simultanee

## [0.1.0-base]

- Firmware base da `examples/indicator_basis` Seeed Studio
- Clock, Sensors (CO2, tVOC, temp, umidità), Settings Wi-Fi originali
