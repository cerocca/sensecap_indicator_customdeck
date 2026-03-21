# SenseCAP Indicator Deck — TODO

## In corso
- [ ] Screen Hue — toggle ON/OFF + slider luminosità

## Release 0.1
- [x] Restart firmware da base pulita (indicator_basis)
- [x] Navigazione circolare bidirezionale (tutte le schermate vuote)
- [x] Screen Settings — tab Wi-Fi + Hue + Server + Proxy + AI con NVS
- [ ] Screen Hue — toggle ON/OFF + slider luminosità
- [ ] Screen LocalServer Dashboard — Glances + Uptime Kuma via proxy
- [ ] Screen Launcher — 4 pulsanti proxy Mac
- [ ] Screen AI — placeholder tastiera touch

## Post release 0.1
- [ ] Beep riaccende schermo
- [ ] Fix Uptime Kuma contatore servizi (17→16)
- [ ] Crediti cirutech + versione in UI
- [ ] Nome server LocalServer parametrico (da NVS)
- [ ] Schermata config Hue: scegliere quali luci mostrare
- [ ] NTP/Timezone: CET/CEST, passaggio ora legale automatico
- [ ] Web UI proxy Python per configurazione (`http://localhost:8765/config`)
- [ ] Sistema configurazione per nuovi utenti (quali schermate abilitare)
- [ ] LocalServer Dashboard: valutare aggiunta top 3 processi per CPU e/o RAM (via Glances API /api/4/processlist)
- [ ] Valutare sostituzione schermata AI con schermata Weather (meteo locale):
      OpenWeatherMap API gratuita, icone meteo via font custom LVGL o image array,
      temperatura attuale + condizione + umidità + vento + previsione 3-5 giorni

## Futuro
- [ ] Valutare scheda esterna XIAO (ESP32-S3 o RP2040) come coprocessore audio:
      microfono + speaker I2S gestiti da XIAO, comunicazione con SI via Grove UART.
      XIAO → Grove 2 (UART) → RP2040 SI → UART interno → ESP32-S3 → Claude API.
      Approccio analogo all'architettura interna SI (RP2040 come bridge sensori).

**Nota schermata AI:** Visti i limiti hardware del SI (GPIO Grove su RP2040,
conflitto Wi-Fi/BT per audio), la schermata AI potrebbe essere rimossa
in una release futura. Per ora resta come placeholder tastiera touch + Claude API via HTTPS.
