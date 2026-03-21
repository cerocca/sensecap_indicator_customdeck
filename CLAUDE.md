# SenseCAP Indicator Deck — CLAUDE.md

## Progetto
Firmware custom per **SenseCAP Indicator D1S** (ESP32-S3, schermo touch 480×480).  
Toolchain: ESP-IDF + LVGL 8.x + FreeRTOS. IDE: Claude Code (CLI).  
Repo: `https://github.com/cerocca/sensecap_indicator_customdeck` (privato)  
Path locale: `/Users/ciru/sensecap_indicator_cirutech`  
Build:
```bash
source ~/esp/esp-idf/export.sh   # ogni nuovo terminale
cd sensecap_indicator_deck && idf.py build flash monitor
```

---

## Regole fondamentali

1. **Non toccare mai** le schermate originali Seeed: clock, sensors — né il pulsante fisico.
2. La schermata **settings** originale Seeed viene **sostituita** dalla settings custom (che include Wi-Fi + tutte le config).
3. Le altre schermate custom si **aggiungono** alla navigazione.
4. Aggiornamenti UI sempre con `lv_port_sem_take()` / `lv_port_sem_give()` — mai `lv_lock()`/`lv_unlock()`.
5. Buffer HTTP: minimo **2048 bytes** (1024 causa parse failure silenzioso su `/api/4/fs`).
6. Struct grandi in task FreeRTOS: sempre `static` — evita stack overflow.
7. Pattern reference per nuovi task HTTP polling: `indicator_glances.c`.

---

## Architettura

### Navigazione circolare bidirezionale
Swipe LEFT = avanti, swipe RIGHT = indietro.

```
clock ↔ sensors ↔ settings ↔ hue ↔ sibilla ↔ launcher ↔ ai ↔ (torna a clock)
```

### Schermate
| # | Nome | Tipo | Note |
|---|---|---|---|
| 1 | `screen_clock` | Originale Seeed — NON toccare | — |
| 2 | `screen_sensors` | Originale Seeed — NON toccare | CO2, temp, umidità |
| 3 | `screen_settings` | Custom (sostituisce originale Seeed) | Tab: Wi-Fi · Hue · Server · Proxy |
| 4 | `screen_hue` | Custom | Toggle ON/OFF + slider luminosità |
| 5 | `screen_sibilla` | Custom | Glances + Uptime Kuma via proxy |
| 6 | `screen_launcher` | Custom | 4 pulsanti → proxy Mac |
| 7 | `screen_ai` | Custom — placeholder | Tastiera touch ora; microfono Grove in futuro |

### Struttura file
```
sensecap_indicator_deck/
├── CMakeLists.txt                  # EXTRA_COMPONENT_DIRS = ../components
├── sdkconfig.defaults              # fix PSRAM XIP + mbedTLS dynamic — NON toccare
├── partitions.csv
└── main/
    ├── app_main.c
    ├── ui/
    │   ├── ui_manager.c/.h         # navigazione schermate
    │   ├── screen_settings.c/.h    # settings custom (Wi-Fi + config)
    │   ├── screen_hue.c/.h
    │   ├── screen_sibilla.c/.h
    │   ├── screen_launcher.c/.h
    │   └── screen_ai.c/.h
    └── model/
        ├── indicator_glances.c/.h
        ├── indicator_uptime_kuma.c/.h
        └── indicator_hue.c/.h
```

---

## Configurazione

Tutti i valori configurabili (IP, porte, API key, nomi luci, label launcher) sono centralizzati in `main/app_config.h`. La schermata Settings permette di modificarli a runtime (salvati in NVS). Fallback ai valori hardcoded se NVS vuoto.

---

## Schermata 3 — Settings (custom)

Sostituisce la schermata settings originale Seeed nella navigazione.  
Layout: **tab orizzontali** nella parte alta, contenuto sotto.

| Tab | Contenuto |
|---|---|
| **Wi-Fi** | SSID + password (comportamento originale Seeed preservato) |
| **Hue** | IP Bridge, API key, nomi 4 luci |
| **Server** | IP Sibilla, porta Glances |
| **Proxy** | IP + porta proxy Mac |
| **AI** | Claude API key, endpoint (per uso futuro con hardware Grove) |

Valori modificati salvati in **NVS** e letti all'avvio.

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
- Toggle ON/OFF al tap
- Polling stato ogni 5 secondi
- Feedback visivo su errore di rete

---

## Schermata 5 — Sibilla Dashboard

### Sorgenti dati
- **Glances API:** `http://192.168.1.69:61208/api/4/`
  - CPU: `/cpu` → `total`
  - RAM: `/mem` → `percent`
  - Disco: `/fs` → primo elemento, `percent`
  - Uptime: `/uptime`
  - Load avg: `/load`
- **Uptime Kuma:** via proxy Mac → `GET http://192.168.1.70:8765/uptime` → JSON compatto

### Layout UI — posizioni y (480×480)
```
y=65   CPU bar
y=105  RAM bar
y=145  DSK bar
y=185  Uptime
y=215  Load avg (1m · 5m · 15m) — spazio dopo ":"
y=242  Separatore
y=254  "Servizi: X/Y UP" (verde=tutti UP, arancione=qualcuno DOWN)
y=278+i×22  Righe servizi DOWN in rosso (max 6, pre-allocate, nascoste se non usate)
```

### Note critiche
- `/api/status-page/heartbeat/myuk` → ~55KB, non bufferizzabile — **NON usare**
- Usare solo proxy Mac `GET /uptime` → JSON compatto
- Parsing gruppi Uptime Kuma: escludere nomi che iniziano per `"0-"`
- `esp_http_client_read` non fa loop automatico — iterare fino a `r <= 0`

---

## Schermata 6 — Launcher

- 4 pulsanti griglia 2×2 (200×170 px): GitHub, Strava, Garmin, Intervals
- Al tap: `GET http://192.168.1.70:8765/open/<n>` (n=1..4)
- Il proxy Python apre l'URL corrispondente sul Mac

---

## Schermata 7 — AI (placeholder)

- Schermata presente ma non funzionale finché non disponibile hardware Grove
- UI attuale: tastiera touch LVGL + area testo per input/output
- **Futuro:** tap/wake word → microfono Grove → Whisper → Claude API → TTS speaker Grove
- **Vincolo futuro:** una sola connessione TLS attiva alla volta — sospendere polling Hue/Glances durante sessione vocale
- Buffer audio in PSRAM (`MALLOC_CAP_SPIRAM`)

---

## ⚠️ Warning critici

### sdkconfig — non rigenerare da zero
`sdkconfig` ha precedenza su `sdkconfig.defaults`. Se corrotto (device si resetta in loop standalone):
```bash
cd sensecap_indicator_deck && rm sdkconfig && idf.py build
grep -E "SPIRAM_XIP_FROM_PSRAM|MEMPROT_FEATURE|MBEDTLS_DYNAMIC_BUFFER" sdkconfig
```
Valori attesi: `XIP_FROM_PSRAM=y` · `MEMPROT_FEATURE=n` · `DYNAMIC_BUFFER=y`

### TLS simultanee — heap ESP32
Default mbedTLS alloca 16KB+4KB per connessione → `mbedtls_ssl_setup -0x7F00` (heap esaurito).  
Fix in `sdkconfig.defaults`: `CONFIG_MBEDTLS_DYNAMIC_BUFFER=y`.  
In ESP-IDF 5.3 solo questa opzione serve (DYNAMIC_RX/TX non esistono come kconfig symbol).

### InstrFetchProhibited crash (PSRAM + Wi-Fi)
Crash in `idle_hook_cb` subito dopo connessione Wi-Fi.  
Causa: Wi-Fi sospende cache Flash ~200µs — codice non in IRAM inaccessibile.  
Fix in `sdkconfig.defaults`:
```
CONFIG_SPIRAM_XIP_FROM_PSRAM=y
CONFIG_ESP_SYSTEM_MEMPROT_FEATURE=n
```

### Navigazione swipe LVGL — guard anti-reentrant
Obbligatorio in ogni gesture handler. Senza di esso swipe doppio durante animazione blocca la navigazione.  
Tutti i container child: `lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE)`.

```c
static void ui_event_screen_xxx(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_GESTURE) return;
    if (lv_scr_act() != ui_screen_xxx) return;          // guard anti-reentrant
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT)
        _ui_screen_change(next, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0);
    else if (dir == LV_DIR_RIGHT)
        _ui_screen_change(prev, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0);
}
```

---

## Ordine di implementazione

1. **Setup** — dal repo Seeed copiare **solo** `examples/indicator_basis/` come base (non tutta la cartella `examples/`), verifica build pulita, flash base funzionante
2. **Analisi** — mappare navigazione esistente, pulsante fisico, comportamenti UI prima di toccare nulla
3. **Navigazione** — aggiungere schermate vuote nello stack senza rompere nulla, verificare swipe
4. **Screen Settings** — tab Wi-Fi (comportamento Seeed) + tab Hue/Server/Proxy con NVS
5. **Screen Hue** — toggle ON/OFF, poi slider luminosità
6. **Screen Sibilla** — Glances prima, poi Uptime Kuma via proxy
7. **Screen Launcher** — 4 pulsanti proxy Mac
8. **Screen AI** — placeholder tastiera touch
9. **Test completo** — verificare che clock, sensors e Wi-Fi originali funzionino ancora

---

## Checklist pre-commit

- [ ] **CLAUDE.md** aggiornato con learnings della sessione ← sempre per primo
- [ ] **TODO.md** aggiornato (aggiungi/spunta task)
- [ ] **CHANGELOG.md** aggiornato (sezione `[Unreleased]`)
- [ ] **README.md** aggiornato se cambiano schermate o funzionalità
- [ ] **Caricare CLAUDE.md aggiornato nel Project** su Claude.ai (sostituire il file esistente)
