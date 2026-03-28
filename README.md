<p align="center">
  <a href="https://wiki.seeedstudio.com/SenseCAP_Indicator_Get_Started/">
    <img src="https://files.seeedstudio.com/wiki/SenseCAP/SenseCAP_Indicator/SenseCAP_Indicator_1.png" width="480" alt="SenseCAP Indicator">
  </a>
</p>

<div align="center">

# SenseCAP Indicator Custom Deck

*A custom firmware turning the SenseCAP Indicator D1S into a personal home automation dashboard — inspired by the Elgato Stream Deck concept.*

</div>

<p align="center">
  <a href="https://github.com/cerocca/sensecap_indicator_customdeck/blob/main/LICENSE">
    <img src="https://img.shields.io/github/license/cerocca/sensecap_indicator_customdeck" alt="license">
  </a>
  <img src="https://img.shields.io/badge/esp--idf-v5.3.2-00b202" alt="esp-idf">
  <img src="https://img.shields.io/badge/LVGL-v8.x-blue" alt="lvgl">
  <img src="https://img.shields.io/badge/status-in--development-orange" alt="status">
</p>

---

## What is this?

This is a personal firmware project for the **SenseCAP Indicator D1S** by Seeed Studio. The goal is to transform the device into a compact, always-on home automation panel — a physical dashboard for lights, server monitoring, URL launching, and weather.

The project is built on top of the official Seeed firmware (`examples/indicator_basis`) and extends it with custom screens, without modifying any of the original functionality.

---

## The Device

The **SenseCAP Indicator D1S** is a 4-inch touch screen device powered by an **ESP32-S3** dual-core MCU, with built-in CO2, tVOC, temperature and humidity sensors, Wi-Fi, Bluetooth, and two Grove connectors.

- **MCU:** ESP32-S3 (dual-core, 240MHz)
- **Display:** 480×480px capacitive touch screen
- **Memory:** 8MB Flash, 8MB PSRAM
- **Sensors:** CO2, tVOC, temperature, humidity (built-in)
- **Connectivity:** Wi-Fi, Bluetooth, LoRa (optional)
- **Grove:** I2C + ADC interfaces

---

## Screens

All original Seeed screens are preserved. Custom screens extend the navigation without modifying any Seeed files.

**Horizontal swipe** (LEFT = forward, RIGHT = back):
```
Clock ↔ [Sensors] ↔ [Hue] ↔ [LocalServer] ↔ [Launcher] ↔ [Weather] ↔ (back to Clock)
```
Screens in `[]` are optional and can be disabled individually. Slot 2 is reserved for a future Traffic screen.

**Vertical swipe from Clock:**
- Swipe UP → **Settings** (custom config screen)
- Swipe DOWN → **Wi-Fi / Device settings** (original Seeed screen)

| # | Screen | Type | Description |
|---|--------|------|-------------|
| 1 | **Clock** | Original | Date & time display with NTP sync and CET/CEST timezone |
| 2 | **Sensors** | Original | Live CO2, tVOC, temperature, humidity readings |
| — | **Settings** | Custom | All integrations config (tabs), values saved to NVS — swipe UP from Clock |
| 3 | **Hue Control** | Custom | Toggle + brightness slider for 4 Philips Hue lights |
| 4 | **LocalServer Dashboard** | Custom | LAN server monitoring via Glances API + Uptime Kuma |
| 5 | **Launcher** | Custom | 4 buttons to open URLs on Mac via local Python proxy |
| 6 | **Weather** | Custom | Current conditions + 4-slot forecast via OpenWeatherMap |

---

## Features

### Settings
- Accessible via swipe UP from Clock (outside horizontal rotation)
- Unified settings screen with tabs: Hue · Server · Proxy · Meteo · Screens
- All values editable on-device and persisted to NVS
- Fallback to hardcoded defaults if NVS is empty
- Configuration can also be managed via the Mac proxy Web UI (`http://localhost:8765/config/ui`)
- On Wi-Fi connect, device automatically fetches latest config from the proxy (no reboot needed)
- Wi-Fi configuration accessible via swipe DOWN from Clock (original Seeed screen)

### Hue Control
- Toggle ON/OFF and adjust brightness (0–100%) for 4 configurable Philips Hue lights
- Polling every 5 seconds via local Hue Bridge (HTTPS, self-signed cert)
- Visual feedback on network error

### LocalServer Dashboard
- Real-time CPU, RAM, disk usage and load average via [Glances](https://nicolargo.github.io/glances/) REST API
- Server uptime display
- Service status (UP/DOWN) via Uptime Kuma, delivered through a local Python proxy
- Polling every 10 seconds

### Launcher
- 4 configurable buttons (2×2 grid) that open URLs on a Mac via a local Python proxy
- Fully customizable labels and endpoints

### Weather
- Current conditions: temperature, feels-like, description, humidity, wind speed
- 4-slot forecast (+0h, +3h, +6h, +9h) via OpenWeatherMap free tier
- Data fetched directly from OWM (HTTPS) — no Mac proxy dependency
- Configurable via proxy Web UI (API key, lat/lon, units); synced to device on reload
- Display refresh every 30 seconds; polling every 10 minutes

---

## Tech Stack

- **ESP-IDF** v5.3.2
- **LVGL** v8.x
- **FreeRTOS**
- **mbedTLS** (with dynamic buffer enabled)
- **cJSON** (included in ESP-IDF)
- **Philips Hue Bridge Local API v2** (HTTPS)
- **Glances REST API** (HTTP)
- **Uptime Kuma** (via local Python proxy)

---

## Based on

- [SenseCAP Indicator D1S – Product Page](https://www.seeedstudio.com/SenseCAP-Indicator-D1-p-5643.html)
- [SenseCAP Indicator ESP32 – Official Firmware](https://github.com/Seeed-Solution/SenseCAP_Indicator_ESP32) by Seeed Studio
- [Seeed Wiki – SenseCAP Indicator](https://wiki.seeedstudio.com/SenseCAP_Indicator_Get_Started/)
