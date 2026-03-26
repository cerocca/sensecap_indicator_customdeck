# SenseCAP Indicator Deck — TODO

## Implementation order

1. [x] **Setup** — dal repo Seeed copiare **solo** `examples/indicator_basis/` in `firmware/` (non tutta la cartella `examples/`), verifica build pulita, flash base funzionante
2. [x] **Analisi** — mappare navigazione esistente, pulsante fisico, comportamenti UI prima di toccare nulla
3. [x] **Navigazione** — aggiungere schermate vuote nello stack senza rompere nulla, verificare swipe
4. [x] **Screen Settings** — tab Wi-Fi (comportamento Seeed) + tab Hue/Server/Proxy con NVS
5. [x] **Screen Hue** — toggle ON/OFF, poi slider luminosità
6. [x] **Screen Sibilla** — Glances ✓ · Uptime Kuma via proxy ✓
7. [x] **Screen Launcher** — 4 pulsanti proxy Mac
8. [ ] **Screen AI** — placeholder tastiera touch
9. [ ] **Test completo** — verificare che clock, sensors e Wi-Fi originali funzionino ancora


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
- [ ] Screen AI — placeholder tastiera touch

## Post release 0.1
- [ ] aumentare spazio titoli schermate per swipe (o altro metodo)
- [ ] aggiornare SETUP.md con istruzioni uso proxy e setup varie schermate
- [ ] aggiungere in UI (es. tab Proxy in Settings) la riga con l'indirizzo web del config proxy: `http://localhost:8765/config/ui`
- [ ] rendere proxy mac cliccabile
 Crediti cirutech + versione in UI
- [ ] NTP/Timezone: CET/CEST, passaggio ora legale automatico, sincronizzazione NTP da rifinire
- [x] Web UI proxy Python per configurazione (`http://localhost:8765/config`)
- [ ] Layout Web UI proxy `/config/ui` a 3 colonne:
      colonna 1: Hue (bridge IP, API key, nomi luci) |
      colonna 2: Glances + Proxy Mac |
      colonna 3: Launcher URLs + AI/Weather (placeholder)
- [ ] Uniformare documentazione: scegliere tutto italiano o tutto inglese
      (attualmente CLAUDE.md e TODO.md in italiano, README.md e SETUP.md in inglese)
- [ ] Prima di rendere pubblico il repo: decidere se aggiungere CLAUDE.md
      a .gitignore (consigliato) o pulirlo da riferimenti personali
- [ ] Eliminare le sottodirectory `components/` non utilizzate dal progetto
      (verificare quali componenti Seeed non sono referenziati da `CMakeLists.txt` e rimuoverli)
- [ ] Valutare sostituzione schermata AI con schermata Weather (meteo locale):
      OpenWeatherMap API gratuita, icone meteo via font custom LVGL o image array,
      temperatura attuale + condizione + umidità + vento + previsione 3-5 giorni
- [ ] Valutare schermata Traffico: Google Maps Distance Matrix API
      (gratuita con crediti mensili Google Cloud, ~0.02$/mese per uso personale).
      Itinerario fisso in app_config.h, polling ogni 10 min via proxy Mac,
      UI con tempo stimato + delta vs normale + indicatore verde/giallo/rosso.
- [ ] Schermata Clock: immagine di sfondo custom (es. tema circuito PCB) —
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
