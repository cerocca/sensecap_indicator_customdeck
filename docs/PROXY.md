# Mac Proxy — sensedeck_proxy.py

Python script on Mac, port 8765. Manages centralized configuration and local service integration.

## Endpoints

| Endpoint | Method | Description |
|---|---|---|
| `/uptime` | GET | Uptime Kuma service status (compact JSON) |
| `/traffic` | GET | travel time from Google Maps Distance Matrix API |
| `/docker` | GET | top containers by RAM from Beszel → `[{"name":..., "mem_mb":...}]` |
| `/open/<n>` | GET | opens URL n in Firefox (n=1..4) |
| `/ping` | GET | health check |
| `/config` | GET | device configuration (JSON, from `config.json`) |
| `/config` | POST | saves new config to `config.json` |
| `/config/ui` | GET | configuration Web UI (dark theme) |

`config.json` is in the same directory as the script; it is in `.gitignore` (not versioned).
DEFAULT_CONFIG in the proxy: fields for hue bridge/api/lights/ID, server + `srv_name`, proxy, launcher URLs + names (`lnch_name_1..4`), Beszel (`beszel_port`, `beszel_user`, `beszel_password`), OWM (`owm_api_key`, `owm_lat`, `owm_lon`, `owm_units`, `owm_city_name`, `owm_location`), Uptime Kuma port (`uk_port`), `gmaps_api_key`, `traffic_routes` (array of 2 routes: `name/origin/destination/mode/enabled`). Merged with defaults on POST for backward compatibility.

## Web UI — `/config/ui`

**6-tab** layout (dark theme): **Hue** | **LocalServer** | **Proxy** | **Launcher** | **Weather** | **Traffic**
- **Hue**: bridge IP, API key, names of 4 lights (UUID IDs as hidden inputs)
- **LocalServer**: server name, IP, Glances port, UK port, Beszel port/user/password
- **Proxy**: proxy IP, proxy port
- **Launcher**: 4 names + URLs
- **Weather**: OWM API key, location, lat, lon, units select (`owm_city_name` hidden)
- **Traffic**: gmaps_api_key + 2 routes (Enabled checkbox right below title, then name, origin, destination, mode select)

Tab bar with active bottom border (#7ec8e0); panels show/hide via JS click; Save + status always at bottom; `max-width: 480px` per tab panel.
JS collects all `input, select` (including hidden) and POSTs to `/config`. Unicode checkmark/cross for feedback.

## Config merge — `_merge_config(defaults, saved)`

Replaces the shallow `dict.update` merge. Used in `load_config()` and `POST /config`.
- **Scalars**: uses `saved[key]` if present, otherwise `defaults[key]`
- **Nested dicts**: recursive
- **Lists of dicts** (e.g. `traffic_routes`): for each element `i`, merges `{**default_item, **saved_item}` — saved takes priority, new fields from defaults are added automatically
- Extra keys in saved (not in defaults) are preserved (forward compat)
- DEFAULT_CONFIG is never mutated (deep copy for lists/dicts)

## Uptime Kuma — `/uptime`

Two sequential fetches to the same Uptime Kuma status page:
1. `GET /api/status-page/active` → `publicGroupList[].monitorList[]` — builds `name_map {id→name}`
2. `GET /api/status-page/heartbeat/active` → `heartbeatList {id→[heartbeat]}` — current status

`monitorList` in the `/heartbeat/active` response body is absent in this version of Uptime Kuma; names are only available in `/api/status-page/active`. Monitors with name prefix `"0-"` are excluded (group headers). Fallback: `f"Monitor {id}"` if the id is not in `name_map`.

## Beszel Docker integration — `/docker`

Beszel is a Docker container dashboard (PocketBase-based). The proxy authenticates and provides a compact endpoint for the firmware.

**Auth:**
```
POST http://<server_ip>:<beszel_port>/api/collections/users/auth-with-password
Body: {"identity": "<user>", "password": "<password>"}
Response: {"token": "...", ...}
```
Token cached in global `_beszel_token`; on 401 an automatic refresh is performed.

**Container stats:**
```
GET http://<server_ip>:<beszel_port>/api/collections/container_stats/records?sort=-created&perPage=1
Header: Authorization: <token>
Response: {"items": [{"stats": [{"n":"name","m":203.7,"c":0.5,"b":...}, ...]}]}
```
Record fields: `n`=container name, `m`=RAM MB (real RSS, not page-cached), `c`=CPU%, `b`=network.

**Note:** `memory_usage` from Glances `/api/4/containers` includes page cache (inflated). Beszel `m` is real RSS — always use Beszel for container RAM.

## Boot config fetch — `indicator_config.c`

`indicator_config_init()` registers a handler on `IP_EVENT_STA_GOT_IP`.
The handler launches `config_boot_fetch_task` (FreeRTOS, stack 4096, once only — guard `s_boot_fetched`):
1. `vTaskDelay` 3000 ms (IP stack stabilization)
2. `config_fetch_from_proxy()`: reads PROXY_IP/PORT from NVS → GET `/config` → cJSON parse → saves NVS fields (hue, server, proxy, launcher URLs+names, OWM including `owm_location`); 3× retry with 2s backoff

## "Reload config" — Settings tab Proxy

Button launches `config_reload_task` async:
1. Saves PROXY_IP/PORT to NVS
2. Calls `config_fetch_from_proxy()`
3. Calls `indicator_traffic_force_poll()` — one-shot task that immediately polls `/traffic` and updates the Traffic screen with new routes
4. Updates `lbl_cfg_status`: "OK" (#7ec8a0) or "Error" (#e07070)

UI update from task: `lv_port_sem_take()` / `lv_port_sem_give()` required.

## Hostname fetch — `indicator_system.c`

`indicator_system_init()` registers a handler on `IP_EVENT_STA_GOT_IP`.
The handler launches `system_fetch_task` (FreeRTOS, stack 4096, once only):
1. `vTaskDelay` 5000 ms (waits for indicator_config to write SERVER_IP/PORT to NVS)
2. Reads `NVS_KEY_SERVER_IP` / `NVS_KEY_SERVER_PORT` from NVS
3. GET `http://<srv_ip>:<srv_port>/api/4/system` → parses `hostname`
4. **Writes `srv_name` to NVS only if the current value is empty or equal to `"LocalServer"` (default)** — a custom name set by the user is never overwritten

`srv_name` priority: hardcoded default → proxy fetch (boot) → manual Settings → Glances hostname (boot+5s, overwrites default only).
