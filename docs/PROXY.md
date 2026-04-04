# Proxy Mac — sensedeck_proxy.py

Script Python sul Mac, porta 8765. Gestisce config centralizzata e integrazione servizi locali.

## Endpoints

| Endpoint | Metodo | Descrizione |
|---|---|---|
| `/uptime` | GET | stato servizi Uptime Kuma (JSON compatto) |
| `/traffic` | GET | tempo percorrenza Google Maps Distance Matrix API |
| `/docker` | GET | top 3 container per RAM da Beszel → `[{"name":..., "mem_mb":...}]` |
| `/open/<n>` | GET | apre URL n in Firefox (n=1..4) |
| `/ping` | GET | health check |
| `/config` | GET | configurazione device (JSON, da `config.json`) |
| `/config` | POST | salva nuova config in `config.json` |
| `/config/ui` | GET | Web UI configurazione (dark theme) |

`config.json` è nella stessa directory dello script; è in `.gitignore` (non versionato).
DEFAULT_CONFIG nel proxy: campi hue bridge/api/luci/ID, server + `srv_name`, proxy, launcher URLs + nomi (`lnch_name_1..4`), Beszel (`beszel_port`, `beszel_user`, `beszel_password`), OWM (`owm_api_key`, `owm_lat`, `owm_lon`, `owm_units`, `owm_city_name`, `owm_location`), Uptime Kuma port (`uk_port`), `gmaps_api_key`, `traffic_routes` (array 2 route: `name/origin/destination/mode/enabled`). Merge con defaults su POST per backward compat.

## Web UI — `/config/ui`

Layout a **6 tab** (dark theme): **Hue** | **LocalServer** | **Proxy** | **Launcher** | **Weather** | **Traffic**
- **Hue**: bridge IP, API key, nomi 4 luci (ID UUID come hidden inputs)
- **LocalServer**: server name, IP, Glances port, UK port, Beszel port/user/password
- **Proxy**: proxy IP, proxy port
- **Launcher**: 4 nomi + URL
- **Weather**: OWM API key, location, lat, lon, units select (`owm_city_name` hidden)
- **Traffic**: gmaps_api_key + 2 route (checkbox Enabled subito sotto titolo, poi name, origin, destination, mode select)

Tab bar con bordo inferiore attivo (#7ec8e0); pannelli show/hide via JS click; Save + status sempre in basso; `max-width: 480px` per tab panel.
JS raccoglie tutti `input, select` (inclusi hidden) e fa POST `/config`. Checkmark/cross Unicode per feedback.

## Merge config — `_merge_config(defaults, saved)`

Sostituisce il merge superficiale `dict.update`. Usato in `load_config()` e `POST /config`.
- **Scalari**: usa `saved[key]` se presente, altrimenti `defaults[key]`
- **Dict annidati**: ricorsione
- **Liste di dict** (es. `traffic_routes`): per ogni elemento `i`, merge `{**default_item, **saved_item}` — saved ha priorità, campi nuovi da default vengono aggiunti automaticamente
- Chiavi extra in saved (non in defaults) vengono preservate (forward compat)
- DEFAULT_CONFIG non viene mutato (copia profonda per liste/dict)

## Uptime Kuma — `/uptime`

Due fetch sequenziali alla stessa status page di Uptime Kuma:
1. `GET /api/status-page/active` → `publicGroupList[].monitorList[]` — costruisce `name_map {id→name}`
2. `GET /api/status-page/heartbeat/active` → `heartbeatList {id→[heartbeat]}` — stato corrente

`monitorList` nel corpo di `/heartbeat/active` è assente in questa versione di Uptime Kuma; i nomi si trovano solo in `/api/status-page/active`. I monitor con nome prefisso `"0-"` vengono esclusi (intestazioni gruppo). Fallback: `f"Monitor {id}"` se l'id non è in `name_map`.

## Beszel Docker integration — `/docker`

Beszel è una dashboard per container Docker (PocketBase-based). Il proxy autentica e fornisce un endpoint compatto al firmware.

**Auth:**
```
POST http://<server_ip>:<beszel_port>/api/collections/users/auth-with-password
Body: {"identity": "<user>", "password": "<password>"}
Response: {"token": "...", ...}
```
Token cachato in `_beszel_token` globale; su 401 si esegue refresh automatico.

**Container stats:**
```
GET http://<server_ip>:<beszel_port>/api/collections/container_stats/records?sort=-created&perPage=1
Header: Authorization: <token>
Response: {"items": [{"stats": [{"n":"name","m":203.7,"c":0.5,"b":...}, ...]}]}
```
Campi record: `n`=nome container, `m`=RAM MB (RSS reale, non page-cached), `c`=CPU%, `b`=network.

**Nota:** `memory_usage` di Glances `/api/4/containers` include page cache (inflated). Beszel `m` è RSS reale — usare sempre Beszel per RAM container.

## Boot config fetch — `indicator_config.c`

`indicator_config_init()` registra un handler su `IP_EVENT_STA_GOT_IP`.
L'handler lancia `config_boot_fetch_task` (FreeRTOS, stack 4096, una sola volta — guard `s_boot_fetched`):
1. `vTaskDelay` 1500 ms (stabilizzazione stack IP)
2. `config_fetch_from_proxy()`: legge PROXY_IP/PORT da NVS → GET `/config` → cJSON parse → salva campi NVS (hue, server, proxy, launcher URL+nomi, OWM including `owm_location`)

## "Ricarica config" — tab Proxy in Settings

Bottone lancia `config_reload_task` async:
1. Salva PROXY_IP/PORT in NVS
2. Chiama `config_fetch_from_proxy()`
3. Chiama `indicator_traffic_force_poll()` — task one-shot che esegue immediatamente un poll `/traffic` e aggiorna la schermata Traffic con le nuove route
4. Aggiorna `lbl_cfg_status`: "OK" (#7ec8a0) o "Errore" (#e07070)

Aggiornamento UI dal task: obbligatorio `lv_port_sem_take()` / `lv_port_sem_give()`.

## Hostname fetch — `indicator_system.c`

`indicator_system_init()` registra handler su `IP_EVENT_STA_GOT_IP`.
L'handler lancia `system_fetch_task` (FreeRTOS, stack 4096, una sola volta):
1. `vTaskDelay` 5000 ms (attende che indicator_config abbia scritto SERVER_IP/PORT in NVS)
2. Legge `NVS_KEY_SERVER_IP` / `NVS_KEY_SERVER_PORT` da NVS
3. GET `http://<srv_ip>:<srv_port>/api/4/system` → parsa `hostname`
4. **Scrive `srv_name` in NVS solo se il valore attuale è vuoto o uguale a `"LocalServer"` (default)** — un nome personalizzato impostato dall'utente non viene mai sovrascritto

Priorità `srv_name`: hardcoded default → proxy fetch (boot) → manuale Settings → Glances hostname (boot+5s, sovrascrive solo il default).
