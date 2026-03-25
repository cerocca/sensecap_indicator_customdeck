# SenseCAP Indicator Deck — CLAUDE.md

## Progetto
Firmware custom per **SenseCAP Indicator D1S** (ESP32-S3, schermo touch 480×480).  
Toolchain: ESP-IDF + LVGL 8.x + FreeRTOS. IDE: Claude Code (CLI).  
Repo: `https://github.com/cerocca/sensecap_indicator_customdeck` (privato)
Build:
```bash
source ~/esp/esp-idf/export.sh   # ogni nuovo terminale
cd firmware && idf.py build flash monitor
```

> **Nota CMake**: quando si aggiungono nuovi `.c` in directory con `GLOB_RECURSE`, eseguire `idf.py reconfigure` prima di `idf.py build`.

---

## Regole fondamentali

1. **Non toccare mai** le schermate originali Seeed: clock, sensors — né il pulsante fisico.
2. La schermata **settings** originale Seeed viene **sostituita** dalla settings custom.
3. Le altre schermate custom si **aggiungono** alla navigazione.
4. Aggiornamenti UI sempre con `lv_port_sem_take()` / `lv_port_sem_give()` — mai `lv_lock()`/`lv_unlock()`.
5. Buffer HTTP: minimo **2048 bytes** (1024 causa parse failure silenzioso su `/api/4/fs`).
6. Struct grandi in task FreeRTOS: sempre `static` — evita stack overflow.
7. Pattern reference per nuovi task HTTP polling: `indicator_glances.c`.
8. Schermate con molti widget LVGL: usare sempre il pattern **lazy init** (split `init`/`populate`).
9. **`IP_EVENT` / `IP_EVENT_STA_GOT_IP`**: includere `"esp_wifi.h"` — non `"esp_netif.h"`. Il tipo corretto per il metodo HTTP client è `esp_http_client_method_t` (non `esp_http_method_t`).

---

## Architettura

### Navigazione circolare bidirezionale
Swipe LEFT = avanti, swipe RIGHT = indietro.

```
clock ↔ sensors ↔ settings_custom ↔ [hue] ↔ [sibilla] ↔ [launcher] ↔ [ai] ↔ (torna a clock)
```

Le schermate tra `[]` sono opzionali: se disabilitate vengono saltate automaticamente.
Clock, sensors e settings_custom sono sempre visibili.

**Skip logic — `next_from(idx, dir)` in `ui_manager.c`:**
```c
// Indici: 0=clock, 1=sensors, 2=settings, 3=hue, 4=sibilla, 5=launcher, 6=ai
static lv_obj_t *next_from(int cur, int dir) {
    for (int i = 1; i < N_SCREENS; i++) {
        int idx = ((cur + dir * i) % N_SCREENS + N_SCREENS) % N_SCREENS;
        if (scr_enabled(idx)) return s_scr[idx];
    }
    return s_scr[cur];
}
```
Tutti i gesture handler usano `next_from()`. La tabella `s_scr[7]` è popolata in `ui_manager_init()` dopo gli `screen_xxx_init()`.

**Flag di abilitazione schermate:**
- `g_scr_hue_enabled`, `g_scr_srv_enabled`, `g_scr_lnch_enabled`, `g_scr_ai_enabled` — definiti in `ui_manager.c`, esposti in `ui_manager.h`
- Caricati da NVS in `ui_manager_init()` (chiavi in `app_config.h`, default `true`)
- Aggiornati live da `screen_settings_custom.c` (tab "Schermate") al toggle switch
- Salvati in NVS come `"1"`/`"0"` (2 byte, coerente con il resto delle chiavi)

### Schermate
| # | Nome | Tipo | Note |
|---|---|---|---|
| 1 | `screen_clock` | Originale Seeed — NON toccare | — |
| 2 | `screen_sensors` | Originale Seeed — NON toccare | CO2, temp, umidità |
| 3 | `screen_settings_custom` | Custom (sostituisce originale Seeed) | Tab: Wi-Fi · Hue · Server · Proxy · AI |
| 4 | `screen_hue` | Custom | Toggle ON/OFF + slider luminosità |
| 5 | `screen_sibilla` | Custom | Glances + Uptime Kuma via proxy |
| 6 | `screen_launcher` | Custom | 4 pulsanti → proxy Mac |
| 7 | `screen_ai` | Custom — placeholder | Tastiera touch ora |

### Struttura file
```
/Users/ciru/sensecap_indicator_cirutech/   ← root repo
├── CLAUDE.md
├── TODO.md
├── README.md
├── SETUP.md
├── CHANGELOG.md
├── sensedeck_proxy.py                     # proxy Mac — /uptime, /open/<n>, /config, /config/ui
├── config.json                            # generato dal proxy — non in git (in .gitignore)
├── sdkconfig.defaults                     # fix PSRAM XIP + mbedTLS dynamic — NON toccare
└── firmware/                              ← codice firmware
    ├── CMakeLists.txt                     # EXTRA_COMPONENT_DIRS = ../components
    ├── partitions.csv
    └── main/
        ├── app_main.c
        ├── app_config.h                   # defaults NVS + chiavi NVS (NON usare NVS key > 15 char)
        ├── ui/
        │   ├── ui_manager.c/.h            # navigazione schermate + init tutte le custom
        │   ├── screen_settings_custom.c/.h
        │   ├── screen_hue.c/.h
        │   ├── screen_sibilla.c/.h
        │   ├── screen_launcher.c/.h
        │   └── screen_ai.c/.h
        └── model/
            ├── indicator_config.c/.h      # fetch config dal proxy al boot (IP_EVENT_STA_GOT_IP)
            ├── indicator_glances.c/.h
            ├── indicator_uptime_kuma.c/.h
            └── indicator_hue.c/.h
```

---

## Schermata 3 — Settings (custom)

Layout: tabview LVGL (44px header), 6 tab orizzontali.

| Tab | Contenuto |
|---|---|
| **Wi-Fi** | Bottone "Configura Wi-Fi" → naviga a `ui_screen_wifi` Seeed (ritorna via `ui_screen_last`) |
| **Hue** | IP Bridge, API key, nomi 4 luci — textarea + Save → NVS |
| **Server** | IP LocalServer, porta Glances → NVS |
| **Proxy** | IP + porta proxy Mac → NVS · pulsante "Ricarica config" → fetch da proxy senza reboot |
| **AI** | Claude API key, endpoint → NVS |
| **Schermate** | 4 switch ON/OFF (Hue, LocalServer, Launcher, AI) → aggiornano `g_scr_xxx_enabled` + NVS |

- Valori letti da NVS su `SCREEN_LOAD_START`; fallback a `app_config.h` se NVS vuoto.
- Tastiera LVGL popup al tap su qualsiasi textarea; chiude su READY/CANCEL.
- `ui_screen_last` in `ui.c` è **non-static** (necessario per return da `ui_screen_wifi`).
- Chiavi NVS max 15 char — centralizzate in `app_config.h`.

---

## Schermata 4 — Hue Control

### API (Hue Bridge Local REST API v2)
- Base URL: `https://<HUE_BRIDGE_IP>/clip/v2/resource/light`
- Header: `hue-application-key: <HUE_API_KEY>`
- SSL: `skip_common_name = true`, `use_global_ca_store = false`

### Operazioni
```
GET  /clip/v2/resource/light/<ID>          # stato luce
PUT  /clip/v2/resource/light/<ID>          # body: {"on":{"on":true/false}}
PUT  /clip/v2/resource/light/<ID>          # body: {"dimming":{"brightness":80.0}}
```

### UI
- Lista 4 luci: nome + stato ON/OFF + slider luminosità (0–100%)
- Toggle ON/OFF al tap, polling ogni 5 secondi
- Feedback visivo su errore di rete

---

## Schermata 5 — LocalServer Dashboard

### Sorgenti dati
- **Glances API:** `http://<LOCALSERVER_IP>:<GLANCES_PORT>/api/4/`
  - CPU: `/cpu` → `total`
  - RAM: `/mem` → `percent`
  - Disco: `/fs` → primo elemento, `percent`
  - Uptime: `/uptime`
  - Load avg: `/load`
- **Uptime Kuma:** via proxy Mac → `GET http://<PROXY_IP>:<PROXY_PORT>/uptime` → array JSON `[{"name": "...", "up": true/false}]`; nomi da `monitorList` Uptime Kuma; gruppi "0-..." esclusi

### Layout UI — posizioni y (480×480)
```
y=65   CPU bar
y=105  RAM bar
y=145  DSK bar
y=185  Uptime
y=215  Load avg (1m · 5m · 15m) — spazio dopo ":"
y=242  Separatore orizzontale
y=254  "Top Docker (RAM):" — header, font16, #7ec8a0
y=276+i×22  Righe container Docker (i=0..2, y=276/298/320):
              nome: label sx x=10, width=330, LV_LABEL_LONG_DOT, font16, #b0c8e0
              MB:   label dx x=350, width=100, LV_TEXT_ALIGN_RIGHT, font16, #b0c8e0
y=366  "Servizi: X/Y UP" (verde=tutti UP, arancione=qualcuno DOWN), font16, white
y=388+i×16  Righe servizi DOWN in rosso (max 6, pre-allocate, nascoste se non usate), font14
```

### Note critiche
- `/api/status-page/heartbeat/myuk` → ~55KB, non bufferizzabile — **NON usare**
- Usare solo proxy Mac `GET /uptime` → JSON compatto
- Parsing gruppi Uptime Kuma: escludere nomi che iniziano per `"0-"`
- `esp_http_client_read` non fa loop automatico — iterare fino a `r <= 0`

---

## Schermata 6 — Launcher

- 4 pulsanti griglia 2×2 (200×170 px)
- Al tap: `GET http://<PROXY_IP>:<PROXY_PORT>/open/<n>` (n=1..4)
- Il proxy Python apre l'URL corrispondente sul Mac

---

## Proxy Mac — sensedeck_proxy.py

Script Python sul Mac, porta 8765. Gestisce config centralizzata e integrazione servizi locali.

### Endpoints

| Endpoint | Metodo | Descrizione |
|---|---|---|
| `/uptime` | GET | stato servizi Uptime Kuma (JSON compatto) |
| `/open/<n>` | GET | apre URL n in Firefox (n=1..4) |
| `/ping` | GET | health check |
| `/config` | GET | configurazione device (JSON, da `config.json`) |
| `/config` | POST | salva nuova config in `config.json` |
| `/config/ui` | GET | Web UI configurazione (dark theme) |

`config.json` è nella stessa directory dello script; è in `.gitignore` (non versionato).
DEFAULT_CONFIG nel proxy: 19 campi (hue bridge/api/luci/ID, server + `srv_name`, proxy, launcher URLs). Merge con defaults su POST per backward compat.

### Boot config fetch — `indicator_config.c`

`indicator_config_init()` registra un handler su `IP_EVENT_STA_GOT_IP`.
L'handler lancia `config_boot_fetch_task` (FreeRTOS, stack 4096, una sola volta — guard `s_boot_fetched`):
1. `vTaskDelay` 1500 ms (stabilizzazione stack IP)
2. `config_fetch_from_proxy()`: legge PROXY_IP/PORT da NVS → GET `/config` → cJSON parse → salva 19 campi NVS

### "Ricarica config" — tab Proxy in Settings

Bottone lancia `config_reload_task` async:
1. Salva PROXY_IP/PORT in NVS
2. Chiama `config_fetch_from_proxy()`
3. Aggiorna `lbl_cfg_status`: "OK" (#7ec8a0) o "Errore" (#e07070)

Aggiornamento UI dal task: obbligatorio `lv_port_sem_take()` / `lv_port_sem_give()`.

### Hostname fetch — `indicator_system.c`

`indicator_system_init()` registra handler su `IP_EVENT_STA_GOT_IP`.
L'handler lancia `system_fetch_task` (FreeRTOS, stack 4096, una sola volta):
1. `vTaskDelay` 5000 ms (attende che indicator_config abbia scritto SERVER_IP/PORT in NVS)
2. Legge `NVS_KEY_SERVER_IP` / `NVS_KEY_SERVER_PORT` da NVS
3. GET `http://<srv_ip>:<srv_port>/api/4/system` → parsa `hostname`
4. **Scrive `srv_name` in NVS solo se il valore attuale è vuoto o uguale a `"LocalServer"` (default)** — un nome personalizzato impostato dall'utente non viene mai sovrascritto

Priorità `srv_name`: hardcoded default → proxy fetch (boot) → manuale Settings → Glances hostname (boot+5s, sovrascrive solo il default).

---

## Schermata 7 — AI (placeholder)

- UI attuale: tastiera touch LVGL + area testo
- Futuro: valutare XIAO come coprocessore audio (vedi TODO)
- Potrebbe essere rimossa in release futura (vedi TODO)
- **Vincolo futuro:** una sola connessione TLS attiva alla volta — sospendere polling Hue/Glances durante sessione AI

---

## ⚠️ Warning critici

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
`gesture_clock()` in `ui_manager.c` replica LEFT e BOTTOM identici a Seeed; RIGHT usa `next_from(0, -1)`.

**Perché non aggiungere un secondo handler "override":** `lv_scr_load_anim` chiamato due volte
nello stesso tick carica *immediatamente* la prima schermata (flash visivo) e poi anima alla seconda
(`lv_disp.c:229`, controllo `d->scr_to_load`). L'unico modo sicuro è *rimuovere* il primo handler.

---

## Checklist pre-commit

- [ ] **CLAUDE.md** aggiornato con learnings della sessione ← sempre per primo
- [ ] **TODO.md** aggiornato (aggiungi/spunta task)
- [ ] **CHANGELOG.md** aggiornato (sezione `[Unreleased]`)
- [ ] **README.md** aggiornato se cambiano schermate o funzionalità
- [ ] **Caricare CLAUDE.md aggiornato nel Project** su Claude.ai (sostituire il file esistente)
- [ ] Al momento della release: rinomina `[Unreleased]` in `[x.y.z] — data`,
      crea tag Git: `git tag -a vx.y.z -m "Release x.y.z" && git push origin vx.y.z`
