# ⚠️ Warning critici

### sdkconfig — non rigenerare da zero
`sdkconfig` ha precedenza su `sdkconfig.defaults`. Se corrotto:
```bash
cd firmware && rm sdkconfig && idf.py build
grep -E "SPIRAM_XIP_FROM_PSRAM|MEMPROT_FEATURE|MBEDTLS_DYNAMIC_BUFFER|MAIN_TASK_STACK|STA_DISCONNECTED_PM" sdkconfig
```
Valori attesi: `XIP_FROM_PSRAM=y` · `MEMPROT_FEATURE=n` · `DYNAMIC_BUFFER=y` · `MAIN_TASK_STACK_SIZE=16384` · `STA_DISCONNECTED_PM_ENABLE=n`

### Bug Seeed — VIEW_EVENT_WIFI_ST null-check (fixato)
In `indicator_view.c`: `wifi_rssi_level_get()` fuori range {1,2,3} lasciava `p_src = NULL`.
`lv_img_set_src(obj, NULL)` causava `LoadProhibited`. Fix: guard `if (p_src != NULL)`.

### TLS simultanee — heap ESP32
`CONFIG_MBEDTLS_DYNAMIC_BUFFER=y` in `sdkconfig.defaults`. In ESP-IDF 5.3 basta questa.

### InstrFetchProhibited crash (PSRAM + Wi-Fi)
Fix in `sdkconfig.defaults`:
```
CONFIG_SPIRAM_XIP_FROM_PSRAM=y
CONFIG_ESP_SYSTEM_MEMPROT_FEATURE=n
```

### Stack overflow main task — LVGL object creation
Init di 5+ schermate LVGL custom → silent stack overflow → crash tardivo (`IllegalInstruction`, backtrace corrotto).
Fix: `CONFIG_ESP_MAIN_TASK_STACK_SIZE=16384` in `sdkconfig.defaults`.

### Lazy init — schermate pesanti (pattern obbligatorio)
Split `screen_xxx_init()` (lightweight, al boot) e `screen_xxx_populate()` (heavy, lazy al primo swipe).
```c
static bool s_xxx_populated = false;
static void ensure_xxx_populated(void) {
    if (!s_xxx_populated) { screen_xxx_populate(); s_xxx_populated = true; }
}
```
La populate avviene nello stesso event dispatch del gesture → contenuto visibile al primo frame.
Aggiungere un handler su `ui_screen_sensor` in `ui_manager_init()` che chiama solo `ensure_xxx_populated()` senza navigare — il handler Seeed naviga già per primo.

### Wi-Fi power management crash (esp_phy_enable / ppTask / pm_dream)
Fix obbligatorio in **due posti**:
1. `sdkconfig.defaults`: `CONFIG_ESP_WIFI_STA_DISCONNECTED_PM_ENABLE=n`
2. `indicator_wifi.c`: `esp_wifi_set_ps(WIFI_PS_NONE)` dopo **ogni** `esp_wifi_start()`

```c
ESP_ERROR_CHECK(esp_wifi_start());
esp_wifi_set_ps(WIFI_PS_NONE);   // obbligatorio — crash senza
```

### Buffer HTTP grandi (>16KB) — allocare in PSRAM, non in DRAM statica

Buffer statici da 32KB in DRAM (`.bss`) possono esaurire la DRAM disponibile al boot e causare crash silenziosi nell'init UI.
**Regola**: buffer HTTP > 8KB → `heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)` nel task, con guard su NULL.
```c
static char *s_cont_buf = NULL;
// All'inizio del task:
if (!s_cont_buf) {
    s_cont_buf = heap_caps_malloc(SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_cont_buf) { vTaskDelete(NULL); return; }
}
```
**Attenzione**: quando si passa da `static char buf[N]` a `static char *buf`, `sizeof(buf)` diventa `sizeof(char*) = 4` invece di `N`. Passare sempre la costante esplicita alla funzione HTTP: `glances_http_get(url, buf, BUFFER_SIZE)` — mai `sizeof(buf)`.

### Ordine init — indicator_xxx_init() deve seguire indicator_model_init()

Tutti gli `indicator_xxx_init()` che registrano handler su `IP_EVENT_STA_GOT_IP` devono essere
chiamati in `main.c` **dopo** `indicator_model_init()`. `indicator_model_init()` inizializza il
Wi-Fi, che crea internamente il default event loop. Se `esp_event_handler_register` viene chiamato
prima (es. durante `ui_manager_init()` a ~1697ms), il loop non esiste ancora e l'handler non
viene mai attivato — nessun errore, nessun log, silenzio totale.

**Ordine corretto in `main.c`:**
```c
indicator_model_init();    // 1. crea il default event loop (Wi-Fi init)
ui_manager_init();         // 2. init schermate (NON mettere indicator_xxx_init qui)
indicator_glances_init();  // 3. poi tutti gli indicator_xxx_init()
indicator_uptime_kuma_init();
indicator_traffic_init();
```

**Sintomo**: `ESP_LOGI("init called")` appare nel log, ma `ESP_LOGI("got IP")` nell'handler
non appare mai, nonostante il Wi-Fi si connetta e altri handler IP_EVENT funzionino.
Debuggato su `indicator_traffic_init()` (sessione 2026-03-29).

### CONFIG_UART_ISR_IN_IRAM — incompatibile con SPIRAM_XIP_FROM_PSRAM
**NON aggiungere** — causa boot loop immediato. NON aggiungere mai a `sdkconfig.defaults`.

### Flash incompleto — "invalid segment length 0xffffffff"
Non interrompere `idf.py flash`. Non usare `| head`. Se incompleto: ri-flashare completamente.

### Navigazione swipe LVGL — guard anti-reentrant
Obbligatorio in ogni gesture handler. Tutti i container child: `lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE)`.
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

### LVGL keyboard — layout keyboard-aware in Settings

La tastiera LVGL (`lv_keyboard_create`) su uno schermo 480px occupa di default il 50% = 240px.
Tutti i tab container in Settings hanno `LV_OBJ_FLAG_SCROLLABLE` rimosso (necessario per i gesture).
Risultato: il contenuto del tab viene nascosto sotto la tastiera e non è scrollabile.

**Fix in `ta_focused_cb` / `kb_event_cb`:**
- All'apertura: `lv_obj_set_height(s_kb, 200)` + `lv_obj_set_height(s_tv, 280)` → area content = 236px
- `lv_obj_get_parent(ta)` restituisce il tab panel; aggiungere temporaneamente `LV_OBJ_FLAG_SCROLLABLE`
- `lv_obj_scroll_to_view_recursive(ta, LV_ANIM_ON)` porta la textarea in vista
- Alla chiusura: ripristinare height tabview a 480px, `lv_obj_scroll_to_y(panel, 0, LV_ANIM_OFF)`, rimuovere `LV_OBJ_FLAG_SCROLLABLE` se non era presente prima

**Pressed feedback tasti:**
Il default theme LVGL cambia solo l'opacità su `LV_STATE_PRESSED` — impercettibile su touch capacitivo.
Fix: aggiungere stile esplicito al momento della creazione della tastiera:
```c
lv_obj_set_style_bg_color(s_kb, lv_color_hex(0x4a90d9), LV_PART_ITEMS | LV_STATE_PRESSED);
lv_obj_set_style_bg_opa(s_kb,   LV_OPA_COVER,           LV_PART_ITEMS | LV_STATE_PRESSED);
```

### Bug Seeed — ui_event_screen_time target hardcoded (fixato)
`ui_event_screen_time` in `ui.c` aveva `_ui_screen_change(ui_screen_ai, ...)` hardcoded su swipe RIGHT,
ignorando i flag di abilitazione schermate.

**Fix:** in `ui_manager_init()`:
```c
lv_obj_remove_event_cb(ui_screen_time, ui_event_screen_time);  // rimuove handler Seeed
lv_obj_add_event_cb(ui_screen_time, gesture_clock, LV_EVENT_ALL, NULL);  // sostituisce
```
`gesture_clock()` in `ui_manager.c`: LEFT usa `next_from(0, +1)` (con guard `ensure_settings_populated()` se destinazione è settings); RIGHT usa `next_from(0, -1)`; BOTTOM replica Seeed.

**Perché non aggiungere un secondo handler "override":** `lv_scr_load_anim` chiamato due volte
nello stesso tick carica *immediatamente* la prima schermata (flash visivo) e poi anima alla seconda
(`lv_disp.c:229`, controllo `d->scr_to_load`). L'unico modo sicuro è *rimuovere* il primo handler.

### Task HTTP boot — diluire i delay di primo poll

Troppi task HTTP/TLS simultanei al boot esauriscono la heap lwIP →
`thread_sem_init: out of memory` → `LoadProhibited` in `netconn_gethostbyname`.
Valori attuali (non ridurre):
- glances: 3000ms (hardcoded)
- weather: 8000ms (WEATHER_FIRST_DELAY_MS)
- traffic: 20000ms (TRAFFIC_FIRST_DELAY_MS)

Hue polling TLS produce `esp-aes: Failed to allocate memory` al primo ciclo
(4 richieste consecutive) — non causa crash ma le luci risultano HTTP -1
al boot. CONFIG_MBEDTLS_DYNAMIC_BUFFER=y deve essere presente in sdkconfig.defaults.

### Traffic black screen — bug irrisolto (sessione 2026-04-02)
Sintomo: schermata Traffic nera al primo swipe dopo boot. Funziona solo
passando prima da Weather.
Tentate senza successo: LV_EVENT_SCREEN_LOADED, timer wait 1000ms,
lv_obj_invalidate, force_redraw timer, update() in populate().
Soluzione parziale attuale: pattern identico a Weather (traffic_update_ui()
static, timer 5s, indicator_traffic.c senza chiamate UI).
Root cause non identificata: Weather funziona con lo stesso pattern,
Traffic no. Differenza sospetta: traffic ha route_count e layout
adattivo (single/dual) — weather ha layout fisso.
Prossima ipotesi da verificare: il problema è nel layout adattivo
show_single_layout()/show_dual_layout() che mostra/nasconde widget —
provare a forzare il layout prima che la schermata diventi visibile.

### Fix Fase 2 — regressione confermata (aprile 2026)

I seguenti fix sono stati identificati dalla code review Fase 2 ma causano
regressioni su Hue e Traffic. NON applicare finché non viene isolato il colpevole.

Fix da applicare UNO ALLA VOLTA con flash e test su device tra l'uno e l'altro:

- [A4] indicator_hue.c — hue_cmd_task stack 4096 → 8192
- [A2] indicator_weather.c — weather_poll_task stack 4096 → 6144
- [A3] indicator_hue.c — hue_poll_task stack 6144 → 8192
- [A1] indicator_traffic.c — TRAFFIC_BUF_SIZE 512 → 2048
- [C2] indicator_config.c — rimuovi content_len unused
- [C1] indicator_weather.c — pattern IP_EVENT_STA_GOT_IP (più invasivo, applicare ultimo)

Strategia: iniziare da C2 (meno invasivo), poi A1, A3, A4, A2, C1.
Dopo ogni fix: build → flash → test Hue toggle/slider + Traffic popolamento → poi fix successivo.
