# SenseCAP Indicator Deck — TODO

## Future developments

- [ ] **Screen Clock**: custom background image (e.g. PCB circuit theme) 
      convert with LVGL image converter to C array, include as `.c` in the project.
      **Explicitly agreed exception to CLAUDE.md rule #1** (background only, no modification to Seeed logic).
      Annotate the exception in CLAUDE.md at implementation time.
- [ ] **Screen Traffic: black screen on first swipe after boot** — UI logic rewritten following weather pattern (5s timer, `traffic_update_ui()` static, `on_screen_load_start` added); build OK, check if happens again
- [ ] **Screen AI** Not implemented due to SI hardware limitations (Grove GPIO on RP2040, Wi-Fi/BT conflict for audio).
      Consider external XIAO board (ESP32-S3 or RP2040) as audio coprocessor:
      microphone + I2S speaker managed by XIAO, communication with SI via Grove UART.
      XIAO → Grove 2 (UART) → RP2040 SI → internal UART → ESP32-S3 → Claude API.
      Similar to the SI internal architecture (RP2040 as sensor bridge).
- [ ] **Screen Weather**: implement real PNG icons (LVGL image converter C array) — currently ASCII text
- [ ] **dir screenshots**: photograph the device and add images to `docs/screenshots/`
      (traffic.png, hue.png, server.png, launcher.png, weather.png, settings.png)
