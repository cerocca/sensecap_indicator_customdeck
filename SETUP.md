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

La configurazione avviene principalmente tramite la **Web UI del proxy** (`http://localhost:8765/config/ui`) — non è necessario modificare `app_config.h` per l'uso normale.

`app_config.h` contiene i valori di fallback usati solo se NVS è vuoto e il proxy non è raggiungibile.

### Primo avvio: flusso consigliato

1. Avvia `sensedeck_proxy.py` sul Mac
2. Apri `http://localhost:8765/config/ui` e inserisci tutti i valori
3. Clicca **Salva** — la config viene scritta in `config.json`
4. Accendi il device e connettilo al Wi-Fi (Settings → tab Wi-Fi)
5. Il device scarica automaticamente la config dal proxy al boot

### Come ottenere l'API key Hue

1. Vai su `https://<HUE_BRIDGE_IP>/debug/clip.html`
2. POST su `/api` con body `{"devicetype":"sensecap_indicator"}`
3. Premi il pulsante fisico sul Bridge entro 30 secondi
4. Copia il token dalla risposta

### Come ottenere gli ID UUID delle luci Hue

```
GET https://<HUE_BRIDGE_IP>/clip/v2/resource/light
Header: hue-application-key: <API_KEY>
```

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

## Configurazione runtime

I valori possono essere aggiornati in tre modi, senza ricompilare:

1. **Web UI proxy** (`http://localhost:8765/config/ui`) → Salva → il device li scarica al prossimo boot o con "Ricarica config"
2. **Direttamente sul device** — schermata Settings (terza schermata, swipe). I valori vengono salvati in NVS e persistono al reboot.
3. **"Ricarica config"** — Settings → tab Proxy → pulsante per aggiornare da proxy senza riavviare.

---

## Proxy Python (Mac)

Il proxy è necessario per la schermata LocalServer Dashboard (Uptime Kuma) e la schermata Launcher.
Gestisce anche la configurazione centralizzata del device tramite Web UI.

### Avvio

```bash
python3 sensedeck_proxy.py
```

### Configurazione via Web UI

Apri nel browser: `http://localhost:8765/config/ui`

Dalla Web UI puoi configurare:
- **Hue Bridge:** IP, API key, nome e ID UUID di ogni luce
- **LocalServer:** IP e porta Glances
- **Proxy:** IP e porta del Mac
- **Launcher:** URL 1–4

La configurazione viene salvata in `config.json` nella stessa directory del proxy.

**Come ottenere gli ID UUID delle luci Hue:**
```
GET https://<HUE_BRIDGE_IP>/clip/v2/resource/light
Header: hue-application-key: <API_KEY>
```

### Sincronizzazione con il device

- **Al boot:** il device scarica automaticamente la config dal proxy dopo la connessione Wi-Fi
- **Senza riavvio:** Settings → tab Proxy → pulsante **"Ricarica config"**

### Endpoint disponibili

| Endpoint | Metodo | Descrizione |
|---|---|---|
| `/uptime` | GET | stato servizi Uptime Kuma (JSON compatto) |
| `/open/<n>` | GET | apre URL n nel browser del Mac (n=1..4) |
| `/ping` | GET | health check |
| `/config` | GET | restituisce configurazione corrente (JSON) |
| `/config` | POST | salva nuova configurazione in `config.json` |
| `/config/ui` | GET | Web UI configurazione (dark theme) |

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
