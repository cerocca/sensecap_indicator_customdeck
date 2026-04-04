# Custom screens detail — SenseDeck

## Screen 2 — Hue Control

### API (Hue Bridge Local REST API v2)
- Base URL: `https://<HUE_BRIDGE_IP>/clip/v2/resource/light`
- Header: `hue-application-key: <HUE_API_KEY>`
- SSL: `skip_common_name = true`, `use_global_ca_store = false`

### Operations
```
GET  /clip/v2/resource/light/<ID>          # light state
PUT  /clip/v2/resource/light/<ID>          # body: {"on":{"on":true/false}}
PUT  /clip/v2/resource/light/<ID>          # body: {"dimming":{"brightness":80.0}}
```

### UI
- List of 4 lights: name + ON/OFF status + brightness slider (0–100%)
- ON/OFF toggle on tap, polling every 5 seconds
- Visual feedback on network error

---

## Screen 3 — LocalServer Dashboard

### Data sources
- **Glances API:** `http://<LOCALSERVER_IP>:<GLANCES_PORT>/api/4/`
  - CPU: `/cpu` → `total`
  - RAM: `/mem` → `percent`
  - Disk: `/fs` → first element, `percent`
  - Uptime: `/uptime`
  - Load avg: `/load`
- **Uptime Kuma:** via Mac proxy → `GET http://<PROXY_IP>:<PROXY_PORT>/uptime` → JSON array `[{"name": "...", "up": true/false}]`; names from Uptime Kuma `monitorList`; groups "0-..." excluded

### UI layout — y positions (480×480)
```
y=65   CPU bar
y=105  RAM bar
y=145  DSK bar
y=185  Uptime
y=215  Load avg (1m · 5m · 15m)
y=242  Horizontal separator
y=254  "Top Docker (RAM):" — header, font16, #7ec8a0
y=276+i×22  Docker container rows (i=0..4, y=276/294/312/330/348):
              name: label left x=10, width=330, LV_LABEL_LONG_DOT, font16, #b0c8e0
              MB:   label right x=350, width=100, LV_TEXT_ALIGN_RIGHT, font16, #b0c8e0
y=372  "Services: X/Y UP" (green=all UP, red=some DOWN), font16, white
y=394+i×14  DOWN service rows in red (max 6, pre-allocated, hidden if unused), font14
```

### Critical notes
- `/api/status-page/heartbeat/myuk` → ~55KB, cannot be buffered — **DO NOT use**
- Use only Mac proxy `GET /uptime` → compact JSON
- Uptime Kuma group parsing: exclude names starting with `"0-"`
- `esp_http_client_read` does not loop automatically — iterate until `r <= 0`

---

## Screen 4 — Launcher

- 4 buttons in 2×2 grid (200×170 px)
- On tap: `GET http://<PROXY_IP>:<PROXY_PORT>/open/<n>` (n=1..4)
- The Python proxy opens the corresponding URL on the Mac

---

## Screen 5 — Weather

### Data sources
The firmware calls **OpenWeatherMap directly** (HTTPS), without going through the Mac proxy.
The proxy is only used to configure parameters (API key, lat/lon, units, location) via Web UI → saved to NVS via `indicator_config.c` at boot (GET `/config`).

- **Current:** `https://api.openweathermap.org/data/2.5/weather?lat={lat}&lon={lon}&appid={key}&units={units}`
- **Forecast:** `https://api.openweathermap.org/data/2.5/forecast?lat={lat}&lon={lon}&appid={key}&units={units}&cnt=4`

### NVS keys
| Key | Content |
|---|---|
| `wth_api_key` | OWM API key (32 chars) |
| `wth_lat` | Latitude e.g. "43.7711" |
| `wth_lon` | Longitude e.g. "11.2486" |
| `wth_units` | "metric" or "imperial" |
| `wth_city` | City name (e.g. "Florence") |
| `wth_location` | Location display string (e.g. "Florence, IT") — takes priority over wth_city |
| `scr_wthr_en` | Screen enable flag |

### Location label — priority
`update_city_label()` looks in order:
1. `wth_location` (NVS) — e.g. "Florence, IT"
2. `wth_city` (NVS) — e.g. "Florence"
3. Fallback: formatted lat/lon — e.g. "43.77°N 11.25°E"

### UI layout (480×480)
- `y=0-44` Header "Weather" — font20, **white**
- `y=34` Header separator
- `y=52` City/location — font14, #aaaaaa
- `y=74` Condition icon — ASCII text font20, white (LVGL Montserrat does not support emoji — PNG icons are a future TODO)
- `y=96` Temperature — font20, white
- `y=138` Feels like — font14, #888888
- `y=160` Description — font16, #cccccc
- `y=186` Humidity + wind — font14, #aaaaaa
- `y=212` Separator
- `y=222` "Next hours" — font12, #7ec8e0
- `y=245` Forecast time (3 columns 160px)
- `y=265` Forecast icon
- `y=283` Forecast temp
- `y=301` Separator
- `y=311` "Next 3 days" — font12, #7ec8e0
- `y=328` Day name (3 columns 160px) — font12, #aaaaaa
- `y=343` Day icon — font14, white
- `y=358` Max/min temp — font12, #aaaaaa (format "max°/min°")
- `LV_ALIGN_BOTTOM_MID, 0, -30` "Updated X min ago" — font12, #555555
- `LV_ALIGN_BOTTOM_MID, 0, -10` Error label — font12, #e07070

### Weather icons (ASCII fallback)
OWM icon code → short text mapping (Montserrat font has no emoji):
`01d`→SUN, `01n`→MOON, `02x`→PRTC, `03x`→CLD, `04x`→OCLD, `09x`→DRZL, `10x`→RAIN, `11x`→STRM, `13x`→SNOW, `50x`→FOG

### Polling
- Every **10 minutes** (WEATHER_POLL_MS = 600000 ms)
- First poll: 5s after boot (WEATHER_FIRST_DELAY_MS)
- If `wth_api_key` NVS empty → no polling, `valid = false`
- Response buffer: 2048 bytes on stack (response ~700 bytes current, ~1500 bytes forecast cnt=4)
- TLS: `skip_cert_common_name_check=true, use_global_ca_store=false, cert_pem=NULL` → MBEDTLS_SSL_VERIFY_NONE (same pattern as Hue)

### UI timer
`lv_timer_create(weather_refresh_cb, 30000, NULL)` — refresh every 30s in timer cb (already in LVGL context → NO lv_port_sem_take/give in the cb itself)

---

## Screen 6 — Traffic

### Data source
Mac proxy `GET /traffic` → iterates enabled `traffic_routes`, calls Google Maps Distance Matrix API for each.
Response: **JSON array** of routes:
```json
[{"name":"Route 1","duration_sec":877,"duration_normal_sec":912,"delta_sec":-35,"distance_m":14721,"status":"ok"}, ...]
```
`status` per route: `"ok"` (delta ≤120s), `"slow"` (120–600s), `"bad"` (>600s).
If `gmaps_api_key` is empty or no routes are enabled/configured → `{"error":"not_configured"}`.

### Polling
- Every **10 minutes** (TRAFFIC_POLL_MS = 600000 ms)
- First poll: 8s after boot (TRAFFIC_FIRST_DELAY_MS)
- Response buffer: 2048 bytes
- Task pattern: see `indicator_glances.c`
- `indicator_traffic_force_poll()`: launches one-shot task (`force_poll_task`) that immediately executes `do_traffic_poll()` then self-deletes; called from "Reload config" in Settings tab Proxy

### UI update pattern — identical to screen_weather
`traffic_update_ui()` is **static** — not exposed in `.h`. `indicator_traffic.c` does not include
`screen_traffic.h` and never calls UI functions (no `lv_port_sem_take/give`).
The UI is updated exclusively by:
1. `traffic_update_ui()` called at the end of `screen_traffic_populate()`
2. Recurring LVGL timer every **5s** (`traffic_refresh_cb`) — runs only if `lv_scr_act() == ui_screen_traffic`

### Data structs
```c
typedef struct {
    char name[32];
    int  duration_sec, duration_normal_sec, delta_sec, distance_m;
    char status[8];
} traffic_route_t;

typedef struct {
    traffic_route_t routes[2];
    int     route_count;   /* 0, 1 or 2 */
    bool    valid;
    int64_t last_update_ms;
} traffic_data_t;
```

### UI layout (480×480) — adaptive
**1 route (route_count == 1):**
- Header "Traffic" — font20, white, TOP_MID
- `y=55` Separator
- `y=110` Route name — **font20**, #aaaaaa, center, **underlined** (larger than status)
- `y=155` Status "OK"/"SLOW"/"HEAVY" — font16, dynamic color (no ●)
- `y=200` Estimated time — font20, white
- `y=245` Delta — font16, dynamic color
- `y=285` Distance — font14, #aaaaaa
- `y=320` Separator
- `y=345` "Configure route via proxy Web UI" — only if `!valid`, font14, #aaaaaa
- `BOTTOM_MID -50` "Updated X min ago" — font12, #555555
- `BOTTOM_MID -25` Error label — font12, #e07070

**2 routes (route_count == 2), split screen:**
- Header "Traffic" — font20, white
- `y=55` Header separator
- Route 0: name(72,**font20**,underline) · status(104,font16) · duration(132,**font18**) · delta(162,font14) · distance(186,font12)
- `y=212` Mid-screen separator
- Route 1: name(224,**font20**,underline) · status(256,font16) · duration(284,**font18**) · delta(314,font14) · distance(338,font12)
- `BOTTOM_MID` Updated / error

**If `!valid`:** single layout with "NO DATA" + configure label visible.

### Settings tab "Traffic"
Info label only: "Configure origin/destination at: http://\<proxy\>/config/ui" (dynamic URL from NVS).
No ON/OFF switch — the Traffic switch is in the Screens tab.

---

## Settings (custom) — outside rotation

Accessible via **swipe UP from clock** (outside horizontal rotation).
Layout: LVGL tabview (44px header), 6 horizontal tabs.

| Tab | Content |
|---|---|
| **Hue** | Bridge IP, API key, names of 4 lights — textarea + Save → NVS |
| **Server** | Server IP, Server Name, Glances Port, Beszel Port, Uptime Kuma Port → NVS (5 fields) |
| **Proxy** | Mac proxy IP + port → NVS · "Reload config" button → fetch from proxy without reboot · "Config UI: http://..." label with LVGL recolor (white + #7ec8e0), updated dynamically from NVS |
| **Weather** | Info URL proxy Web UI (dynamic, no switch — switch is in Screens) |
| **Traffic** | Info URL proxy Web UI (dynamic, no switch — switch is in Screens) |
| **Screens** | 6 switches: Default sensor screen, Hue, LocalServer, Launcher, Weather, Traffic → update flag + NVS |

**Note**: Wi-Fi tab removed — Wi-Fi configuration is accessible via swipe DOWN from clock → Seeed `ui_screen_setting`.

- Values read from NVS on `SCREEN_LOAD_START`; fallback to `app_config.h` if NVS empty.
- LVGL popup keyboard on tap on any textarea; closes on READY/CANCEL.
- NVS keys max 15 chars — centralized in `app_config.h`.

**`ui_screen_sensor` (Seeed) — customized from outside in `ui_manager_init()`:**
- `ui_wifi__st_button_2` (Wi-Fi status button top-right + child icon) → `LV_OBJ_FLAG_HIDDEN`
- `ui_scrolldots2` (3-dot navigation container) → `LV_OBJ_FLAG_HIDDEN`

**`ui_screen_setting` (Seeed) — customized from outside:**
- Handler replaced with `gesture_seeed_setting` (UP → clock)
- Elements hidden via `lv_obj_add_flag(..., LV_OBJ_FLAG_HIDDEN)` in `ui_manager_init()`:
  `ui_setting_title` (label "Setting"), `ui_setting_icon` (gear icon), `ui_scrolldots3` (3 dots)
- Wi-Fi button and icon remain visible and functional
