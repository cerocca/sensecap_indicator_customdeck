# Changelog

## [Unreleased]

### Added
- `app_config.h`: `NVS_KEY_WTH_CITY`, `NVS_KEY_WTH_LOCATION`, `NVS_KEY_BESZEL_PORT` + `DEFAULT_BESZEL_PORT "8090"`, `NVS_KEY_UK_PORT "uk_port"` + `DEFAULT_UK_PORT "3001"`, `NVS_KEY_SCR_DEFSENS "scr_defsens"` + `DEFAULT_SCR_DEFSENS "1"`
- Settings tab Server: campi Beszel Port e Uptime Kuma Port; ora 5 campi totali (Server IP, Server Name, Glances Port, Beszel Port, UK Port)
- Settings tab Proxy: label dinamica "Config UI: http://…/config/ui" con recolor LVGL — "Config UI:" bianco, URL in #7ec8e0; aggiornata da NVS al SCREEN_LOAD_START
- Settings tab Screens: switch "Default sensor screen" come primo switch (`g_scr_defsens_enabled`, NVS `scr_defsens`) — quando OFF, `screen_sensors` viene saltata nello swipe
- `ui_manager.c/.h`: flag `g_scr_defsens_enabled`; `scr_enabled(1)` usa il flag (non più hardcoded true); one-shot `lv_timer` 3000ms in `ui_manager_init()` per auto-navigare a sensors al boot se defsens abilitato; `gesture_clock` LEFT usa `next_from(0,+1)` con guard `ensure_settings_populated()`
- `sensedeck_proxy.py`: campo `owm_location` in `DEFAULT_CONFIG` e Web UI; campo `uk_port` in `DEFAULT_CONFIG`; Web UI `/config/ui` ristrutturata a 3 colonne flex (Hue / LocalServer+Proxy / Launcher+Weather); helper `field()`, `pwd_field()`, `select_field()`; Hue IDs come hidden inputs; `/uptime` URL costruito dinamicamente da `server_ip` + `uk_port`
- `model/indicator_config.c`: salva `owm_location` (`NVS_KEY_WTH_LOCATION`) da JSON proxy al boot fetch
- `ui/screen_weather.c`: label città/location con priorità `wth_location` → `wth_city` → lat/lon formattato; tutti gli elementi sotto y=44 shiftati di +35px; font città font14 #aaaaaa; titolo colore bianco
- `ui/screen_sibilla.c`: titolo colore bianco (era #7ec8a0), posizione y=10
- `model/indicator_weather.c/.h`: polling OpenWeatherMap `/data/2.5/weather` + `/data/2.5/forecast?cnt=4` direttamente (HTTPS, no proxy) ogni 10 minuti; parse current (temp/feels/icon/desc/hum/wind) + 4 slot forecast 3h; buffer 2048 byte; TLS no-verify; task avviato con 8s delay; `weather_icon_label()` mappa codice OWM → testo ASCII breve
- `ui/screen_weather.c/.h`: schermata meteo con lazy init — icona ASCII, temperatura, feels_like, descrizione, umidità+vento, 4 slot forecast 3h (ora/icona/temp), label "Updated X min ago", label errore; timer `lv_timer` 30s per refresh display; gesture handler anti-reentrant con `next_from(6, ±1)`; sostituisce `screen_ai` (idx 6)
- `app_config.h`: chiavi NVS `wth_api_key`, `wth_lat`, `wth_lon`, `wth_units`, `wth_city`, `wth_location`, `scr_wthr_en`; `DEFAULT_WTH_UNITS = "metric"`
- `model/indicator_config.c`: salva `owm_api_key`, `owm_lat`, `owm_lon`, `owm_units`, `owm_city_name`, `owm_location` da JSON proxy in NVS al boot fetch
- `sensedeck_proxy.py`: campi weather in `DEFAULT_CONFIG`; sezione "Meteo (OpenWeatherMap)" nella Web UI `/config/ui`
- Settings tab "Meteo" (sostituisce "AI"): info URL proxy Web UI dinamica + switch ON/OFF schermata Weather
- Settings tab "Screens": rimosso switch AI, rimangono Hue/LocalServer/Launcher (3 switch); switch Weather in tab Meteo

### Changed
- OWM API URLs: rimosso `&lang=it` — descrizioni meteo in inglese (lingua di sistema OWM)
- Settings tab Server: reintrodotto campo Server IP come primo campo (era stato rimosso per errore)
- `ui_manager.c/.h`: `screen_ai` → `screen_weather` (idx 6), `g_scr_ai_enabled` → `g_scr_wthr_enabled`, NVS key `scr_wthr_en`; aggiunto `indicator_weather_init()` in `ui_manager_init()`
- Navigazione: `clock ↔ [sensors] ↔ settings ↔ [hue] ↔ [sibilla] ↔ [launcher] ↔ [weather] ↔ clock` (sensors ora opzionale via switch)
- Font temperatura schermata Weather: `lv_font_montserrat_20`
- `sensedeck_proxy.py`: URL Uptime Kuma costruito dinamicamente da `server_ip` + `uk_port` (rimosso `UPTIME_KUMA_URL` costante globale)

### Fixed
- `sensedeck_proxy.py`: aggiunta dichiarazione encoding `# -*- coding: utf-8 -*-` (fix SyntaxError con Python 2)
- `sensedeck_proxy.py`: rimosso riferimento a `UPTIME_KUMA_URL` rimossa; startup print aggiornato


- `ui/screen_launcher.c/.h`: schermata Launcher completa — griglia 2×2 pulsanti (200×170px), feedback colore al press (#4a90d9 → #1e1e2e), HTTP fire-and-forget verso proxy `/open/<n>` su task FreeRTOS separato (stack 3072, heap-allocated args), titolo "Launcher" (font 20), lazy populate su `LV_EVENT_SCREEN_LOAD_START`
- Nomi mnemonici pulsanti Launcher (`NVS_KEY_LNCH_NAME_1..4`, default GitHub/Strava/Garmin/Intervals) separati dagli URL; labels uppercase centrate con `lv_font_montserrat_20`; refresh da NVS ad ogni `SCREEN_LOAD_START`
- `sensedeck_proxy.py`: endpoint `GET /docker` — autentica a Beszel via `/api/collections/users/auth-with-password`, recupera `container_stats` collection, ordina per `m` (RAM MB) desc, ritorna `[{"name":..., "mem_mb":...}]` top-3; token cached globalmente con refresh su 401
- `sensedeck_proxy.py`: Web UI aggiornata — sezione Beszel (port/user/password) e launcher name fields paired con URL fields; `DEFAULT_CONFIG` esteso con `beszel_port`, `beszel_user`, `beszel_password`, `lnch_name_1..4`
- `model/indicator_glances.c`: legge `NVS_KEY_PROXY_IP`/`PORT` in `glances_read_config()`; URL containers cambiato da `http://<server>/api/4/containers` a `http://<proxy>/docker`; `parse_top_docker()` riscritta per formato proxy `[{"name":..., "mem_mb":...}]`
- `model/indicator_config.c`: salva `lnch_name_1..4` da JSON proxy in NVS al boot fetch
- `model/indicator_glances.c/.h`: modulo polling Glances API ogni 10s — CPU (`/api/4/cpu`), RAM (`/api/4/mem`), disco (`/api/4/fs`), uptime (`/api/4/uptime`), load avg (`/api/4/load`), top 3 container Docker per RAM; buffer containers (32KB) allocato in PSRAM (`heap_caps_malloc MALLOC_CAP_SPIRAM`); task avviato su `IP_EVENT_STA_GOT_IP`; callback UI registrata da `screen_sibilla_populate()`
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
- `ui/screen_sibilla.c`: Docker label reset bug — le label "Top Docker" non si resettavano tra poll consecutivi; aggiunto loop reset esplicito (→ "--"/""  ) prima del loop di aggiornamento in `on_glances_data()`
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
