#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
sensedeck_proxy.py — Server proxy per SenseCAP Indicator Deck
Gira sul Mac, ascolta sulla porta 8765

Endpoints:
  GET  /uptime      → stato monitor da Uptime Kuma (JSON compatto)
  GET  /open/<n>    → apre URL n in Firefox (n=1..4)
  GET  /ping        → health check
  GET  /config      → configurazione device (JSON); legge config.json, fallback a defaults
  POST /config      → salva configurazione (JSON) in config.json
  GET  /config/ui   → Web UI per configurazione (HTML dark theme)
"""

import http.server
import json
import os
import subprocess
import urllib.request
import urllib.error
import urllib.parse

# ── Percorso config.json (stesso dir dello script) ─────────────────────────────

CONFIG_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "config.json")

# ── Default config (fallback se config.json non esiste) ────────────────────────

DEFAULT_CONFIG = {
    "hue_bridge_ip":  "192.168.1.100",
    "hue_api_key":    "",
    "hue_light_1":    "Light 1",
    "hue_light_2":    "Light 2",
    "hue_light_3":    "Light 3",
    "hue_light_4":    "Light 4",
    "hue_light_1_id": "",
    "hue_light_2_id": "",
    "hue_light_3_id": "",
    "hue_light_4_id": "",
    "server_ip":      "192.168.1.69",
    "server_port":    "61208",
    "srv_name":       "LocalServer",
    "proxy_ip":       "192.168.1.70",
    "proxy_port":     "8765",
    "launcher_url_1": "https://github.com/cerocca/",
    "launcher_url_2": "https://www.strava.com",
    "launcher_url_3": "https://connect.garmin.com/",
    "launcher_url_4": "https://intervals.icu/",
    "lnch_name_1":    "GitHub",
    "lnch_name_2":    "Strava",
    "lnch_name_3":    "Garmin",
    "lnch_name_4":    "Intervals",
    "beszel_port":    "8090",
    "beszel_user":    "",
    "beszel_password": "",
    "uk_port":        "3001",
    "owm_api_key":          "",
    "owm_lat":              "",
    "owm_lon":              "",
    "owm_units":            "metric",
    "owm_city_name":        "",
    "owm_location":         "",   # es. "Firenze, IT" — sovrascrive lat/lon display
    "gmaps_api_key":        "",
    "traffic_origin":       "",
    "traffic_destination":  "",
    "traffic_mode":         "driving",
}

LISTEN_PORT     = 8765

# ── Beszel token cache ──────────────────────────────────────────────────────────

_beszel_token = None

# ── Config helpers ──────────────────────────────────────────────────────────────

def load_config():
    """Carica config.json; se non esiste o è corrotto, ritorna i defaults."""
    try:
        with open(CONFIG_PATH, "r") as f:
            data = json.load(f)
        # Merge con defaults per campi mancanti
        merged = dict(DEFAULT_CONFIG)
        merged.update(data)
        return merged
    except (FileNotFoundError, json.JSONDecodeError):
        return dict(DEFAULT_CONFIG)


def save_config(data):
    """Salva data in config.json. Ritorna True su successo."""
    try:
        with open(CONFIG_PATH, "w") as f:
            json.dump(data, f, indent=2)
        return True
    except Exception as e:
        print(f"[config] save error: {e}")
        return False


def get_beszel_token(cfg):
    """Autentica su Beszel e cachea il token. Ritorna None se le credenziali mancano."""
    global _beszel_token
    if _beszel_token:
        return _beszel_token
    user     = cfg.get("beszel_user", "")
    password = cfg.get("beszel_password", "")
    host     = cfg.get("server_ip",    DEFAULT_CONFIG["server_ip"])
    port     = cfg.get("beszel_port",  DEFAULT_CONFIG["beszel_port"])
    if not user or not password:
        return None
    url  = f"http://{host}:{port}/api/collections/users/auth-with-password"
    body = json.dumps({"identity": user, "password": password}).encode()
    req  = urllib.request.Request(url, data=body,
                                  headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            _beszel_token = json.loads(resp.read()).get("token")
            return _beszel_token
    except Exception as e:
        print(f"[beszel] auth error: {e}")
        return None


def get_beszel_docker():
    """
    Chiama Beszel container_stats, ritorna lista container ordinata per RAM desc.
    Formato: [{"name": "uptime-kuma", "mem_mb": 203.73}, ...]
    """
    global _beszel_token
    cfg  = load_config()
    host = cfg.get("server_ip",   DEFAULT_CONFIG["server_ip"])
    port = cfg.get("beszel_port", DEFAULT_CONFIG["beszel_port"])

    def fetch(token):
        url = (f"http://{host}:{port}/api/collections/container_stats/records"
               f"?sort=-created&perPage=1")
        req = urllib.request.Request(url, headers={"Authorization": f"Bearer {token}"})
        with urllib.request.urlopen(req, timeout=5) as resp:
            data  = json.loads(resp.read())
            items = data.get("items", [])
            if not items:
                return []
            stats = items[0].get("stats", [])
            sorted_stats = sorted(stats, key=lambda x: x.get("m", 0), reverse=True)
            return [{"name": c["n"], "mem_mb": round(c["m"], 1)}
                    for c in sorted_stats if "n" in c and "m" in c]

    token = get_beszel_token(cfg)
    if not token:
        return []
    try:
        return fetch(token)
    except urllib.error.HTTPError as e:
        if e.code == 401:
            # Token scaduto — ri-autentica una volta
            _beszel_token = None
            token = get_beszel_token(cfg)
            if not token:
                return []
            try:
                return fetch(token)
            except Exception as e2:
                print(f"[beszel] docker retry error: {e2}")
                return []
        print(f"[beszel] docker error: {e}")
        return []
    except Exception as e:
        print(f"[beszel] docker error: {e}")
        return []


def get_traffic_data():
    """
    Chiama Google Maps Distance Matrix API e ritorna un dict compatto.
    Risposta: {"duration_sec": N, "duration_normal_sec": N, "delta_sec": N,
               "distance_m": N, "status": "ok"|"slow"|"bad"}
    o {"error": "not_configured"} / {"error": "api_error"}
    """
    cfg  = load_config()
    key  = cfg.get("gmaps_api_key", "").strip()
    orig = cfg.get("traffic_origin", "").strip()
    dest = cfg.get("traffic_destination", "").strip()
    mode = cfg.get("traffic_mode", "driving").strip() or "driving"

    if not key or not orig or not dest:
        return {"error": "not_configured"}

    params = urllib.parse.urlencode({
        "origins":        orig,
        "destinations":   dest,
        "mode":           mode,
        "departure_time": "now",
        "traffic_model":  "best_guess",
        "key":            key,
    })
    url = f"https://maps.googleapis.com/maps/api/distancematrix/json?{params}"

    try:
        req = urllib.request.Request(url)
        with urllib.request.urlopen(req, timeout=8) as resp:
            data = json.loads(resp.read())
    except Exception as e:
        print(f"[traffic] API error: {e}")
        return {"error": "api_error"}

    try:
        row     = data["rows"][0]["elements"][0]
        status  = row.get("status", "")
        if status != "OK":
            print(f"[traffic] element status: {status}")
            return {"error": "api_error"}

        dur_sec     = row["duration_in_traffic"]["value"]
        dur_nor_sec = row["duration"]["value"]
        dist_m      = row["distance"]["value"]
        delta_sec   = dur_sec - dur_nor_sec

        if delta_sec <= 120:
            traffic_status = "ok"
        elif delta_sec <= 600:
            traffic_status = "slow"
        else:
            traffic_status = "bad"

        return {
            "duration_sec":        dur_sec,
            "duration_normal_sec": dur_nor_sec,
            "delta_sec":           delta_sec,
            "distance_m":          dist_m,
            "status":              traffic_status,
        }
    except (KeyError, IndexError, TypeError) as e:
        print(f"[traffic] parse error: {e}")
        return {"error": "api_error"}


def launcher_urls():
    cfg = load_config()
    return {
        "1": cfg.get("launcher_url_1", DEFAULT_CONFIG["launcher_url_1"]),
        "2": cfg.get("launcher_url_2", DEFAULT_CONFIG["launcher_url_2"]),
        "3": cfg.get("launcher_url_3", DEFAULT_CONFIG["launcher_url_3"]),
        "4": cfg.get("launcher_url_4", DEFAULT_CONFIG["launcher_url_4"]),
    }

# ── Web UI HTML ─────────────────────────────────────────────────────────────────

def build_config_ui(cfg):
    def field(label, name, value, placeholder=""):
        return (
            f'<div class="field">'
            f'<label for="{name}">{label}</label>'
            f'<input type="text" id="{name}" name="{name}" value="{value}" placeholder="{placeholder}">'
            f'</div>'
        )

    def pwd_field(label, name, value):
        return (
            f'<div class="field">'
            f'<label for="{name}">{label}</label>'
            f'<input type="password" id="{name}" name="{name}" value="{value}">'
            f'</div>'
        )

    def select_field(label, name, value, options):
        opts = "".join(
            f'<option value="{v}"{" selected" if v == value else ""}>{l}</option>'
            for v, l in options
        )
        return (
            f'<div class="field">'
            f'<label for="{name}">{label}</label>'
            f'<select id="{name}" name="{name}">{opts}</select>'
            f'</div>'
        )

    def sep():
        return '<hr class="sep">'

    # Hue light IDs: not shown in columns but preserved as hidden inputs
    hidden = "".join(
        f'<input type="hidden" name="hue_light_{i}_id" value="{cfg.get(f"hue_light_{i}_id","")}">'
        for i in range(1, 5)
    )
    # owm_city_name preserved as hidden (superseded by owm_location in UI)
    hidden += f'<input type="hidden" name="owm_city_name" value="{cfg.get("owm_city_name","")}">'

    col1 = (
        "<h2>Hue Bridge</h2>"
        + field("IP Bridge",  "hue_bridge_ip", cfg.get("hue_bridge_ip",""), "192.168.1.x")
        + field("API Key",    "hue_api_key",   cfg.get("hue_api_key",""))
        + field("Light 1",    "hue_light_1",   cfg.get("hue_light_1",""),   "Light 1")
        + field("Light 2",    "hue_light_2",   cfg.get("hue_light_2",""),   "Light 2")
        + field("Light 3",    "hue_light_3",   cfg.get("hue_light_3",""),   "Light 3")
        + field("Light 4",    "hue_light_4",   cfg.get("hue_light_4",""),   "Light 4")
    )

    col2 = (
        "<h2>LocalServer</h2>"
        + field("Server Name",      "srv_name",    cfg.get("srv_name",""),    "LocalServer")
        + field("Server IP",        "server_ip",   cfg.get("server_ip",""),   "192.168.1.x")
        + field("Glances Port",     "server_port", cfg.get("server_port",""), "61208")
        + field("Uptime Kuma Port", "uk_port",     cfg.get("uk_port",""),     "3001")
        + field("Beszel Port",      "beszel_port", cfg.get("beszel_port",""), "8090")
        + field("Beszel User",      "beszel_user", cfg.get("beszel_user",""))
        + pwd_field("Beszel Password", "beszel_password", cfg.get("beszel_password",""))
        + sep()
        + "<h2>Proxy Mac</h2>"
        + field("Proxy IP",   "proxy_ip",   cfg.get("proxy_ip",""),   "192.168.1.x")
        + field("Proxy Port", "proxy_port", cfg.get("proxy_port",""), "8765")
    )

    col3 = (
        "<h2>Launcher</h2>"
        + field("Nome 1", "lnch_name_1",   cfg.get("lnch_name_1",""),   "GitHub")
        + field("URL 1",  "launcher_url_1", cfg.get("launcher_url_1",""))
        + field("Nome 2", "lnch_name_2",   cfg.get("lnch_name_2",""),   "Strava")
        + field("URL 2",  "launcher_url_2", cfg.get("launcher_url_2",""))
        + field("Nome 3", "lnch_name_3",   cfg.get("lnch_name_3",""),   "Garmin")
        + field("URL 3",  "launcher_url_3", cfg.get("launcher_url_3",""))
        + field("Nome 4", "lnch_name_4",   cfg.get("lnch_name_4",""),   "Intervals")
        + field("URL 4",  "launcher_url_4", cfg.get("launcher_url_4",""))
        + sep()
        + "<h2>Weather (OpenWeatherMap)</h2>"
        + field("OWM API Key", "owm_api_key",  cfg.get("owm_api_key",""),  "32 caratteri")
        + field("Location",    "owm_location", cfg.get("owm_location",""), "es. Firenze, IT")
        + field("Latitude",    "owm_lat",      cfg.get("owm_lat",""),      "es. 43.7711")
        + field("Longitude",   "owm_lon",      cfg.get("owm_lon",""),      "es. 11.2486")
        + select_field("Units", "owm_units", cfg.get("owm_units","metric"),
                       [("metric","metric (°C, km/h)"), ("imperial","imperial (°F, mph)")])
        + sep()
        + "<h2>Traffic (Google Maps)</h2>"
        + field("Google Maps API Key", "gmaps_api_key",       cfg.get("gmaps_api_key",""))
        + field("Origin",              "traffic_origin",      cfg.get("traffic_origin",""),      "es. Via Roma 1, Firenze")
        + field("Destination",         "traffic_destination", cfg.get("traffic_destination",""), "es. Piazza Duomo, Firenze")
        + select_field("Mode", "traffic_mode", cfg.get("traffic_mode","driving"),
                       [("driving","driving"), ("walking","walking"),
                        ("bicycling","bicycling"), ("transit","transit")])
    )

    return f"""<!DOCTYPE html>
<html lang="it">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>SenseDeck Config</title>
  <style>
    *, *::before, *::after {{ box-sizing: border-box; margin: 0; padding: 0; }}
    body {{
      font-family: 'Segoe UI', system-ui, sans-serif;
      background: #0f1117;
      color: #e0e0e0;
      padding: 32px 16px;
    }}
    h1 {{ font-size: 1.4rem; color: #7ec8a0; margin-bottom: 4px; }}
    .subtitle {{ font-size: 0.8rem; color: #666; margin-bottom: 28px; }}
    h2 {{
      font-size: 0.85rem;
      text-transform: uppercase;
      letter-spacing: 0.08em;
      color: #7ec8a0;
      margin: 20px 0 12px;
      padding-bottom: 6px;
      border-bottom: 1px solid #2a2a3a;
    }}
    h2:first-child {{ margin-top: 0; }}
    .cols {{ display: flex; gap: 24px; align-items: flex-start; }}
    .col {{ flex: 1; min-width: 0; }}
    .field {{ margin-bottom: 14px; }}
    label {{
      display: block;
      font-size: 0.78rem;
      color: #999;
      margin-bottom: 4px;
    }}
    input[type="text"], input[type="password"], select {{
      width: 100%;
      background: #1e1e2e;
      border: 1px solid #333;
      border-radius: 6px;
      color: #e0e0e0;
      padding: 8px 10px;
      font-size: 0.9rem;
      outline: none;
      transition: border-color .15s;
    }}
    input[type="text"]:focus, input[type="password"]:focus, select:focus {{ border-color: #7ec8a0; }}
    select option {{ background: #1e1e2e; }}
    .sep {{ border: none; border-top: 1px solid #2a2a3a; margin: 20px 0; }}
    .actions {{ margin-top: 32px; display: flex; justify-content: center; }}
    button {{
      padding: 10px 32px;
      border: none;
      border-radius: 6px;
      font-size: 0.9rem;
      cursor: pointer;
      font-weight: 600;
      transition: opacity .15s;
    }}
    button:hover {{ opacity: 0.85; }}
    #btn-save {{ background: #529d53; color: #fff; }}
    #status-msg {{
      margin-top: 16px;
      font-size: 0.85rem;
      min-height: 1.2em;
      color: #7ec8a0;
      text-align: center;
    }}
    @media (max-width: 900px) {{ .cols {{ flex-direction: column; }} }}
  </style>
</head>
<body>
  <h1>SenseDeck Config</h1>
  <div class="subtitle">Configurazione dispositivo — sensecap_indicator_customdeck/config.json</div>

  <form id="cfg-form">
    {hidden}
    <div class="cols">
      <div class="col">{col1}</div>
      <div class="col">{col2}</div>
      <div class="col">{col3}</div>
    </div>
    <div class="actions">
      <button type="button" id="btn-save">Salva tutto</button>
    </div>
    <div id="status-msg"></div>
  </form>

  <script>
    document.getElementById('btn-save').addEventListener('click', async () => {{
      const form = document.getElementById('cfg-form');
      const data = {{}};
      form.querySelectorAll('input, select').forEach(el => {{
        if (el.name) data[el.name] = el.value;
      }});
      const msg = document.getElementById('status-msg');
      msg.style.color = '#7ec8a0';
      msg.textContent = 'Salvataggio...';
      try {{
        const res = await fetch('/config', {{
          method: 'POST',
          headers: {{'Content-Type': 'application/json'}},
          body: JSON.stringify(data)
        }});
        if (res.ok) {{
          msg.textContent = '\u2713 Salvato';
        }} else {{
          msg.style.color = '#e07070';
          msg.textContent = '\u2717 Errore ' + res.status;
        }}
      }} catch(e) {{
        msg.style.color = '#e07070';
        msg.textContent = '\u2717 ' + e.message;
      }}
    }});
  </script>
</body>
</html>"""

# ── Handler ────────────────────────────────────────────────────────────────────

class ProxyHandler(http.server.BaseHTTPRequestHandler):

    def log_message(self, format, *args):
        print(f"[{self.address_string()}] {format % args}")

    def send_json(self, data, status=200):
        body = json.dumps(data, indent=2).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", len(body))
        self.end_headers()
        self.wfile.write(body)

    def send_text(self, text, status=200):
        body = text.encode()
        self.send_response(status)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", len(body))
        self.end_headers()
        self.wfile.write(body)

    def send_html(self, html, status=200):
        body = html.encode()
        self.send_response(status)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", len(body))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        path = self.path.split("?")[0]

        # ── /uptime ────────────────────────────────────────────────────────────
        if path == "/uptime":
            try:
                _cfg = load_config()
                _uk_host = _cfg.get("server_ip", DEFAULT_CONFIG["server_ip"])
                _uk_port = _cfg.get("uk_port",   DEFAULT_CONFIG["uk_port"])
                _uk_url  = f"http://{_uk_host}:{_uk_port}/api/status-page/heartbeat/active"
                req = urllib.request.Request(_uk_url)
                with urllib.request.urlopen(req, timeout=5) as resp:
                    raw = json.loads(resp.read())

                monitor_list = raw.get("monitorList", {})
                result = []
                for monitor_id, heartbeats in raw.get("heartbeatList", {}).items():
                    if not heartbeats:
                        continue
                    last = heartbeats[-1]
                    info = monitor_list.get(str(monitor_id), {})
                    name = info.get("name", f"Monitor {monitor_id}")
                    # Escludi intestazioni gruppo (es. "0-Infra")
                    if name.startswith("0-"):
                        continue
                    result.append({
                        "name": name,
                        "up":   last["status"] == 1,
                    })

                self.send_json(result)

            except Exception as e:
                print(f"[uptime] error: {e}")
                self.send_json({"error": str(e)}, status=502)

        # ── /open/<n> ──────────────────────────────────────────────────────────
        elif path.startswith("/open/"):
            key = path.split("/open/")[-1].strip("/")
            urls = launcher_urls()
            url = urls.get(key)
            if url:
                subprocess.Popen(["open", "-a", "Firefox", url])
                print(f"[launcher] opened {url}")
                self.send_text(f"OK: {url}")
            else:
                self.send_text(f"Unknown key: {key}", status=404)

        # ── /traffic ───────────────────────────────────────────────────────────
        elif path == "/traffic":
            self.send_json(get_traffic_data())

        # ── /docker ────────────────────────────────────────────────────────────
        elif path == "/docker":
            try:
                self.send_json(get_beszel_docker())
            except Exception as e:
                print(f"[docker] error: {e}")
                self.send_json([])

        # ── /ping ──────────────────────────────────────────────────────────────
        elif path == "/ping":
            self.send_text("pong")

        # ── /config/ui ─────────────────────────────────────────────────────────
        elif path == "/config/ui":
            cfg = load_config()
            self.send_html(build_config_ui(cfg))

        # ── /config ────────────────────────────────────────────────────────────
        elif path == "/config":
            self.send_json(load_config())

        else:
            self.send_text("Not found", status=404)

    def do_POST(self):
        path = self.path.split("?")[0]

        # ── POST /config ───────────────────────────────────────────────────────
        if path == "/config":
            try:
                length = int(self.headers.get("Content-Length", 0))
                body = self.rfile.read(length)
                data = json.loads(body)
            except Exception as e:
                self.send_json({"error": f"Invalid JSON: {e}"}, status=400)
                return

            # Merge con defaults per campi mancanti
            merged = dict(DEFAULT_CONFIG)
            merged.update(data)

            if save_config(merged):
                print(f"[config] saved to {CONFIG_PATH}")
                self.send_json({"ok": True})
            else:
                self.send_json({"error": "Save failed"}, status=500)

        else:
            self.send_text("Not found", status=404)


# ── Main ───────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    cfg = load_config()
    server = http.server.HTTPServer(("0.0.0.0", LISTEN_PORT), ProxyHandler)
    print(f"sensedeck_proxy listening on port {LISTEN_PORT}")
    print(f"  /uptime     → Uptime Kuma {cfg.get('server_ip','?')}:{cfg.get('uk_port','?')}")
    print(f"  /traffic    → Google Maps Distance Matrix API")
    for k in ["1", "2", "3", "4"]:
        print(f"  /open/{k}     → {cfg.get(f'launcher_url_{k}', '?')}")
    print(f"  /config     → GET config / POST save")
    print(f"  /config/ui  → Web UI ({CONFIG_PATH})")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStop.")
