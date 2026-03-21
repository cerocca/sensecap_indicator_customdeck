# SenseCAP Indicator Deck — Setup Guide

Guida per configurare e compilare il firmware da zero.

---

## Prerequisiti

- **Hardware:** SenseCAP Indicator D1S
- **Toolchain:** ESP-IDF v5.3.2
- **OS:** macOS (testato) o Linux

### Installazione ESP-IDF

```bash
git clone --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf
cd ~/esp/esp-idf && git checkout v5.3.2
./install.sh esp32s3
source ~/esp/esp-idf/export.sh
```

---

## Clone del repo

```bash
git clone https://github.com/cerocca/sensecap_indicator_customdeck.git
cd sensecap_indicator_customdeck
```

---

## Configurazione

Apri `firmware/main/app_config.h` e imposta i tuoi valori:

### Philips Hue
```c
#define HUE_BRIDGE_IP    "192.168.x.x"      // IP del tuo Hue Bridge
#define HUE_API_KEY      "la-tua-api-key"   // API key Hue Bridge

#define HUE_LIGHT_1_ID   "uuid-luce-1"
#define HUE_LIGHT_1_NAME "Nome Luce 1"
// ripeti per le luci 2, 3, 4
```

**Come ottenere l'API key Hue:**
1. Vai su `https://<HUE_BRIDGE_IP>/debug/clip.html`
2. POST su `/api` con body `{"devicetype":"sensecap_indicator"}`
3. Premi il pulsante fisico sul Bridge entro 30 secondi
4. Copia il token dalla risposta

**Come ottenere gli ID delle luci:**
- GET su `https://<HUE_BRIDGE_IP>/clip/v2/resource/light` con header `hue-application-key: <API_KEY>`

### LocalServer (Glances + Uptime Kuma)
```c
#define LOCALSERVER_IP   "192.168.x.x"  // IP del server con Glances
#define GLANCES_PORT     61208
```

### Proxy Mac
```c
#define PROXY_IP    "192.168.x.x"  // IP del Mac con il proxy Python
#define PROXY_PORT  8765
```

### Launcher — label pulsanti
```c
#define LAUNCHER_1_LABEL  "Website1"
#define LAUNCHER_2_LABEL  "Website2"
#define LAUNCHER_3_LABEL  "Website3"
#define LAUNCHER_4_LABEL  "Website4"
```
Configura gli URL corrispondenti nel proxy Python.

---

## Build e Flash

```bash
source ~/esp/esp-idf/export.sh
cd firmware
idf.py build
idf.py -p /dev/cu.usbserial-1110 flash monitor
```

> Porta seriale usata su questo setup: `/dev/cu.usbserial-1110`

> Sostituisci `/dev/cu.usbserial-1110` con la porta seriale del tuo dispositivo.  
> Su Linux: tipicamente `/dev/ttyUSB0` o `/dev/ttyACM0`.

Per trovare la porta:
```bash
# macOS
ls /dev/cu.usb*
# Linux
ls /dev/ttyUSB* /dev/ttyACM*
```

---

## Configurazione runtime (opzionale)

Tutti i valori di `app_config.h` possono essere modificati direttamente sul device dalla schermata **Settings** (swipe fino alla terza schermata). I valori vengono salvati in NVS e persistono al reboot.

---

## Proxy Python (Mac)

Il proxy è necessario per la schermata LocalServer Dashboard (Uptime Kuma) e la schermata Launcher.

Avvia il proxy sul Mac:
```bash
python3 proxy.py
```

Il proxy espone:
- `GET /uptime` → stato servizi Uptime Kuma (JSON compatto)
- `GET /open/<n>` → apre URL n-esimo nel browser del Mac

---

## Troubleshooting

**Device in boot loop dopo flash:**
```bash
cd firmware && rm sdkconfig && idf.py build && idf.py -p /dev/cu.usbserial-1110 flash
```

**Flash incompleto ("invalid segment length 0xffffffff"):**
Non interrompere mai `idf.py flash`. Ri-flashare completamente.

**Nuovi file `.c` non rilevati dalla build:**
```bash
idf.py reconfigure && idf.py build
```
