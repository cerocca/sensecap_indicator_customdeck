# Dettaglio schermate custom — SenseDeck

## Schermata 6 — Traffic

### Sorgente dati
Proxy Mac `GET /traffic` → itera `traffic_routes` abilitate, chiama Google Maps Distance Matrix API per ognuna.
Risposta: **array JSON** di route:
```json
[{"name":"Route 1","duration_sec":877,"duration_normal_sec":912,"delta_sec":-35,"distance_m":14721,"status":"ok"}, ...]
```
`status` per route: `"ok"` (delta ≤120s), `"slow"` (120-600s), `"bad"` (>600s).
Se `gmaps_api_key` vuota o nessuna route abilitata/configurata → `{"error":"not_configured"}`.

### Polling
- Ogni **10 minuti** (TRAFFIC_POLL_MS = 600000 ms)
- Primo poll: 8s dopo boot (TRAFFIC_FIRST_DELAY_MS)
- Buffer response: 512 byte
- Pattern task: vedi `indicator_glances.c`
- `indicator_traffic_force_poll()`: lancia task one-shot (`force_poll_task`) che esegue `do_traffic_poll()` immediatamente e poi si auto-cancella; chiamato da "Reload config" in Settings tab Proxy

### Pattern aggiornamento UI — identico a screen_weather
`traffic_update_ui()` è **static** — non esposta in `.h`. `indicator_traffic.c` non include
`screen_traffic.h` e non chiama mai funzioni UI (niente `lv_port_sem_take/give`).
La UI viene aggiornata esclusivamente da:
1. `traffic_update_ui()` chiamata al termine di `screen_traffic_populate()`
2. Timer LVGL ricorrente ogni **5s** (`traffic_refresh_cb`) — esegue solo se `lv_scr_act() == ui_screen_traffic`

### Struct dati
```c
typedef struct {
    char name[32];
    int  duration_sec, duration_normal_sec, delta_sec, distance_m;
    char status[8];
} traffic_route_t;

typedef struct {
    traffic_route_t routes[2];
    int     route_count;   /* 0, 1 o 2 */
    bool    valid;
    int64_t last_update_ms;
} traffic_data_t;
```

### Layout UI (480×480) — adattivo
**1 route (route_count == 1):**
- Header "Traffic" — font20, bianco, TOP_MID
- `y=55` Separatore
- `y=110` Nome route — **font20**, #aaaaaa, center, **sottolineato** (più grande dello stato)
- `y=155` Stato "OK"/"SLOW"/"HEAVY" — font16, colore dinamico (no ●)
- `y=200` Tempo stimato — font20, bianco
- `y=245` Delta — font16, colore dinamico
- `y=285` Distanza — font14, #aaaaaa
- `y=320` Separatore
- `y=345` "Configure route via proxy Web UI" — solo se `!valid`, font14, #aaaaaa
- `BOTTOM_MID -50` "Updated X min ago" — font12, #555555
- `BOTTOM_MID -25` Label errore — font12, #e07070

**2 route (route_count == 2), schermo diviso:**
- Header "Traffic" — font20, bianco
- `y=55` Separatore header
- Route 0: name(72,**font20**,underline) · status(104,font16) · duration(132,**font18**) · delta(162,font14) · distance(186,font12)
- `y=212` Separatore metà schermo
- Route 1: name(224,**font20**,underline) · status(256,font16) · duration(284,**font18**) · delta(314,font14) · distance(338,font12)
- `BOTTOM_MID` Updated / errore

**Se `!valid`:** layout singolo con "NO DATA" + label configure visibile.

### Settings tab "Traffic"
Solo info label: "Configure origin/destination at: http://\<proxy\>/config/ui" (URL dinamico da NVS).
Nessuno switch ON/OFF — lo switch Traffic è nel tab Screens.

---

## Schermata 3 — Settings (custom)

Accessibile via **swipe UP dal clock** (fuori dalla rotazione orizzontale).
Layout: tabview LVGL (44px header), 6 tab orizzontali.

| Tab | Contenuto |
|---|---|
| **Hue** | IP Bridge, API key, nomi 4 luci — textarea + Save → NVS |
| **Server** | Server IP, Server Name, Glances Port, Beszel Port, Uptime Kuma Port → NVS (5 campi) |
| **Proxy** | IP + porta proxy Mac → NVS · pulsante "Ricarica config" → fetch da proxy senza reboot · label "Config UI: http://..." con recolor LVGL (bianco + #7ec8e0), aggiornata dinamicamente da NVS |
| **Weather** | Info URL proxy Web UI (dinamico, no switch — lo switch è in Screens) |
| **Traffic** | Info URL proxy Web UI (dinamico, no switch — lo switch è in Screens) |
| **Screens** | 6 switch: Default sensor screen, Hue, LocalServer, Launcher, Weather, Traffic → aggiornano flag + NVS |

**Nota**: Tab Wi-Fi rimosso — la configurazione Wi-Fi è accessibile via swipe DOWN dal clock → `ui_screen_setting` Seeed.

- Valori letti da NVS su `SCREEN_LOAD_START`; fallback a `app_config.h` se NVS vuoto.
- Tastiera LVGL popup al tap su qualsiasi textarea; chiude su READY/CANCEL.
- Chiavi NVS max 15 char — centralizzate in `app_config.h`.

**`ui_screen_sensor` (Seeed) — personalizzazione dall'esterno in `ui_manager_init()`:**
- `ui_wifi__st_button_2` (bottone status Wi-Fi top-right + icona figlia) → `LV_OBJ_FLAG_HIDDEN`
- `ui_scrolldots2` (container 3 puntini navigazione) → `LV_OBJ_FLAG_HIDDEN`

**`ui_screen_setting` (Seeed) — personalizzazione dall'esterno:**
- Handler sostituito con `gesture_seeed_setting` (UP → clock)
- Elementi nascosti via `lv_obj_add_flag(..., LV_OBJ_FLAG_HIDDEN)` in `ui_manager_init()`:
  `ui_setting_title` (label "Setting"), `ui_setting_icon` (ingranaggio), `ui_scrolldots3` (3 puntini)
- Wi-Fi button e icona restano visibili e funzionanti

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

## Schermata 7 — Weather (sostituisce AI)

### Sorgenti dati
Il firmware chiama **OpenWeatherMap direttamente** (HTTPS), senza passare dal proxy Mac.
Il proxy serve solo per configurare i parametri (API key, lat/lon, units, location) via Web UI → saved in NVS via `indicator_config.c` al boot (GET `/config`).

- **Current:** `https://api.openweathermap.org/data/2.5/weather?lat={lat}&lon={lon}&appid={key}&units={units}`
- **Forecast:** `https://api.openweathermap.org/data/2.5/forecast?lat={lat}&lon={lon}&appid={key}&units={units}&cnt=4`

### NVS keys
| Chiave | Contenuto |
|---|---|
| `wth_api_key` | OWM API key (32 char) |
| `wth_lat` | Latitudine es. "43.7711" |
| `wth_lon` | Longitudine es. "11.2486" |
| `wth_units` | "metric" o "imperial" |
| `wth_city` | Nome città (es. "Firenze") |
| `wth_location` | Stringa location display (es. "Firenze, IT") — priorità su wth_city |
| `scr_wthr_en` | Flag abilitazione schermata |

### Location label — priorità
`update_city_label()` cerca in ordine:
1. `wth_location` (NVS) — es. "Firenze, IT"
2. `wth_city` (NVS) — es. "Firenze"
3. Fallback: lat/lon formattato — es. "43.77°N 11.25°E"

### Layout UI (480×480)
- `y=0-44` Header "Weather" — font20, **bianco**
- `y=34` Separatore header
- `y=52` Città/location — font14, #aaaaaa
- `y=74` Icona condizione — testo ASCII font20, bianco (LVGL Montserrat non supporta emoji — icone PNG sono TODO futuro)
- `y=96` Temperatura — font20, bianco
- `y=138` Feels like — font14, #888888
- `y=160` Descrizione — font16, #cccccc
- `y=186` Umidità + vento — font14, #aaaaaa
- `y=212` Separatore
- `y=222` "Next hours" — font12, #7ec8e0
- `y=245` Ora forecast (3 colonne 160px)
- `y=265` Icona forecast
- `y=283` Temp forecast
- `y=301` Separatore
- `y=311` "Next 3 days" — font12, #7ec8e0
- `y=328` Nome giorno (3 colonne 160px) — font12, #aaaaaa
- `y=343` Icona giorno — font14, bianco
- `y=358` Temp max/min — font12, #aaaaaa (formato "max°/min°")
- `LV_ALIGN_BOTTOM_MID, 0, -30` "Updated X min ago" — font12, #555555
- `LV_ALIGN_BOTTOM_MID, 0, -10` Label errore — font12, #e07070

### Icone meteo (ASCII fallback)
Mapping OWM icon code → testo breve (font Montserrat non ha emoji):
`01d`→SUN, `01n`→MOON, `02x`→PRTC, `03x`→CLD, `04x`→OCLD, `09x`→DRZL, `10x`→RAIN, `11x`→STRM, `13x`→SNOW, `50x`→FOG

### Polling
- Ogni **10 minuti** (WEATHER_POLL_MS = 600000 ms)
- Prima poll: 5 s dopo boot (WEATHER_FIRST_DELAY_MS)
- Se `wth_api_key` NVS vuota → non polla, `valid = false`
- Buffer response: 2048 byte in stack (response ~700 byte current, ~1500 byte forecast cnt=4)
- TLS: `skip_cert_common_name_check=true, use_global_ca_store=false, cert_pem=NULL` → MBEDTLS_SSL_VERIFY_NONE (stesso pattern Hue)

### Timer UI
`lv_timer_create(weather_refresh_cb, 30000, NULL)` — refresh ogni 30s nel timer cb (già in LVGL context → NO lv_port_sem_take/give nel cb stesso)
