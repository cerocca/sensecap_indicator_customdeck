# SenseCAP Indicator Deck — TODO

## Implementation order

1. [x] **Setup** — dal repo Seeed copiare **solo** `examples/indicator_basis/` in `firmware/` (non tutta la cartella `examples/`), verifica build pulita, flash base funzionante
2. [x] **Analisi** — mappare navigazione esistente, pulsante fisico, comportamenti UI prima di toccare nulla
3. [x] **Navigazione** — aggiungere schermate vuote nello stack senza rompere nulla, verificare swipe
4. [x] **Screen Settings** — tab Wi-Fi (comportamento Seeed) + tab Hue/Server/Proxy con NVS
5. [x] **Screen Hue** — toggle ON/OFF, poi slider luminosità
6. [x] **Screen Sibilla** — Glances ✓ · Uptime Kuma via proxy ✓
7. [x] **Screen Launcher** — 4 pulsanti proxy Mac
8. [x] **Screen Weather** — meteo OWM diretto: temp, icona, umidità, vento, 4 slot forecast (sostituisce AI placeholder)
9. [x] **Restyle navigazione** — settings_custom su swipe UP dal clock (fuori rotazione orizzontale); swipe DOWN dal clock → ui_screen_setting Seeed; rimosso auto-navigate al boot; rimosso tab Wi-Fi da settings_custom; nascosti elementi ridondanti in sensor e setting screen
10. [ ] **Test completo** — verificare che clock, sensors e Wi-Fi originali funzionino ancora


## Release 0.1
- [x] Restart firmware da base pulita (indicator_basis)
- [x] Navigazione circolare bidirezionale (tutte le schermate vuote)
- [x] Screen Settings — tab Wi-Fi + Hue + Server + Proxy + AI con NVS
- [x] Sistema configurazione per nuovi utenti (quali schermate abilitare) con switch su ogni screen
- [x] Screen Hue — toggle ON/OFF + slider luminosità
- [x] Screen LocalServer Dashboard — Glances + Uptime Kuma via proxy
- [x] Screen LocalServer Dashboard — top 3 container Docker per RAM (via Glances API /api/4/containers)
- [x] Screen LocalServer Dashboard — Nome server LocalServer parametrico (da NVS)
- [x] Screen Launcher — 4 pulsanti proxy Mac, nomi mnemonici NVS, Beszel Docker integration
- [x] Screen Weather — meteo OWM diretto (temp, icona ASCII, umidità, vento, 4 slot forecast 3h)

## Post release 0.1
- [x] aggiornare README e SETUP su localServer Dashboard per mettere  LAN server monitoring via Glances API + Uptime Kuma + Beszel e loro uso
- [ ] aumentare spazio titoli schermate per swipe (o altro metodo)
- [x] aggiornare SETUP.md con istruzioni uso proxy e setup varie schermate
- [x] aggiungere in UI (es. tab Proxy in Settings) la riga con l'indirizzo web del config proxy: `http://localhost:8765/config/ui`
- [ ] NTP/Timezone: CET/CEST, passaggio ora legale automatico, sincronizzazione NTP da rifinire
- [x] Web UI proxy Python per configurazione (`http://localhost:8765/config`)
- [x] Layout Web UI proxy `/config/ui` a 3 colonne: Hue / LocalServer+Proxy / Launcher+Weather
- [x] Uniformare documentazione: README.md e SETUP.md riscritti completamente in inglese
      (CLAUDE.md e TODO.md restano in italiano)
- [ ] Prima di rendere pubblico il repo: decidere se aggiungere CLAUDE.md
      a .gitignore (consigliato) o pulirlo da riferimenti personali;
      anonimizzare SenseDeck_Proxy_Start.command e SenseDeck_Proxy_Stop.command
      (rimuovere path assoluti o riferimenti personali)
- [x] Eliminare le sottodirectory `components/` non utilizzate dal progetto
      (liblorahub, radio_drivers, smtc_ral aggiunti a EXCLUDE_COMPONENTS — LoRaWAN già escluso; bsp/bus/i2c_devices/iot_button/lora/lvgl necessari)
- [x] ~~Valutare sostituzione schermata AI con schermata Weather~~ → implementato
- [ ] Screen Weather: icone PNG reali (LVGL image converter C array) — attualmente testo ASCII
- [x] Test switch "Default sensor screen" — verifica skip sensors nella navigazione e auto-navigate al boot
- [x] Valutare rimozione `screen_ai.c/.h` — NON rimosso: ui.c (Seeed) ha ancora extern + riferimento
      in ui_event_screen_time (dead code, handler già sostituito da gesture_clock). Footprint: 4 byte BSS, costo zero.
- [ ] **Screen Traffic** (slot 2, riservato): Google Maps Distance Matrix API
      (gratuita con crediti mensili Google Cloud, ~0.02$/mese per uso personale).
      Itinerario fisso in app_config.h, polling ogni 10 min via proxy Mac,
      UI con tempo stimato + delta vs normale + indicatore verde/giallo/rosso.
- [ ] Schermata Clock: immagine di sfondo custom (es. tema circuito PCB) + crediti cirutech + versione in UI —
      convertire con LVGL image converter in C array, includere come `.c` nel progetto.
      **Eccezione concordata a regola #1 CLAUDE.md** (solo sfondo, no modifica logica Seeed).
      Annotare l'eccezione in CLAUDE.md al momento dell'implementazione.

- [ ] Rimuovere vecchio progetto: eliminare repo locale `/Users/ciru/SenseCAP_Indicator_ESP32`
      e archiviare/eliminare repo GitHub `cerocca/mySenseCAP_Indicator_ESP32`

## Futuro
- [ ] **Nota schermata AI:** Visti i limiti hardware del SI (GPIO Grove su RP2040, conflitto Wi-Fi/BT per audio),
      Valutare scheda esterna XIAO (ESP32-S3 o RP2040) come coprocessore audio:
      microfono + speaker I2S gestiti da XIAO, comunicazione con SI via Grove UART.
      XIAO → Grove 2 (UART) → RP2040 SI → UART interno → ESP32-S3 → Claude API.
      Approccio analogo all'architettura interna SI (RP2040 come bridge sensori).
