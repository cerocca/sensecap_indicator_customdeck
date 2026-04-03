# SenseCAP Indicator Deck — TODO

## Pending

- [ ] **Code review sistematica pre-pubblicazione**:
  - [x] **Fase 1** — `ui_manager.c` (gesture/guard/navigazione): fix applicati (ensure_populated mancanti, indicator_weather_init spostata)
  - [x] **Fase 2** — `model/` (task HTTP, buffer, stack, guard FreeRTOS)
  - [x] **Fase 3** — `ui/` (lazy init, lv_port_sem, event handler)
  - [x] **Fase 4** — `sensedeck_proxy.py` (endpoints, error handling, merge config)
  - [ ] **Sanity check finale**: dopo ogni fase, lanciare `/code-review` in Claude Code CLI come verifica aggiuntiva sui file modificati
- [ ] **Preparare CHANGELOG finale per prima release pubblica**: archiviare `[Unreleased]` come `[0.1.0-dev]`, scrivere voce pulita `[0.1.0]` dal punto di vista utente
- [ ] **Screenshot schermate**: fotografare il device e aggiungere immagini in `docs/screenshots/`
      (traffic.png, hue.png, sibilla.png, launcher.png, weather.png, settings.png)
- [ ] aumentare spazio titoli schermate per swipe (o altro metodo)
- [ ] NTP/Timezone: CET/CEST, passaggio ora legale automatico, sincronizzazione NTP da rifinire
- [ ] Prima di rendere pubblico il repo: decidere se aggiungere CLAUDE.md
      a .gitignore (consigliato) o pulirlo da riferimenti personali;
      anonimizzare SenseDeck_Proxy_Start.command e SenseDeck_Proxy_Stop.command
      (rimuovere path assoluti o riferimenti personali)
- [ ] **esp-aes: Failed to allocate memory su Hue polling** — al boot, il polling TLS di Hue (4 luci in sequenza) fallisce per heap DRAM insufficiente. Il device non crasha ma le luci risultano HTTP -1 al primo ciclo. Investigare: aumentare delay primo poll Hue, o serializzare le 4 richieste con pausa tra una e l'altra, o verificare se `CONFIG_MBEDTLS_DYNAMIC_BUFFER=y` è effettivamente attivo in sdkconfig.

- [x] **Tab Info in Settings**: versione firmware (`esp_app_get_description`), QR code repo (`LV_USE_QRCODE`), URL, credits "cerocca", MIT License
- [x] **Screen Weather**: fix layout — separatore sotto titolo, aggiunta sezione "Next 3 days"
- [x] **Uniformità font schermate custom** — HUE come riferimento, verificare tutte le schermate
- [x] **Screen Traffic** (slot 6): Google Maps Distance Matrix API via proxy Mac.
      Polling ogni 10 min, indicatore OK/SLOW/HEAVY, tempo stimato, delta vs normale.
      Layout adattivo single/dual route. force_poll via "Reload config".
- [x] **Setup** — dal repo Seeed copiare **solo** `examples/indicator_basis/` in `firmware/` (non tutta la cartella `examples/`), verifica build pulita, flash base funzionante
- [x] **Analisi** — mappare navigazione esistente, pulsante fisico, comportamenti UI prima di toccare nulla
- [x] **Navigazione** — aggiungere schermate vuote nello stack senza rompere nulla, verificare swipe
- [x] **Screen Settings** — tab Wi-Fi (comportamento Seeed) + tab Hue/Server/Proxy con NVS
- [x] **Screen Hue** — toggle ON/OFF, poi slider luminosità
- [x] **Screen Sibilla** — Glances ✓ · Uptime Kuma via proxy ✓
- [x] **Screen Launcher** — 4 pulsanti proxy Mac
- [x] **Screen Weather** — meteo OWM diretto: temp, icona, umidità, vento, 4 slot forecast (sostituisce AI placeholder)
- [x] **Restyle navigazione** — settings_custom su swipe UP dal clock (fuori rotazione orizzontale); swipe DOWN dal clock → ui_screen_setting Seeed; rimosso auto-navigate al boot; rimosso tab Wi-Fi da settings_custom; nascosti elementi ridondanti in sensor e setting screen
- [x] **Screen Weather**: ricontrollare layout e funzionamento completo (flash+monitor)
- [x] Rimuovere vecchio progetto: eliminare repo locale `/Users/ciru/SenseCAP_Indicator_ESP32`
      e archiviare/eliminare repo GitHub `cerocca/mySenseCAP_Indicator_ESP32` (locale eliminato; GitHub da fare manualmente)
- [x] Restart firmware da base pulita (indicator_basis)
- [x] Navigazione circolare bidirezionale (tutte le schermate vuote)
- [x] Screen Settings — tab Wi-Fi + Hue + Server + Proxy + AI + Schermate con NVS
- [x] Sistema configurazione per nuovi utenti (quali schermate abilitare) con switch su ogni screen
- [x] Screen Hue — toggle ON/OFF + slider luminosità
- [x] Screen LocalServer Dashboard — Glances + Uptime Kuma via proxy
- [x] Screen LocalServer Dashboard — top 3 container Docker per RAM (via Glances API /api/4/containers)
- [x] Screen LocalServer Dashboard — Nome server LocalServer parametrico (da NVS)
- [x] Screen Launcher — 4 pulsanti proxy Mac, nomi mnemonici NVS, Beszel Docker integration
- [x] Screen Weather — meteo OWM diretto (temp, icona ASCII, umidità, vento, 4 slot forecast 3h)
- [x] aggiornare README e SETUP su localServer Dashboard per mettere LAN server monitoring via Glances API + Uptime Kuma + Beszel e loro uso
- [x] aggiornare SETUP.md con istruzioni uso proxy e setup varie schermate
- [x] aggiungere in UI (es. tab Proxy in Settings) la riga con l'indirizzo web del config proxy: `http://localhost:8765/config/ui`
- [x] Web UI proxy Python per configurazione (`http://localhost:8765/config`)
- [x] Layout Web UI proxy `/config/ui` a 6 tab: Hue | LocalServer | Proxy | Launcher | Weather | Traffic
- [x] Uniformare documentazione: README.md e SETUP.md riscritti completamente in inglese
      (CLAUDE.md e TODO.md restano in italiano)
- [x] Eliminare le sottodirectory `components/` non utilizzate dal progetto
- [x] ~~Valutare sostituzione schermata AI con schermata Weather~~ → implementato
- [x] Test switch "Default sensor screen" — verifica skip sensors nella navigazione e auto-navigate al boot
- [x] Valutare rimozione `screen_ai.c/.h` — NON rimosso: ui.c (Seeed) ha ancora extern + riferimento
      in ui_event_screen_time (dead code, handler già sostituito da gesture_clock). Footprint: 4 byte BSS, costo zero.

## Futuro

- [ ] **Schermata Clock**: immagine di sfondo custom (es. tema circuito PCB) —
      convertire con LVGL image converter in C array, includere come `.c` nel progetto.
      **Eccezione concordata a regola #1 CLAUDE.md** (solo sfondo, no modifica logica Seeed).
      Annotare l'eccezione in CLAUDE.md al momento dell'implementazione.
- [ ] **Screen Traffic: schermata nera al primo swipe dopo boot** — logica UI riscritta seguendo pattern weather (timer 5s, `traffic_update_ui()` static, `on_screen_load_start` aggiunto); build OK, verificare con flash+monitor e route configurata. Root cause sospetta: layout adattivo show_single/show_dual — provare a forzare il layout prima che la schermata diventi visibile.
- [ ] **Nota schermata AI:** Visti i limiti hardware del SI (GPIO Grove su RP2040, conflitto Wi-Fi/BT per audio),
      Valutare scheda esterna XIAO (ESP32-S3 o RP2040) come coprocessore audio:
      microfono + speaker I2S gestiti da XIAO, comunicazione con SI via Grove UART.
      XIAO → Grove 2 (UART) → RP2040 SI → UART interno → ESP32-S3 → Claude API.
      Approccio analogo all'architettura interna SI (RP2040 come bridge sensori).
- [ ] **Screen Weather**: implementare icone PNG reali (LVGL image converter C array) — attualmente testo ASCII
