# SenseDeck — Setup Guide

---

## Requirements

- **Hardware:** SenseCAP Indicator D1S
- **Host:** macOS (the proxy script requires Mac; firmware build works on macOS and Linux)
- **ESP-IDF:** v5.3.2 — Seeed fork recommended for full compatibility
- **Python 3** — required to run the SenseDeck Proxy

---

## 1. Clone & Build

```bash
# Install ESP-IDF (Seeed fork or upstream v5.3.2)
git clone --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf
cd ~/esp/esp-idf && git checkout v5.3.2
./install.sh esp32s3
source ~/esp/esp-idf/export.sh

# Clone the repo
git clone https://github.com/cerocca/sensecap_indicator_customdeck.git
cd sensecap_indicator_customdeck

# Build and flash
source ~/esp/esp-idf/export.sh   # required in every new terminal
cd firmware
idf.py build
idf.py -p /dev/cu.usbserial-1110 flash monitor
```

> Find your serial port:
> ```bash
> # macOS
> ls /dev/cu.usb*
> # Linux
> ls /dev/ttyUSB* /dev/ttyACM*
> ```

> If you add new `.c` files, run `idf.py reconfigure` before `idf.py build`.

---

## 2. SenseDeck Proxy (Mac)

The SenseDeck Proxy is a lightweight Python script that runs on your Mac and acts as a bridge between the firmware and services that cannot be reached directly from the device (Uptime Kuma, Beszel, Google Maps, URL launching). It also hosts the configuration Web UI.

```bash
python3 sensedeck_proxy.py
```

Two convenience scripts are also available in the root of the repo for macOS:

- **`SenseDeck_Proxy_Start.command`** — start the proxy (double-click from Finder)
- **`SenseDeck_Proxy_Stop.command`** — stop the proxy (double-click from Finder)

The proxy listens on port **8765**. Web UI available at:
```
http://localhost:8765/config/ui
```

---

## 3. Configure via Web UI

Open `http://localhost:8765/config/ui` in your browser to configure all integrations. Click **Save** — the config is written to `config.json`. The device will fetch it automatically on the next Wi-Fi connection (or immediately via Settings → Proxy → "Reload config").

**Hue**
- Hue Bridge IP and API key
- Name for each of the 4 configured lights
- Light UUIDs are stored as hidden fields (set them via API or leave if already saved)

To get your Hue API key:
1. Go to `https://<HUE_BRIDGE_IP>/debug/clip.html`
2. POST to `/api` with body `{"devicetype":"sensecap_indicator"}`
3. Press the physical button on the Bridge within 30 seconds
4. Copy the token from the response

To list light UUIDs:
```
GET https://<HUE_BRIDGE_IP>/clip/v2/resource/light
Header: hue-application-key: <API_KEY>
```

**LocalServer**
- Server IP, Glances port, Beszel port / user / password, server display name

**Launcher**
- 4 URLs + display names for the Launcher screen buttons

**Weather**
- OpenWeatherMap API key, latitude, longitude, units (`metric` / `imperial`), location display name

**Traffic**
- Google Maps API key, origin address, destination address

---

## 4. On-device Configuration

Swipe **UP** from the Clock screen to open the Custom Settings screen. Tabs:

| Tab | Content |
|-----|---------|
| **Hue** | Hue Bridge IP, API key, light names — saved to NVS |
| **Server** | Server IP, server name, Glances port, Beszel port, Uptime Kuma port — saved to NVS |
| **Proxy** | Proxy Mac IP and port — saved to NVS · "Ricarica config" button fetches full config from proxy without reboot |
| **Weather** | Info + ON/OFF switch for the Weather screen |
| **Screens** | Individual ON/OFF switches for: Default sensor screen, Hue, LocalServer, Launcher |

**First-time setup flow:**
1. Start `sensedeck_proxy.py` on your Mac
2. Open `http://localhost:8765/config/ui` and fill in all fields, then Save
3. Power on the device and connect it to Wi-Fi (swipe DOWN from Clock → Wi-Fi settings)
4. The device fetches the config from the proxy automatically after connecting
5. Alternatively: swipe UP → Custom Settings → Proxy → enter proxy IP:port → tap "Ricarica config"

---

## 5. Tech Stack

| Component | Details |
|-----------|---------|
| ESP-IDF | v5.3.2 (Seeed fork) |
| LVGL | v8.x |
| FreeRTOS | included in ESP-IDF |
| cJSON | included in ESP-IDF |
| esp-http-client | included in ESP-IDF |
| mbedTLS | dynamic buffer enabled via sdkconfig.defaults |
| OpenWeatherMap API | current weather + forecast |
| Philips Hue Bridge Local API v2 | HTTPS, self-signed cert |
| Glances REST API | HTTP, LAN |
| Uptime Kuma | via SenseDeck Proxy |
| Beszel | via SenseDeck Proxy |
| Google Maps Distance Matrix API | via SenseDeck Proxy |

---

## 6. External Services

| Service | Where it runs | Requires Proxy |
|---------|--------------|----------------|
| Hue Bridge | LAN | No — firmware connects directly |
| Glances | LAN | No — firmware connects directly |
| OpenWeatherMap | Internet | No — firmware connects directly |
| Uptime Kuma | LAN | Yes |
| Beszel | LAN | Yes |
| Google Maps | Internet | Yes |

---

## Troubleshooting

**Device in boot loop after flash:**
```bash
cd firmware && rm sdkconfig && idf.py build && idf.py -p /dev/cu.usbserial-1110 flash
```

**Flash incomplete ("invalid segment length 0xffffffff"):**
Never interrupt `idf.py flash`. Re-flash completely.

**New `.c` files not picked up by the build:**
```bash
idf.py reconfigure && idf.py build
```
