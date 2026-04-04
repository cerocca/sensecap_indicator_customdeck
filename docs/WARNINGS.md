# ⚠️ Critical warnings

### sdkconfig — do not regenerate from scratch
`sdkconfig` takes priority over `sdkconfig.defaults`. If corrupted:
```bash
cd firmware && rm sdkconfig && idf.py build
grep -E "SPIRAM_XIP_FROM_PSRAM|MEMPROT_FEATURE|MBEDTLS_DYNAMIC_BUFFER|MAIN_TASK_STACK|STA_DISCONNECTED_PM" sdkconfig
```
Expected values: `XIP_FROM_PSRAM=y` · `MEMPROT_FEATURE=n` · `DYNAMIC_BUFFER=y` · `MAIN_TASK_STACK_SIZE=16384` · `STA_DISCONNECTED_PM_ENABLE=n`

### Seeed bug — VIEW_EVENT_WIFI_ST null-check (fixed)
In `indicator_view.c`: `wifi_rssi_level_get()` out of range {1,2,3} left `p_src = NULL`.
`lv_img_set_src(obj, NULL)` caused `LoadProhibited`. Fix: guard `if (p_src != NULL)`.

### Simultaneous TLS — ESP32 heap
`CONFIG_MBEDTLS_DYNAMIC_BUFFER=y` in `sdkconfig.defaults`. In ESP-IDF 5.3 this is sufficient.

### InstrFetchProhibited crash (PSRAM + Wi-Fi)
Fix in `sdkconfig.defaults`:
```
CONFIG_SPIRAM_XIP_FROM_PSRAM=y
CONFIG_ESP_SYSTEM_MEMPROT_FEATURE=n
```

### Main task stack overflow — LVGL object creation
Init of 5+ custom LVGL screens → silent stack overflow → late crash (`IllegalInstruction`, corrupted backtrace).
Fix: `CONFIG_ESP_MAIN_TASK_STACK_SIZE=16384` in `sdkconfig.defaults`.

### Lazy init — heavy screens (mandatory pattern)
Split `screen_xxx_init()` (lightweight, at boot) and `screen_xxx_populate()` (heavy, lazy on first swipe).
```c
static bool s_xxx_populated = false;
static void ensure_xxx_populated(void) {
    if (!s_xxx_populated) { screen_xxx_populate(); s_xxx_populated = true; }
}
```
Populate happens in the same event dispatch as the gesture → content visible on first frame.
Add a handler on `ui_screen_sensor` in `ui_manager_init()` that only calls `ensure_xxx_populated()` without navigating — the Seeed handler already navigates first.

### Wi-Fi power management crash (esp_phy_enable / ppTask / pm_dream)
Mandatory fix in **two places**:
1. `sdkconfig.defaults`: `CONFIG_ESP_WIFI_STA_DISCONNECTED_PM_ENABLE=n`
2. `indicator_wifi.c`: `esp_wifi_set_ps(WIFI_PS_NONE)` after **every** `esp_wifi_start()`

```c
ESP_ERROR_CHECK(esp_wifi_start());
esp_wifi_set_ps(WIFI_PS_NONE);   // mandatory — crashes without it
```

### Large HTTP buffers (>16KB) — allocate in PSRAM, not static DRAM

Static 32KB buffers in DRAM (`.bss`) can exhaust available DRAM at boot and cause silent crashes during UI init.
**Rule**: HTTP buffers > 8KB → `heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)` in the task, with NULL guard.
```c
static char *s_cont_buf = NULL;
// At task start:
if (!s_cont_buf) {
    s_cont_buf = heap_caps_malloc(SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_cont_buf) { vTaskDelete(NULL); return; }
}
```
**Warning**: when switching from `static char buf[N]` to `static char *buf`, `sizeof(buf)` becomes `sizeof(char*) = 4` instead of `N`. Always pass the explicit constant to the HTTP function: `glances_http_get(url, buf, BUFFER_SIZE)` — never `sizeof(buf)`.

### Init order — indicator_xxx_init() must follow indicator_model_init()

All `indicator_xxx_init()` calls that register handlers on `IP_EVENT_STA_GOT_IP` must be
called in `main.c` **after** `indicator_model_init()`. `indicator_model_init()` initializes
Wi-Fi, which internally creates the default event loop. If `esp_event_handler_register` is called
before (e.g. during `ui_manager_init()` at ~1697ms), the loop does not exist yet and the handler
is never activated — no error, no log, total silence.

**Correct order in `main.c`:**
```c
indicator_model_init();    // 1. creates the default event loop (Wi-Fi init)
ui_manager_init();         // 2. screen init (do NOT put indicator_xxx_init here)
indicator_glances_init();  // 3. then all indicator_xxx_init()
indicator_uptime_kuma_init();
indicator_traffic_init();
```

**Symptom**: `ESP_LOGI("init called")` appears in the log, but `ESP_LOGI("got IP")` in the handler
never appears, even though Wi-Fi connects and other IP_EVENT handlers work.
Debugged on `indicator_traffic_init()` (session 2026-03-29).

### CONFIG_UART_ISR_IN_IRAM — incompatible with SPIRAM_XIP_FROM_PSRAM
**DO NOT add** — causes immediate boot loop. Never add to `sdkconfig.defaults`.

### Incomplete flash — "invalid segment length 0xffffffff"
Do not interrupt `idf.py flash`. Do not use `| head`. If incomplete: re-flash completely.

### LVGL swipe navigation — anti-reentrant guard
Required in every gesture handler. All child containers: `lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE)`.
```c
static void ui_event_screen_xxx(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_GESTURE) return;
    if (lv_scr_act() != ui_screen_xxx) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT)
        _ui_screen_change(next_from(idx, +1), LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0);
    else if (dir == LV_DIR_RIGHT)
        _ui_screen_change(next_from(idx, -1), LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0);
}
```

### LVGL keyboard — keyboard-aware layout in Settings

The LVGL keyboard (`lv_keyboard_create`) on a 480px screen takes up 50% = 240px by default.
All tab containers in Settings have `LV_OBJ_FLAG_SCROLLABLE` removed (required for gestures).
Result: tab content is hidden under the keyboard and cannot be scrolled.

**Fix in `ta_focused_cb` / `kb_event_cb`:**
- On open: `lv_obj_set_height(s_kb, 200)` + `lv_obj_set_height(s_tv, 280)` → content area = 236px
- `lv_obj_get_parent(ta)` returns the tab panel; temporarily add `LV_OBJ_FLAG_SCROLLABLE`
- `lv_obj_scroll_to_view_recursive(ta, LV_ANIM_ON)` brings the textarea into view
- On close: restore tabview height to 480px, `lv_obj_scroll_to_y(panel, 0, LV_ANIM_OFF)`, remove `LV_OBJ_FLAG_SCROLLABLE` if it wasn't there before

**Key press feedback:**
The default LVGL theme only changes opacity on `LV_STATE_PRESSED` — imperceptible on capacitive touch.
Fix: add explicit style at keyboard creation time:
```c
lv_obj_set_style_bg_color(s_kb, lv_color_hex(0x4a90d9), LV_PART_ITEMS | LV_STATE_PRESSED);
lv_obj_set_style_bg_opa(s_kb,   LV_OPA_COVER,           LV_PART_ITEMS | LV_STATE_PRESSED);
```

### Seeed bug — ui_event_screen_time hardcoded target (fixed)
`ui_event_screen_time` in `ui.c` had `_ui_screen_change(ui_screen_ai, ...)` hardcoded on swipe RIGHT,
ignoring screen enable flags.

**Fix:** in `ui_manager_init()`:
```c
lv_obj_remove_event_cb(ui_screen_time, ui_event_screen_time);  // removes Seeed handler
lv_obj_add_event_cb(ui_screen_time, gesture_clock, LV_EVENT_ALL, NULL);  // replaces it
```
`gesture_clock()` in `ui_manager.c`: LEFT uses `next_from(0, +1)` (with `ensure_settings_populated()` guard if destination is settings); RIGHT uses `next_from(0, -1)`; BOTTOM replicates Seeed.

**Why not add a second "override" handler:** `lv_scr_load_anim` called twice in the same tick
loads the first screen *immediately* (visual flash) then animates to the second
(`lv_disp.c:229`, `d->scr_to_load` check). The only safe approach is to *remove* the first handler.

### Boot HTTP tasks — stagger first-poll delays

Too many simultaneous HTTP/TLS tasks at boot exhaust the lwIP heap →
`thread_sem_init: out of memory` → `LoadProhibited` in `netconn_gethostbyname`.
Current values (do not reduce):
- glances: 3000ms (hardcoded)
- weather: 8000ms (WEATHER_FIRST_DELAY_MS)
- traffic: 20000ms (TRAFFIC_FIRST_DELAY_MS)

Hue TLS polling produces `esp-aes: Failed to allocate memory` on the first cycle
(4 consecutive requests) — does not cause a crash but lights show HTTP -1
at boot. CONFIG_MBEDTLS_DYNAMIC_BUFFER=y must be present in sdkconfig.defaults.

### Traffic black screen — unresolved bug (session 2026-04-02)
Symptom: Traffic screen black on first swipe after boot. Works only
after visiting Weather first.
Attempted without success: LV_EVENT_SCREEN_LOADED, 1000ms wait timer,
lv_obj_invalidate, force_redraw timer, update() in populate().
Current partial solution: pattern identical to Weather (traffic_update_ui()
static, 5s timer, indicator_traffic.c without UI calls).
Root cause not identified: Weather works with the same pattern,
Traffic does not. Suspected difference: traffic has route_count and adaptive
layout (single/dual) — weather has a fixed layout.
Next hypothesis to verify: the problem is in the adaptive layout
show_single_layout()/show_dual_layout() that shows/hides widgets —
try forcing the layout before the screen becomes visible.

### Fix Phase 2 — confirmed regression (April 2026)

The following fixes were identified by the Phase 2 code review but cause
regressions on Hue and Traffic. DO NOT apply until the culprit is isolated.

Apply ONE AT A TIME with flash and device test between each:

- [A4] indicator_hue.c — hue_cmd_task stack 4096 → 8192
- [A2] indicator_weather.c — weather_poll_task stack 4096 → 6144
- [A3] indicator_hue.c — hue_poll_task stack 6144 → 8192
- [A1] indicator_traffic.c — TRAFFIC_BUF_SIZE 512 → 2048
- [C2] indicator_config.c — remove unused content_len
- [C1] indicator_weather.c — IP_EVENT_STA_GOT_IP pattern (more invasive, apply last)

Strategy: start from C2 (least invasive), then A1, A3, A4, A2, C1.
After each fix: build → flash → test Hue toggle/slider + Traffic populate → then next fix.
