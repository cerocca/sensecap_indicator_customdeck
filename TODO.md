# SenseCAP Indicator Deck — TODO

## Implementation order

1. [x] **Setup** — dal repo Seeed copiare **solo** `examples/indicator_basis/` in `firmware/` (non tutta la cartella `examples/`), verifica build pulita, flash base funzionante
2. [x] **Analisi** — mappare navigazione esistente, pulsante fisico, comportamenti UI prima di toccare nulla
3. [x] **Navigazione** — aggiungere schermate vuote nello stack senza rompere nulla, verificare swipe
4. [x] **Screen Settings** — tab Wi-Fi (comportamento Seeed) + tab Hue/Server/Proxy con NVS
5. [ ] **Screen Hue** — toggle ON/OFF, poi slider luminosità
6. [ ] **Screen Sibilla** — Glances prima, poi Uptime Kuma via proxy
7. [ ] **Screen Launcher** — 4 pulsanti proxy Mac
8. [ ] **Screen AI** — placeholder tastiera touch
9. [ ] **Test completo** — verificare che clock, sensors e Wi-Fi originali funzionino ancora


## Release 0.1
- [x] Restart firmware da base pulita (indicator_basis)
- [x] Navigazione circolare bidirezionale (tutte le schermate vuote)
- [x] Screen Settings — tab Wi-Fi + Hue + Server + Proxy + AI con NVS
- [x] Sistema configurazione per nuovi utenti (quali schermate abilitare) con switch su ogni screen
- [ ] Screen Hue — toggle ON/OFF + slider luminosità
- [ ] Screen LocalServer Dashboard — Glances + Uptime Kuma via proxy
- [ ] Screen LocalServer Dashboard — aggiunta top 3 processi per CPU e/o RAM (via Glances API /api/4/processlist)
- [ ] Screen LocalServer Dashboard — Nome server LocalServer parametrico (da NVS?)
- [ ] Screen Launcher — 4 pulsanti proxy Mac
- [ ] Screen AI — placeholder tastiera touch

## Post release 0.1
- [ ] Crediti cirutech + versione in UI
- [ ] NTP/Timezone: CET/CEST, passaggio ora legale automatico
- [x] Web UI proxy Python per configurazione (`http://localhost:8765/config`)
- [ ] Uniformare documentazione: scegliere tutto italiano o tutto inglese
      (attualmente CLAUDE.md e TODO.md in italiano, README.md e SETUP.md in inglese)
- [ ] Prima di rendere pubblico il repo: decidere se aggiungere CLAUDE.md
      a .gitignore (consigliato) o pulirlo da riferimenti personali
- [ ] Valutare sostituzione schermata AI con schermata Weather (meteo locale):
      OpenWeatherMap API gratuita, icone meteo via font custom LVGL o image array,
      temperatura attuale + condizione + umidità + vento + previsione 3-5 giorni
- [ ] Valutare schermata Traffico: Google Maps Distance Matrix API
      (gratuita con crediti mensili Google Cloud, ~0.02$/mese per uso personale).
      Itinerario fisso in app_config.h, polling ogni 10 min via proxy Mac,
      UI con tempo stimato + delta vs normale + indicatore verde/giallo/rosso.
- [ ] Schermata Clock: valutare sfondo personalizzato a tema circuito PCB
      (immagine C array via LVGL image converter, sostituisce sfondo nero)

## Futuro
- [ ] **Nota schermata AI:** Visti i limiti hardware del SI (GPIO Grove su RP2040, conflitto Wi-Fi/BT per audio),
      Valutare scheda esterna XIAO (ESP32-S3 o RP2040) come coprocessore audio:
      microfono + speaker I2S gestiti da XIAO, comunicazione con SI via Grove UART.
      XIAO → Grove 2 (UART) → RP2040 SI → UART interno → ESP32-S3 → Claude API.
      Approccio analogo all'architettura interna SI (RP2040 come bridge sensori).
