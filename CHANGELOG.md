# Changelog

## [Unreleased]

### Changed
- `screen_sibilla.c` + `indicator_glances.c/.h`: top Docker container per RAM aumentati da 3 a 5; label aggiornata a "Top 5 Docker (RAM)"; spaziatura righe 22→18px; `Monitored Services` y 366→372; label DOWN y 388→394

### Added
- `screen_settings_custom.c`: tab **Info** (ultimo tab) — titolo "SenseDeck" (font_montserrat_20, #7ec8a0), versione firmware da `esp_app_get_description()->version`, QR code 180×180 (`lv_qrcode`) → `https://github.com/cerocca/sensecap_indicator_customdeck`, URL repo testo (font_12 #aaaaaa), credits "cerocca" (font_14), "MIT License" (font_12 #666666)
- `firmware/sdkconfig`: `LV_USE_QRCODE=y` abilitato (necessario per `lv_qrcode_create`/`lv_qrcode_update` nel tab Info)

### Fixed
- `indicator_traffic.c`: `TRAFFIC_BUF_SIZE` 512→2048 — fix parse failure silenzioso su risposta proxy `/api/4/fs` (buffer troppo piccolo troncava il JSON)
- `sensedeck_proxy.py`: `/open/<n>` — `subprocess.Popen` wrappato in `try/except`; risposta HTTP 500 invece di connection reset se `open -a Firefox` fallisce
- `sensedeck_proxy.py`: Web UI `/config/ui` — valori config inseriti in attributi HTML con `html.escape(..., quote=True)`; fix rendering rotto con `"` nei valori
- `sensedeck_proxy.py`: `load_config()` fallback usa `_merge_config(DEFAULT_CONFIG, {})` invece di `dict(DEFAULT_CONFIG)` — deep copy corretta per `traffic_routes`

### Changed
- `ui_manager.c`: tutti i gesture handler registrati con `LV_EVENT_GESTURE` (era `LV_EVENT_ALL`); `on_screen_load_start` handler in sibilla/launcher/weather/traffic registrati con `LV_EVENT_SCREEN_LOAD_START`
- `ui_manager.c`: `ensure_sibilla_populated()` aggiunta e inclusa in `ensure_populated()` — sibilla ora allineata al pattern di hue/launcher/weather/traffic
- `screen_hue.c`: aggiunto `static bool s_populated` + guard `if (!s_populated) return` in `hue_update_ui()` — allineato al pattern delle altre schermate custom

### Changed
- `screen_weather.c`: separatore header riposizionato (y=34→45); separatore tra "Next hours" e "Next 3 days" riposizionato (y=297→305)
- `screen_settings_custom.c`: label URL dinamica nel tab Weather (era hardcoded `localhost:8765`); label "Config UI" nel tab Proxy ora dinamica da NVS con recolor LVGL; rename label "Mac Proxy IP" → "Proxy IP"

### Added
- `screen_traffic.c`: handler `on_screen_load_start` (`LV_EVENT_SCREEN_LOAD_START`) — chiama `traffic_update_ui()` ad ogni carico schermata, identico al pattern di `screen_weather.c`; prima la UI si aggiornava solo dal timer 5s

### Fixed
- `ui_manager.c`: aggiunto `ensure_populated()` prima di `_ui_screen_change()` in tutti i gesture handler (clock LEFT/RIGHT, traffic LEFT, weather RIGHT, hue RIGHT, launcher RIGHT) — preveniva schermata vuota al primo swipe verso schermate con lazy init
- `indicator_weather_init()` spostata da `ui_manager_init()` a `main.c` dopo `indicator_uptime_kuma_init()` — corretto posizionamento init modello fuori dal contesto LVGL sem
- `indicator_hue.c`: delay primo poll aumentato 3s→10s; aggiunta pausa 200ms tra le 4 richieste HTTPS consecutive — risolve `esp-aes: Failed to allocate memory` al boot causato da contesa heap TLS
- `TRAFFIC_FIRST_DELAY_MS` aumentato da 8000ms a 20000ms — risolto crash lwIP out of memory (`LoadProhibited` in `netconn_gethostbyname`) causato da contesa connessioni TLS simultanee al boot
- `indicator_traffic.c`: `indicator_traffic_force_poll()` non usa più `xTaskCreate` (falliva per heap insufficiente); sostituito con flag atomico `s_force_poll_requested`; il task polling periodico controlla il flag ogni 500ms e interrompe l'attesa, eseguendo il poll entro 500ms dalla richiesta

### Added
- `screen_weather.c`: sezione "Next 3 days" — 3 colonne giorni (nome giorno, icona ASCII, temp max/min); header "Next 3 days" font12 #7ec8e0; separatore; nascosta se `days_count == 0`
- `indicator_weather.c/.h`: aggregazione forecast per giorno (3 giorni successivi) — bin per data da `/forecast?cnt=24` (72h OWM); icona dominante per giorno con normalizzazione base (senza suffisso d/n); `weather_day_t` struct (`day_label`, `icon`, `temp_min`, `temp_max`); `days[3]` + `days_count` in `weather_data_t`; buffer forecast PSRAM 12KB (`heap_caps_malloc MALLOC_CAP_SPIRAM`)

### Changed
- `screen_weather.c`: tutte le coordinate y ridimensionate per ridurre gap eccessivi — città 74→52, icona 101→74, temp 123→96, feels_like 171→138, descrizione 195→160, umidità/vento 227→186, separatore "Next hours" 250→212, header "Next hours" 260→222, slot forecast 283/303/321→245/265/283, separatore "Next 3 days" 339→301, header "Next 3 days" 349→311, righe giorni 366/381/396→328/343/358
- `screen_traffic.c`: logica aggiornamento UI riscritta seguendo il pattern di `screen_weather` — `screen_traffic_update()` rinominata `traffic_update_ui()` (static, non esposta); rimossi `traffic_screen_loaded_cb`, `traffic_wait_data_cb`, `traffic_force_redraw_cb`, `on_screen_load_start`; timer refresh 30s→5s; `screen_traffic.h` rimuove la dichiarazione pubblica di `screen_traffic_update()`
- `indicator_traffic.c`: rimossi `#include "screen_traffic.h"`, `#include "lv_port.h"`, `lv_port_sem_take/give` e chiamata `screen_traffic_update()` — il model aggiorna solo `g_traffic`, mai la UI
- `screen_settings_custom.c`: tab Proxy e Weather mostrano URL fisso `http://localhost:8765/config/ui`; tab Traffic mantiene URL dinamico da NVS

### Added
- `screen_traffic.c/.h` + `indicator_traffic.c/.h`: schermata Traffic (slot 6) con layout adattivo — singola route (1 route, centrato) o doppio (2 route, schermo diviso); indicatore stato OK/SLOW/HEAVY (verde/arancione/rosso); tempo stimato, delta vs normale, distanza; lazy populate; timer refresh 30s; label "Configure route via proxy Web UI" se dati non disponibili
- `sensedeck_proxy.py`: endpoint `GET /traffic` — itera `traffic_routes` abilitate, chiama Google Maps Distance Matrix API per ognuna, ritorna array JSON; `{"error":"not_configured"}` se API key assente o nessuna route configurata
- `sensedeck_proxy.py` Web UI `/config/ui`: layout riscritto da 3 colonne a **6 tab** (Hue | LocalServer | Proxy | Launcher | Weather | Traffic); tab bar con bordo attivo #7ec8e0; Save button sempre visibile
- `indicator_traffic_force_poll()`: poll immediato one-shot senza attendere il timer da 10 min; chiamato da "Reload config" in Settings → tab Proxy — la schermata Traffic si aggiorna senza riavvio device
- `ui_manager.c`: helper `ensure_populated(scr)` unificato — tutti i gesture handler vi passano prima di navigare

### Fixed
- `sensedeck_proxy.py`: merge superficiale (`dict.update`) sostituito con `_merge_config()` ricorsivo — preserva correttamente strutture annidate (`traffic_routes` array di dict) al riavvio del proxy; aggiunto log `[config] loaded N keys` + nomi route per debug
- `indicator_traffic.c` + `main.c`: `indicator_traffic_init()` spostata dopo `indicator_model_init()` in `app_main.c` — il default event loop deve esistere prima di registrare handler su `IP_EVENT_STA_GOT_IP`
- `screen_traffic.c`: fix schermata nera al primo swipe — `traffic_wait_data_cb` (timer ricorrente 1000ms) attende `g_traffic.valid == true`, esegue il primo `screen_traffic_update()`, poi si auto-cancella

### Refactor
- `indicator_traffic.c`: logica poll estratta in `do_traffic_poll()` condivisa tra loop periodico (`traffic_poll_task`) e force-poll one-shot (`force_poll_task`); static buffer spostati a livello modulo (evita stack overflow)

### Changed
- Navigazione: `screen_settings_custom` spostata fuori dalla rotazione orizzontale — raggiungibile solo via swipe UP dal clock (MOVE_TOP); swipe DOWN da settings_custom torna al clock (MOVE_BOTTOM)
- Navigazione: swipe DOWN dal clock ora apre `ui_screen_setting` Seeed (MOVE_BOTTOM); swipe UP da lì torna al clock (MOVE_TOP)
- `ui_manager.c`: slot 2 ora riservato a screen_traffic (placeholder, `scr_enabled(2)=false`, `s_scr[2]=NULL`); settings_custom rimossa da `s_scr[]`
- `ui_manager.c`: `gesture_clock` — aggiunto `LV_DIR_TOP` → settings_custom; `LV_DIR_BOTTOM` → `ui_screen_setting`; rimosso vecchio BOTTOM verso `ui_screen_wifi`
- `ui_manager.c`: `gesture_settings_custom` — rimossi handler LEFT/RIGHT, aggiunto solo DOWN → clock
- `ui_manager.c`: `gesture_seeed_setting` — `lv_obj_remove_event_cb(ui_screen_setting, ui_event_screen_setting)` prima di aggiungere il nuovo handler (fix doppia chiamata `_ui_screen_change`); solo UP → clock
- `ui_manager.c`: aggiunto `gesture_sensor` — `lv_obj_remove_event_cb(ui_screen_sensor, ui_event_screen_sensor)` perché Seeed aveva LEFT hardcoded su `ui_screen_settings_custom` (causa: settings_custom ora fuori rotazione); LEFT ora usa `next_from(1, +1)` con `ensure_hue_populated()`; RIGHT e TOP replicano comportamento Seeed
- `ui_manager.c`: rimossa `gesture_sensor_lazy_settings` (settings non più in rotazione orizzontale)
- `ui_manager.c`: `gesture_hue` RIGHT — rimosso `ensure_settings_populated()` difensivo (non più necessario)
- `ui_manager.c`: rimosso one-shot `lv_timer` auto-navigate a sensors al boot — device parte sempre dal clock
- `screen_settings_custom.c`: rimosso tab Wi-Fi — configurazione Wi-Fi ora accessibile via swipe DOWN dal clock → `ui_screen_setting` Seeed; rimossi `build_tab_wifi`, `on_wifi_btn`, externs `ui_screen_wifi`/`ui_screen_last`; tab count scende da 6 a 5 (Hue · Server · Proxy · Meteo · Screens)

### Added
- `ui_manager.c`: nascoste su `ui_screen_sensor` le variabili `ui_wifi__st_button_2` e `ui_scrolldots2` via `lv_obj_add_flag(..., LV_OBJ_FLAG_HIDDEN)` — file Seeed intatti
- `ui_manager.c`: nascoste su `ui_screen_setting` le variabili `ui_setting_title`, `ui_setting_icon`, `ui_scrolldots3` via `lv_obj_add_flag(..., LV_OBJ_FLAG_HIDDEN)` — Wi-Fi button resta visibile; file Seeed intatti
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
