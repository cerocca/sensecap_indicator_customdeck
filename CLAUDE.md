# SenseCAP Indicator Deck — CLAUDE.md

## Project
Custom firmware for **SenseCAP Indicator D1S** (ESP32-S3, 480×480 touch display).
Toolchain: ESP-IDF + LVGL 8.x + FreeRTOS. IDE: Claude Code (CLI).
Repo: `https://github.com/cerocca/sensecap_indicator_customdeck`
Build:
```bash
source ~/esp/esp-idf/export.sh   # every new terminal
cd firmware && idf.py build
```

> **CMake note**: when adding new `.c` files in directories using `GLOB_RECURSE`, run `idf.py reconfigure` before `idf.py build`.

---

## Workflow

**Flash and monitor: always run manually by the developer, not Claude Code.**
Unless explicitly instructed otherwise, Claude Code stops at `idf.py build`.

Reference commands (documentation only):
```bash
source ~/esp/esp-idf/export.sh
cd firmware && idf.py build flash
idf.py -p /dev/cu.usbserial-XXXX monitor
```

---

## Core rules

1. **Never touch** the original Seeed screens: clock, sensors — or the physical button.
2. The original Seeed **settings** screen is **replaced** by the custom settings screen.
3. Custom screens are **added** to the navigation.
4. UI updates always use `lv_port_sem_take()` / `lv_port_sem_give()` — never `lv_lock()`/`lv_unlock()`.
5. HTTP buffers: minimum **2048 bytes** (1024 causes silent parse failure on `/api/4/fs`).
6. Large structs in FreeRTOS tasks: always `static` — prevents stack overflow.
7. Reference pattern for new HTTP polling tasks: `indicator_glances.c`.
8. Screens with many LVGL widgets: always use the **lazy init** pattern (split `init`/`populate`).
9. **`IP_EVENT` / `IP_EVENT_STA_GOT_IP`**: include `"esp_wifi.h"` — not `"esp_netif.h"`. Correct type for the HTTP client method is `esp_http_client_method_t` (not `esp_http_method_t`).
10. **`LV_USE_QRCODE=y`** enabled in `firmware/sdkconfig` (not in `sdkconfig.defaults`) — required for the Info tab in `screen_settings_custom`. If `sdkconfig` is regenerated from defaults, re-enable manually.
11. **`indicator_hue.c` — `hue_poll_task`**: initial delay **10s** (waits for other TLS connections at boot to complete); **200ms** pause between the 4 consecutive HTTPS requests — prevents `esp-aes: Failed to allocate memory` from TLS heap contention.
12. **`indicator_config.c` — boot fetch**: initial delay **3000ms** (staggered from other HTTP tasks at boot); retry **3×** with **2000ms** backoff — `config_fetch_from_proxy()` returns `int` (0=OK, -1=error), not `esp_err_t`. Constants: `CFG_MAX_RETRIES 3`, `CFG_RETRY_DELAY_MS 2000`.
13. **`sensedeck_proxy.py` — `/uptime`**: `monitorList` is absent from `/api/status-page/heartbeat/active` in this version of Uptime Kuma. Names are found in `/api/status-page/active` → `publicGroupList[].monitorList[]`. The proxy makes two sequential fetches: first `/active` to build `name_map {id→name}`, then `/heartbeat/active` for status. See `docs/PROXY.md` for details.
14. **`indicator_city` — DISABLED**: `indicator_city_init()` and `#include "indicator_city.h"` commented out in `indicator_model.c`. Causes lwIP OOM crash at boot (`lwip_arch: thread_sem_init: out of memory` → Guru Meditation LoadProhibited in `indicator_city.c:216`) due to `getaddrinfo`. Widgets `ui_location`, `ui_location_Icon`, `ui_city` are hidden in `ui_manager_init()` via `LV_OBJ_FLAG_HIDDEN` (without touching `ui.c`). Do not re-enable without first investigating the lwIP memory budget at boot.
15. **`indicator_uptime_kuma.c` — `/uptime` buffer**: `UK_BUF_SIZE = 8192` bytes, allocato in PSRAM con `heap_caps_malloc`. Con 10+ monitor la risposta supera i 2 KB → JSON troncato → parse failure → lista vuota. La callback è invocata **sempre** (anche se `total == 0`) per resettare la UI su failure.
17. **`sensedeck_proxy.py` — `_beszel_renew_loop`**: dopo `_BESZEL_MAX_RETRIES` (20) tentativi falliti, entra in un **retry infinito** ogni `_BESZEL_RETRY_INTERVAL` secondi finché il token non viene recuperato, poi torna al ciclo normale dei 240s. Non usare `time.sleep(60)` fisso: su server down prolungato il token non si recupererebbe mai rapidamente al ritorno online.
18. **`sensedeck_proxy.py` — auto-restart via launchd**: `_record_error()` chiama `os._exit(0)` al 10° errore consecutivo. Il processo è gestito da **launchd** tramite `~/Library/LaunchAgents/com.cirutech.sensedeck-proxy.plist` (`KeepAlive=true`, `ThrottleInterval=5s`): launchd riavvia Python da zero con file descriptor puliti ad ogni uscita (qualsiasi exit code). `SenseDeck_Proxy_Start.command` esegue `launchctl unload` + `launchctl load`; `SenseDeck_Proxy_Stop.command` esegue `launchctl unload`. Questo risolve il problema dei socket corrotti ereditati dal wrapper bash: launchd è il processo padre e non eredita connessioni di rete dal figlio precedente. Non usare il wrapper bash loop: i socket corrotti di en6 venivano ereditati dal nuovo processo Python.
16. **NTP / POSIX TZ string**: timezone is applied as a POSIX TZ string via `setenv("TZ", ...) + tzset()`. The string is composed by `__tz_apply_from_cfg()` from `zone` (UTC offset) and `daylight` (DST flag) saved by Seeed in the NVS blob `"time-cfg"`. Format: `"STD-{n}DST,M3.5.0,M10.5.0/3"` if DST ON (EU rules), `"STD-{n}"` if DST OFF (sign inverted for POSIX). Called after `__time_cfg()` in `indicator_time_init()` and in `VIEW_EVENT_TIME_CFG_APPLY`. No extra NVS keys, no new widgets: uses existing Seeed controls (UTC offset + DST switch).

---

## Explicit exceptions to rule #1

- **`ui_screen_time` (Seeed Clock)** — custom background only, no logic changes (see Future TODO).

---

## Architecture

### Navigation

**Horizontal** (swipe LEFT = forward, swipe RIGHT = back):
```
clock(0) ↔ [sensors(1)] ↔ [hue(2)] ↔ [server(3)] ↔ [launcher(4)] ↔ [weather(5)] ↔ [traffic(6)] ↔ (clock)
```

**Vertical from clock:**
```
swipe UP   → screen_settings_custom  (MOVE_TOP)
swipe DOWN → ui_screen_setting Seeed (MOVE_BOTTOM)
```

`screen_settings_custom` is **outside the horizontal rotation** — reachable only via swipe UP from clock.
Swipe DOWN from settings_custom returns to clock (MOVE_BOTTOM).
Swipe UP from ui_screen_setting returns to clock (MOVE_TOP).

Screens in `[]` are optional: if disabled they are automatically skipped.
Clock (idx 0) is always enabled. Sensors (idx 1) can be disabled via the "Default sensor screen" switch in the Screens tab (**agreed exception to rule #1** — the screen remains accessible, only skipped in swipe). Traffic (idx 6) can be enabled/disabled via the "Traffic" switch in the Screens tab.

**Skip logic — `next_from(idx, dir)` in `ui_manager.c`:**
```c
// Indices: 0=clock, 1=sensors, 2=hue, 3=server, 4=launcher, 5=weather, 6=traffic
// settings_custom is outside the s_scr[] table
static lv_obj_t *next_from(int cur, int dir) {
    for (int i = 1; i < N_SCREENS; i++) {
        int idx = ((cur + dir * i) % N_SCREENS + N_SCREENS) % N_SCREENS;
        if (scr_enabled(idx)) return s_scr[idx];
    }
    return s_scr[cur];
}
```
`scr_enabled(1)` returns `g_scr_defsens_enabled`. `scr_enabled(6)` returns `g_scr_traffic_enabled`.
All gesture handlers call `ensure_populated(next)` before navigating (helper in `ui_manager.c`).
The `s_scr[7]` table is populated in `ui_manager_init()` (s_scr[6] = screen_traffic_get_screen()).

**Screen enable flags:**
- `g_scr_hue_enabled`, `g_scr_srv_enabled`, `g_scr_lnch_enabled`, `g_scr_wthr_enabled`, `g_scr_defsens_enabled`, `g_scr_traffic_enabled` — defined in `ui_manager.c`, exposed in `ui_manager.h`
- Loaded from NVS in `ui_manager_init()` (keys in `app_config.h`, default `true`)
- Updated live by `screen_settings_custom.c` (tab "Screens") on toggle switch
- Saved to NVS as `"1"`/`"0"` (2 bytes, consistent with other keys)

**Boot screen:** always `ui_screen_time` (clock). No automatic navigation.

**Seeed handler replacements (pattern `lv_obj_remove_event_cb` + `lv_obj_add_event_cb`):**
- `ui_event_screen_time` → `gesture_clock` (clock): had RIGHT hardcoded to `ui_screen_ai`
- `ui_event_screen_sensor` → `gesture_sensor` (sensors): had LEFT hardcoded to `ui_screen_settings_custom` (now outside rotation)
- `ui_event_screen_setting` → `gesture_seeed_setting` (Seeed settings): prevents double call on `LV_DIR_TOP`
Required to avoid double `_ui_screen_change` call on the same tick and to fix obsolete hardcoded targets.

### Screens
| idx | Name | Type | Notes |
|-----|------|------|-------|
| 0 | `screen_clock` | Original Seeed — DO NOT touch | — |
| 1 | `screen_sensors` | Original Seeed — DO NOT touch | CO2, temp, humidity; optional via flag |
| 2 | `screen_hue` | Custom | ON/OFF toggle + brightness slider |
| 3 | `screen_server` | Custom | Glances + Uptime Kuma via proxy; top 5 Docker containers by RAM |
| 4 | `screen_launcher` | Custom | 4 buttons → Mac proxy |
| 5 | `screen_weather` | Custom | OWM weather: temp, icon, humidity, wind, 4 forecast slots |
| 6 | `screen_traffic` | Custom | Travel time via Google Maps, delta vs normal |
| — | `screen_settings_custom` | Custom — outside rotation | Reachable only via swipe UP from clock. Tabs: Hue \| Server \| Proxy \| Weather \| Traffic \| Screens \| **Info** (firmware version, repo QR code, credits) |
| — | `ui_screen_setting` | Original Seeed — DO NOT touch | Reachable via swipe DOWN from clock |

### File structure
```
<project_root>/
├── CLAUDE.md
├── TODO.md
├── README.md
├── SETUP.md
├── CHANGELOG.md
├── sensedeck_proxy.py                     # Mac proxy — /uptime, /traffic, /open/<n>, /config, /config/ui
├── config.json                            # generated by proxy — not in git (.gitignore)
├── sdkconfig.defaults                     # PSRAM XIP + mbedTLS dynamic fixes — DO NOT touch
├── docs/screenshots/                      # screen screenshots (add manually)
└── firmware/                              ← firmware code
    ├── CMakeLists.txt                     # EXTRA_COMPONENT_DIRS = ../components
    ├── partitions.csv
    └── main/
        ├── app_main.c
        ├── app_config.h                   # NVS defaults + NVS keys (DO NOT use NVS key > 15 chars)
        ├── ui/
        │   ├── ui_manager.c/.h            # screen navigation + init all custom screens
        │   ├── screen_settings_custom.c/.h
        │   ├── screen_hue.c/.h
        │   ├── screen_server.c/.h
        │   ├── screen_launcher.c/.h
        │   ├── screen_weather.c/.h
        │   └── screen_traffic.c/.h
        └── model/
            ├── indicator_config.c/.h      # config fetch from proxy at boot (IP_EVENT_STA_GOT_IP)
            ├── indicator_glances.c/.h
            ├── indicator_uptime_kuma.c/.h
            ├── indicator_weather.c/.h
            ├── indicator_system.c/.h      # Glances hostname fetch at boot (5s delay)
            ├── indicator_hue.c/.h
            └── indicator_traffic.c/.h     # /traffic proxy polling every 10 min
```

---

## Extended documentation

Always read at the start of each session:
- `docs/WARNINGS.md` — critical sdkconfig warnings, known crashes, unresolved bugs

Read when working on the indicated files:
- `docs/SCREENS.md` — screen details (layout, API, data structs, polling)
- `docs/PROXY.md`   — Python proxy (endpoints, Web UI, config merge, Beszel)

---

## Pre-commit checklist

- [ ] **CLAUDE.md** updated with session learnings ← always first
- [ ] **TODO.md** updated (add/check tasks)
- [ ] **CHANGELOG.md** updated (`[Unreleased]` section)
- [ ] **README.md** updated if screens or features change
- [ ] At release time: rename `[Unreleased]` to `[x.y.z] — date`,
      create Git tag: `git tag -a vx.y.z -m "Release x.y.z" && git push origin vx.y.z`
