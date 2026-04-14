# Changelog

## [Unreleased]

## [1.0.2] — 2026-04-07

### Fixed

- fix: proxy Beszel token — TTL 1h + invalidazione automatica su errore HTTP
- `indicator_uptime_kuma.c`: `UK_BUF_SIZE` 2048 → 8192 bytes (PSRAM) — response from `/uptime` with 10+ monitors exceeded 2 KB, causing JSON truncation, parse failure, and empty service list on `screen_server`
- `indicator_uptime_kuma.c`: callback now called even when `total == 0` (removed `total > 0` guard) — ensures UI resets to empty state on fetch/parse failure instead of retaining stale content

## [1.0.1] — 2026-04-06

### Fixed

- `indicator_system.c`: Glances hostname was written to `NVS_KEY_SERVER_NAME` ("srv_name"), overwriting the user-set display name; moved to separate key `NVS_KEY_GLANCES_HOST` ("glances_host")
- `indicator_config.c`: proxy was sending `srv_name = "LocalServer"` (legacy default from old `config.json`) and firmware applied it on every reboot; added `strcmp != APP_CFG_SERVER_NAME` guard to treat the default value as "not explicitly set"
- `screen_server.c`: added "DOWN" badge next to the name of DOWN Uptime Kuma services (red label `#FF4444`, font14, x=300); name labels constrained to 285px with `LV_LABEL_LONG_DOT`

### Changed

- `sensedeck_proxy.py` `DEFAULT_CONFIG`: `srv_name` changed from `"LocalServer"` to `""` — proxy no longer overwrites the local display name if the field was never explicitly set in the Web UI
- `sensedeck_proxy.py` Web UI: "Server Name" field now shows placeholder `"Leave empty to keep device value"`
- `app_config.h`: added `NVS_KEY_GLANCES_HOST "glances_host"`

## [1.0.0] — 2026-04-04

First public release. Fully functional firmware with 7 screens, circular navigation,
Python proxy, and centralized configuration.

## [0.1.0-dev]

Full development cycle from base Seeed firmware to feature-complete SenseDeck.

### Added — Custom screens

- **`screen_hue`** (idx 2): 4 light cards with name, ON/OFF switch, brightness slider 1–100; lazy populate; HTTPS polling every 5s via Hue Bridge API v2 (`indicator_hue.c`)
- **`screen_server`** (idx 3): CPU/RAM/DSK progress bars, uptime, load avg, top 5 Docker containers by RAM, Uptime Kuma status (X/Y UP + DOWN service list); lazy populate
- **`screen_launcher`** (idx 4): 2×2 button grid, fire-and-forget HTTP to proxy `/open/<n>`, mnemonic names from NVS, refresh on every SCREEN_LOAD_START
- **`screen_weather`** (idx 5): temperature, ASCII icon, feels_like, humidity, wind, 4×3h forecast slots, "Next 3 days" section (dominant icon, max/min temp); direct HTTPS polling OWM every 10 min
- **`screen_traffic`** (idx 6): adaptive single/dual route layout, OK/SLOW/HEAVY indicator (green/orange/red), estimated time, delta vs normal, distance; proxy `/traffic` polling every 10 min; `indicator_traffic_force_poll()` for immediate refresh
- **`screen_settings_custom`** (outside rotation, swipe UP from clock): tabview with tabs Hue · Server · Proxy · Weather · Traffic · Screens · Info
  - Tab **Info**: firmware version, repo QR code (180×180), repo URL, credits, MIT License

### Added — Navigation

- Circular bidirectional navigation: `clock ↔ [sensors] ↔ [hue] ↔ [server] ↔ [launcher] ↔ [weather] ↔ [traffic] ↔ clock`
- `screen_settings_custom` outside horizontal rotation: swipe UP from clock; swipe DOWN from settings returns to clock
- `ui_screen_setting` Seeed: swipe DOWN from clock; swipe UP returns to clock
- Skip logic `next_from(idx, dir)` in `ui_manager.c`: disabled screens skipped automatically
- `g_scr_xxx_enabled` flags (hue/srv/lnch/wthr/defsens/traffic) loaded from NVS, updated live from tab Screens
- Unified `ensure_populated(scr)` helper — all gesture handlers call it before navigating
- Seeed handler replacement (`lv_obj_remove_event_cb` + `lv_obj_add_event_cb`) on clock, sensors, settings — fixes hardcoded targets and double `_ui_screen_change` calls

### Added — Proxy (`sensedeck_proxy.py`)

- Endpoint `GET /config` — centralized firmware configuration; fetched at boot by `indicator_config.c`
- Endpoint `GET /uptime` — Uptime Kuma monitor status (real names from `publicGroupList`, not numeric IDs)
- Endpoint `GET /docker` — top containers by RAM via Beszel API (cached auth token, refresh on 401)
- Endpoint `GET /traffic` — Google Maps Distance Matrix API for configured routes
- Endpoint `GET/POST /open/<n>` — opens URL on host machine; `subprocess.Popen` with error handling
- Web UI `/config/ui`: dark theme interface with **6 tabs** (Hue · LocalServer · Proxy · Launcher · Weather · Traffic); always-visible Save button
- Recursive `_merge_config()` — correctly preserves nested structures (`traffic_routes`) across restarts

### Added — Data model

- `indicator_config.c/.h`: config fetch from proxy at boot (`IP_EVENT_STA_GOT_IP`, 3s delay, 3× retry with 2s backoff); saves 18+ fields to NVS
- `indicator_glances.c/.h`: Glances API polling every 10s (CPU/RAM/DSK/uptime/load/Docker); containers buffer in PSRAM
- `indicator_uptime_kuma.c/.h`: proxy `/uptime` polling every 30s; up to 6 DOWN services displayed
- `indicator_weather.c/.h`: OWM `/weather` + `/forecast` HTTPS polling every 10 min; per-day forecast aggregation (3 days)
- `indicator_traffic.c/.h`: proxy `/traffic` polling every 10 min; force-poll via atomic flag
- `indicator_system.c/.h`: hostname fetch from Glances at boot (5s delay); overwrites NVS only if default value
- `indicator_hue.c/.h`: Hue Bridge API v2 HTTPS polling every 5s; PUT toggle and brightness; optimistic UI update

### Added — Configuration

- `app_config.h`: centralized NVS keys and defaults for all screens (Hue, Weather, Traffic, Launcher, Server, Screens)
- `sdkconfig.defaults`: `CONFIG_SPIRAM_XIP_FROM_PSRAM=y`, `CONFIG_MBEDTLS_DYNAMIC_BUFFER=y`, `CONFIG_ESP_MAIN_TASK_STACK_SIZE=16384`, `CONFIG_ESP_WIFI_STA_DISCONNECTED_PM_ENABLE=n`
- `LV_USE_QRCODE=y` in `sdkconfig` (Info tab in settings)
- Custom `partitions.csv`

### Fixed — Boot stability

- `indicator_hue.c`: initial poll delay 3s→10s; 200ms pause between 4 consecutive HTTPS requests — fixes `esp-aes: Failed to allocate memory` (TLS heap contention)
- `TRAFFIC_FIRST_DELAY_MS` 8s→20s — fixes lwIP OOM crash (`LoadProhibited` in `netconn_gethostbyname`) from simultaneous TLS connections at boot
- `indicator_city_init()` disabled — lwIP OOM crash at boot (`thread_sem_init: out of memory` → Guru Meditation in `indicator_city.c:216`); `ui_city`/`ui_location`/`ui_location_Icon` widgets hidden
- `indicator_traffic_init()` moved after `indicator_model_init()` in `app_main.c` — default event loop must exist before registering `IP_EVENT_STA_GOT_IP` handler
- `esp_wifi_set_ps(WIFI_PS_NONE)` after every `esp_wifi_start()` — fixes `pm_dream/ppTask` crash
- `indicator_view.c`: null-check on `p_src` in `VIEW_EVENT_WIFI_ST` — fixes Seeed crash with out-of-range rssi

### Fixed — UI

- NTP/Timezone: `__tz_apply_from_cfg()` composes POSIX TZ string from Seeed controls (UTC offset + DST switch); `setenv` + `tzset` applied at boot and on every save — automatic CET/CEST DST with no manual intervention
- `screen_server.c`: Docker labels now reset between consecutive polls (were sticky)
- `screen_traffic.c`: black screen on first swipe fixed with `traffic_wait_data_cb` pattern
- `indicator_traffic.c`: `indicator_traffic_force_poll()` uses atomic flag instead of `xTaskCreate` (was failing due to insufficient heap)
- `TRAFFIC_BUF_SIZE` 512→2048 — fixes silent JSON parse failure from proxy
- All gesture handlers registered with `LV_EVENT_GESTURE` (was `LV_EVENT_ALL`) — fixes LVGL 8.x event bubbling
- `ui_manager.c`: `ensure_populated()` before every `_ui_screen_change()` — fixes blank screen on first swipe with lazy init screens

### Changed

- Boot screen: always clock (removed one-shot auto-navigate timer to sensors)
- `screen_settings_custom`: Wi-Fi tab removed (now accessible via swipe DOWN from clock → Seeed settings)
- OWM API: removed `&lang=it` — weather descriptions in English
- Top Docker containers: 3→5; row spacing 22→18px
- Unused Seeed UI widgets hidden via `lv_obj_add_flag(LV_OBJ_FLAG_HIDDEN)` without modifying Seeed sources (except `ui.c` for Date&Time — agreed exception)

---

## [0.1.0-base]

- Base firmware from Seeed Studio `examples/indicator_basis`
- Clock, Sensors (CO2, tVOC, temp, humidity), Wi-Fi Settings — original Seeed screens
`