## changelog

### [Unreleased]

* Screen Settings custom (screen_settings_custom.c) con tabview 5 tab: Wi-Fi, Hue, Server, Proxy, AI
* app_config.h: defaults e chiavi NVS centralizzate
* Navigazione circolare: sensors ↔ settings_custom ↔ hue (sostituisce ui_screen_setting nella chain)
* ui_screen_last reso non-static in ui.c per permettere return da ui_screen_wifi a settings_custom
* Tastiera LVGL popup su tap textarea; campi salvati in NVS su Save
* Valori letti da NVS su SCREEN_LOAD_START, fallback a app_config.h

### v1.0.0 ###

* Initial version.