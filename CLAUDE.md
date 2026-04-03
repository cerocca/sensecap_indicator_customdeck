# SenseCAP Indicator Deck — CLAUDE.md

## Progetto
Firmware custom per **SenseCAP Indicator D1S** (ESP32-S3, schermo touch 480×480).
Toolchain: ESP-IDF + LVGL 8.x + FreeRTOS. IDE: Claude Code (CLI).
Repo: `https://github.com/cerocca/sensecap_indicator_customdeck` (privato)
Build:
```bash
source ~/esp/esp-idf/export.sh   # ogni nuovo terminale
cd firmware && idf.py build
```

> **Nota CMake**: quando si aggiungono nuovi `.c` in directory con `GLOB_RECURSE`, eseguire `idf.py reconfigure` prima di `idf.py build`.

---

## Workflow

**Flash e monitor: li esegue sempre Niwla manualmente, non Claude Code.**
Salvo diverse indicazioni esplicite, Claude Code si ferma al `idf.py build`.

Comandi di riferimento (solo per documentazione):
```bash
source ~/esp/esp-idf/export.sh
cd firmware && idf.py build flash
idf.py -p /dev/cu.usbserial-1110 monitor
```

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

### Navigazione

**Orizzontale** (swipe LEFT = avanti, swipe RIGHT = indietro):
```
clock(0) ↔ [sensors(1)] ↔ [hue(2)] ↔ [sibilla(3)] ↔ [launcher(4)] ↔ [weather(5)] ↔ [traffic(6)] ↔ (clock)
```

**Verticale dal clock:**
```
swipe UP   → screen_settings_custom  (MOVE_TOP)
swipe DOWN → ui_screen_setting Seeed (MOVE_BOTTOM)
```

`screen_settings_custom` è **fuori dalla rotazione orizzontale** — raggiungibile solo via swipe UP dal clock.
Swipe DOWN da settings_custom torna al clock (MOVE_BOTTOM).
Swipe UP da ui_screen_setting torna al clock (MOVE_TOP).

Le schermate tra `[]` sono opzionali: se disabilitate vengono saltate automaticamente.
Clock (idx 0) è sempre abilitato. Sensors (idx 1) può essere disabilitata via switch "Default sensor screen" in tab Screens (**eccezione concordata a regola #1** — la schermata resta accessibile, viene solo saltata nello swipe). Traffic (idx 6) abilitabile/disabilitabile via switch "Traffic" nel tab Screens.

**Skip logic — `next_from(idx, dir)` in `ui_manager.c`:**
```c
// Indici: 0=clock, 1=sensors, 2=hue, 3=sibilla, 4=launcher, 5=weather, 6=traffic
// settings_custom è fuori dalla tabella s_scr[]
static lv_obj_t *next_from(int cur, int dir) {
    for (int i = 1; i < N_SCREENS; i++) {
        int idx = ((cur + dir * i) % N_SCREENS + N_SCREENS) % N_SCREENS;
        if (scr_enabled(idx)) return s_scr[idx];
    }
    return s_scr[cur];
}
```
`scr_enabled(1)` ritorna `g_scr_defsens_enabled`. `scr_enabled(6)` ritorna `g_scr_traffic_enabled`.
Tutti i gesture handler usano `ensure_populated(next)` prima di navigare (helper in `ui_manager.c`).
La tabella `s_scr[7]` è popolata in `ui_manager_init()` (s_scr[6] = screen_traffic_get_screen()).

**Flag di abilitazione schermate:**
- `g_scr_hue_enabled`, `g_scr_srv_enabled`, `g_scr_lnch_enabled`, `g_scr_wthr_enabled`, `g_scr_defsens_enabled`, `g_scr_traffic_enabled` — definiti in `ui_manager.c`, esposti in `ui_manager.h`
- Caricati da NVS in `ui_manager_init()` (chiavi in `app_config.h`, default `true`)
- Aggiornati live da `screen_settings_custom.c` (tab "Screens") al toggle switch
- Salvati in NVS come `"1"`/`"0"` (2 byte, coerente con il resto delle chiavi)

**Schermata iniziale al boot:** sempre `ui_screen_time` (clock). Nessuna navigazione automatica.

**Sostituzioni handler Seeed (pattern `lv_obj_remove_event_cb` + `lv_obj_add_event_cb`):**
- `ui_event_screen_time` → `gesture_clock` (clock): aveva RIGHT hardcoded su `ui_screen_ai`
- `ui_event_screen_sensor` → `gesture_sensor` (sensors): aveva LEFT hardcoded su `ui_screen_settings_custom` (ora fuori rotazione)
- `ui_event_screen_setting` → `gesture_seeed_setting` (Seeed settings): evita doppia chiamata su `LV_DIR_TOP`
Obbligatorio per evitare doppia chiamata a `_ui_screen_change` sullo stesso tick e per correggere target hardcoded obsoleti.

### Schermate
| idx | Nome | Tipo | Note |
|-----|------|------|------|
| 0 | `screen_clock` | Originale Seeed — NON toccare | — |
| 1 | `screen_sensors` | Originale Seeed — NON toccare | CO2, temp, umidità; opzionale via flag |
| 2 | `screen_hue` | Custom | Toggle ON/OFF + slider luminosità |
| 3 | `screen_sibilla` | Custom | Glances + Uptime Kuma via proxy |
| 4 | `screen_launcher` | Custom | 4 pulsanti → proxy Mac |
| 5 | `screen_weather` | Custom | Meteo OWM: temp, icona, umidità, vento, 4 slot forecast |
| 6 | `screen_traffic` | Custom | Tempo percorrenza via Google Maps, delta vs normale |
| — | `screen_settings_custom` | Custom — fuori rotazione | Accessibile solo via swipe UP dal clock |
| — | `ui_screen_setting` | Originale Seeed — NON toccare | Accessibile via swipe DOWN dal clock |

### Struttura file
```
/Users/ciru/sensecap_indicator_cirutech/   ← root repo
├── CLAUDE.md
├── TODO.md
├── README.md
├── SETUP.md
├── CHANGELOG.md
├── sensedeck_proxy.py                     # proxy Mac — /uptime, /traffic, /open/<n>, /config, /config/ui
├── config.json                            # generato dal proxy — non in git (in .gitignore)
├── sdkconfig.defaults                     # fix PSRAM XIP + mbedTLS dynamic — NON toccare
├── docs/screenshots/                      # screenshot schermate (aggiungere manualmente)
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
        │   ├── screen_weather.c/.h
        │   └── screen_traffic.c/.h
        └── model/
            ├── indicator_config.c/.h      # fetch config dal proxy al boot (IP_EVENT_STA_GOT_IP)
            ├── indicator_glances.c/.h
            ├── indicator_uptime_kuma.c/.h
            ├── indicator_weather.c/.h
            ├── indicator_system.c/.h      # fetch hostname Glances al boot (5s delay)
            ├── indicator_hue.c/.h
            └── indicator_traffic.c/.h     # polling /traffic proxy ogni 10 min
```

---

## Documentazione estesa

Leggere sempre all'inizio di ogni sessione:
- `docs/WARNINGS.md` — warning critici sdkconfig, crash noti, bug irrisolti

Leggere se si lavora sui file indicati:
- `docs/SCREENS.md` — dettaglio schermate (layout, API, struct dati, polling)
- `docs/PROXY.md`   — proxy Python (endpoints, Web UI, merge config, Beszel)

---

## Checklist pre-commit

- [ ] **CLAUDE.md** aggiornato con learnings della sessione ← sempre per primo
- [ ] **TODO.md** aggiornato (aggiungi/spunta task)
- [ ] **CHANGELOG.md** aggiornato (sezione `[Unreleased]`)
- [ ] **README.md** aggiornato se cambiano schermate o funzionalità
- [ ] **Caricare CLAUDE.md aggiornato nel Project** su Claude.ai (sostituire il file esistente)
- [ ] Al momento della release: rinomina `[Unreleased]` in `[x.y.z] — data`,
      crea tag Git: `git tag -a vx.y.z -m "Release x.y.z" && git push origin vx.y.z`
