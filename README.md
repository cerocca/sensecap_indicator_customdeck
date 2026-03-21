<p align="center">
  <a href="https://wiki.seeedstudio.com/SenseCAP_Indicator_Get_Started/">
    <img src="https://files.seeedstudio.com/wiki/SenseCAP/SenseCAP_Indicator/SenseCAP_Indicator_1.png" width="480" alt="SenseCAP Indicator">
  </a>
</p>

<div align="center">

# SenseCAP Indicator Deck

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

This is a personal firmware project for the **SenseCAP Indicator D1S** by Seeed Studio. The goal is to transform the device into a compact, always-on home automation panel — a physical dashboard for lights, server monitoring, URL launching, and AI interaction.

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

All original Seeed screens are preserved. Custom screens are added to the circular navigation via horizontal swipe (LEFT = forward, RIGHT = back):

| # | Screen | Type | Description |
|---|--------|------|-------------|
| 1 | **Clock** | Original | Date & time display with NTP sync and CET/CEST timezone |
| 2 | **Sensors** | Original | Live CO2, tVOC, temperature, humidity readings |
| 3 | **Settings** | Custom | Wi-Fi + all integrations config (tabs), values saved to NVS |
| 4 | **Hue Control** | Custom | Toggle + brightness slider for 4 Philips Hue lights |
| 5 | **Sibilla Dashboard** | Custom | LAN server monitoring via Glances API + Uptime Kuma |
| 6 | **Launcher** | Custom | 4 buttons to open URLs on Mac via local Python proxy |
| 7 | **AI** | Custom | Text interface for AI interaction (voice via Grove, future) |

---

## Features

### Settings
- Unified settings screen with tabs: Wi-Fi · Hue · Server · Proxy · AI
- All values editable on-device and persisted to NVS
- Fallback to hardcoded defaults if NVS is empty

### Hue Control
- Toggle ON/OFF and adjust brightness (0–100%) for 4 configurable Philips Hue lights
- Polling every 5 seconds via local Hue Bridge (HTTPS, self-signed cert)
- Visual feedback on network error

### Sibilla Dashboard
- Real-time CPU, RAM, disk usage and load average via [Glances](https://nicolargo.github.io/glances/) REST API
- Server uptime display
- Service status (UP/DOWN) via Uptime Kuma, delivered through a local Python proxy
- Polling every 10 seconds

### Launcher
- 4 configurable buttons (2×2 grid) that open URLs on a Mac via a local Python proxy
- Fully customizable labels and endpoints

### AI (placeholder)
- Touch keyboard interface for AI text interaction
- Future: voice input/output via Grove microphone and speaker

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
