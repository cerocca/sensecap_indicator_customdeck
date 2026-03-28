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

The **SenseCAP Indicator D1S** is a 4-inch touch screen device powered by an ESP32-S3 dual-core MCU, with built-in CO2, tVOC, temperature and humidity sensors, Wi-Fi, Bluetooth, and two Grove connectors.

**SenseDeck** is a custom firmware for this device that turns it into a compact, always-on home automation panel. It is built on top of the official Seeed firmware and adds a set of custom screens for lights control, server monitoring, URL launching, weather, and traffic — all swipeable from a central clock screen, without touching any original Seeed functionality.

---

## Screens

All original Seeed screens are preserved. Custom screens extend the navigation without modifying any Seeed files.

**Navigation:**
- Swipe **LEFT / RIGHT** to move between screens
- Swipe **UP** from Clock → **Settings** (custom config screen)
- Swipe **DOWN** from Clock → **Device Settings** (original Seeed Wi-Fi screen)

Screens in `[]` are optional and can be individually disabled from the Settings → Screens tab.

```
Clock ↔ [Sensors] ↔ [Hue] ↔ [LocalServer] ↔ [Launcher] ↔ [Weather] ↔ [Traffic] ↔ (back to Clock)
```

| Screen | Type | Description |
|--------|------|-------------|
| **Clock** | Original | Date & time with NTP sync and CET/CEST timezone |
| **Sensors** | Original | Live CO2, tVOC, temperature and humidity readings |
| **Hue Control** | Custom | Toggle ON/OFF + brightness slider for 4 Philips Hue lights |
| **LocalServer Dashboard** | Custom | CPU, RAM, disk, uptime, load avg; top Docker containers; service status |
| **Launcher** | Custom | 4 configurable buttons to open URLs on Mac |
| **Weather** | Custom | Current conditions + 4-slot forecast via OpenWeatherMap |
| **Traffic** | Custom | Estimated travel time + delta vs normal + green/yellow/red indicator |
| **Settings** | Custom | Tabbed config screen — accessible via swipe UP from Clock |

---

## Features

### Hue Control
Toggle ON/OFF and adjust brightness (0–100%) for 4 configurable Philips Hue lights. Polling every 5 seconds via local Hue Bridge (HTTPS, self-signed cert accepted).

### LocalServer Dashboard
Real-time CPU, RAM, disk usage, load average and uptime via [Glances](https://nicolargo.github.io/glances/) REST API. Top 3 Docker containers by RAM via [Beszel](https://github.com/henrygd/beszel). Service UP/DOWN status via [Uptime Kuma](https://github.com/louislam/uptime-kuma). *(requires SenseDeck Proxy on Mac)*

### Launcher
4 configurable buttons (2×2 grid) that open URLs on a Mac via a local Python proxy. Labels and URLs fully customizable. *(requires SenseDeck Proxy on Mac)*

### Weather
Current conditions (temperature, feels-like, description, humidity, wind speed) and 4-slot hourly forecast via OpenWeatherMap free tier. Data fetched directly from OWM over HTTPS — no proxy dependency. Refresh every 30 seconds; polling every 10 minutes.

### Traffic
Estimated travel time from a configured origin to destination, delta vs baseline (normal traffic), and a green/yellow/red indicator via Google Maps Distance Matrix API. *(requires SenseDeck Proxy on Mac)*

### Settings
Tabbed configuration screen accessible via swipe UP from Clock (outside the horizontal rotation). Tabs: **Hue**, **Server**, **Proxy**, **Weather**, **Screens**. All values editable on-device and persisted to NVS. Full configuration also available via the proxy Web UI at `http://<mac-ip>:8765/config/ui`.

---

## Tech Stack

Built with ESP-IDF, LVGL 8.x, FreeRTOS on ESP32-S3. See [SETUP.md](SETUP.md) for build instructions.

---

## License

MIT — [cerocca](https://github.com/cerocca)
