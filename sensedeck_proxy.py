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
    "traffic_routes": [
        {"name": "Route 1", "origin": "", "destination": "", "mode": "driving", "enabled": True},
        {"name": "Route 2", "origin": "", "destination": "", "mode": "driving", "enabled": False},
    ],
}

LISTEN_PORT     = 8765

# ── Beszel token cache ──────────────────────────────────────────────────────────

_beszel_token = None

# ── Config helpers ──────────────────────────────────────────────────────────────

def _merge_config(defaults, saved):
    """
    Merge ricorsivo config: usa 'saved' se presente, defaults come fallback.
    - dict annidati: merge ricorsivo
    - lista di dict (es. traffic_routes): per ogni elemento, merge con il
      corrispondente elemento di defaults — preserva campi salvati, aggiunge
      campi nuovi da defaults
    - scalari: usa saved se presente, altrimenti default
    Chiavi extra in saved (non in defaults) vengono preservate.
    """
    result = {}
    for key, default_val in defaults.items():
        if key not in saved:
            # Chiave mancante in saved → usa default (copia profonda per liste/dict)
            if isinstance(default_val, list):
                result[key] = [dict(e) if isinstance(e, dict) else e
                               for e in default_val]
            elif isinstance(default_val, dict):
                result[key] = dict(default_val)
            else:
                result[key] = default_val
        elif isinstance(default_val, dict) and isinstance(saved[key], dict):
            result[key] = _merge_config(default_val, saved[key])
        elif (isinstance(default_val, list) and default_val and
              isinstance(default_val[0], dict)):
            # Lista di dict: merge elemento per elemento
            saved_list = saved[key] if isinstance(saved[key], list) else []
            merged_list = []
            for i, default_item in enumerate(default_val):
                if i < len(saved_list) and isinstance(saved_list[i], dict):
                    # Saved ha priorità, ma campi mancanti vengono aggiunti dal default
                    merged_list.append({**default_item, **saved_list[i]})
                else:
                    merged_list.append(dict(default_item))
            result[key] = merged_list
        else:
            result[key] = saved[key]
    # Chiavi extra in saved non presenti in defaults (forward compat)
    for key in saved:
        if key not in result:
            result[key] = saved[key]
    return result


def load_config():
    """Carica config.json; se non esiste o è corrotto, ritorna i defaults."""
    try:
        with open(CONFIG_PATH, "r") as f:
            data = json.load(f)
        merged = _merge_config(DEFAULT_CONFIG, data)
        print(f"[config] loaded {len(data)} keys from config.json")
        print(f"[config] traffic routes: {[r['name'] for r in merged.get('traffic_routes', [])]}")
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


def _call_distance_matrix(key, origin, destination, mode):
    """
    Chiama Google Maps Distance Matrix per una singola coppia origine/destinazione.
    Ritorna un dict con i campi route oppure {"error": "..."}.
    """
    params = urllib.parse.urlencode({
        "origins":        origin,
        "destinations":   destination,
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
        row = data["rows"][0]["elements"][0]
        if row.get("status", "") != "OK":
            print(f"[traffic] element status: {row.get('status')}")
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


def get_traffic_data():
    """
    Itera sulle traffic_routes abilitate e chiama Distance Matrix per ognuna.
    Risposta: array JSON di route oppure {"error": "not_configured"}.
    """
    cfg    = load_config()
    key    = cfg.get("gmaps_api_key", "").strip()
    routes = cfg.get("traffic_routes", DEFAULT_CONFIG["traffic_routes"])

    if not key:
        return {"error": "not_configured"}

    results = []
    for route in routes:
        if not route.get("enabled", False):
            continue
        orig = route.get("origin", "").strip()
        dest = route.get("destination", "").strip()
        mode = route.get("mode", "driving").strip() or "driving"
        name = route.get("name", "Route")
        if not orig or not dest:
            continue
        r = _call_distance_matrix(key, orig, dest, mode)
        if "error" in r:
            print(f"[traffic] route '{name}' error: {r['error']}")
            continue
        r["name"] = name
        results.append(r)

    if not results:
        return {"error": "not_configured"}

    return results


def launcher_urls():
    cfg = load_config()
    return {
        "1": cfg.get("launcher_url_1", DEFAULT_CONFIG["launcher_url_1"]),
        "2": cfg.get("launcher_url_2", DEFAULT_CONFIG["launcher_url_2"]),
        "3": cfg.get("launcher_url_3", DEFAULT_CONFIG["launcher_url_3"]),
        "4": cfg.get("launcher_url_4", DEFAULT_CONFIG["launcher_url_4"]),
    }

# ── Web UI HTML ─────────────────────────────────────────────────────────────────

def _build_route_fields(routes):
    """Genera HTML per i campi di 2 route traffic."""
    modes = [("driving","driving"), ("walking","walking"),
             ("bicycling","bicycling"), ("transit","transit")]
    out = ""
    for i in range(2):
        route = routes[i] if i < len(routes) else {}
        checked = " checked" if route.get("enabled", False) else ""
        out += f'<h3 class="route-hdr">Route {i+1}</h3>'
        # Enabled subito sotto il titolo
        out += (
            f'<div class="field field-check">'
            f'<label><input type="checkbox" id="tr_{i}_enabled"'
            f' name="tr_{i}_enabled" value="true"{checked}>'
            f' Enabled</label>'
            f'</div>'
        )
        out += (
            f'<div class="field">'
            f'<label for="tr_{i}_name">Name</label>'
            f'<input type="text" id="tr_{i}_name" name="tr_{i}_name"'
            f' value="{route.get("name", f"Route {i+1}")}">'
            f'</div>'
        )
        out += (
            f'<div class="field">'
            f'<label for="tr_{i}_origin">Origin</label>'
            f'<input type="text" id="tr_{i}_origin" name="tr_{i}_origin"'
            f' value="{route.get("origin","")}"'
            f' placeholder="es. Via Roma 1, Firenze">'
            f'</div>'
        )
        out += (
            f'<div class="field">'
            f'<label for="tr_{i}_destination">Destination</label>'
            f'<input type="text" id="tr_{i}_destination" name="tr_{i}_destination"'
            f' value="{route.get("destination","")}"'
            f' placeholder="es. Piazza Duomo, Firenze">'
            f'</div>'
        )
        opts = "".join(
            f'<option value="{v}"{" selected" if v == route.get("mode","driving") else ""}>{l}</option>'
            for v, l in modes
        )
        out += (
            f'<div class="field">'
            f'<label for="tr_{i}_mode">Mode</label>'
            f'<select id="tr_{i}_mode" name="tr_{i}_mode">{opts}</select>'
            f'</div>'
        )
    return out


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

    # Hue light IDs: not shown in tabs but preserved as hidden inputs
    hidden = "".join(
        f'<input type="hidden" name="hue_light_{i}_id" value="{cfg.get(f"hue_light_{i}_id","")}">'
        for i in range(1, 5)
    )
    # owm_city_name preserved as hidden (superseded by owm_location in UI)
    hidden += f'<input type="hidden" name="owm_city_name" value="{cfg.get("owm_city_name","")}">'

    tab_hue = (
        field("IP Bridge", "hue_bridge_ip", cfg.get("hue_bridge_ip",""), "192.168.1.x")
        + field("API Key",  "hue_api_key",  cfg.get("hue_api_key",""))
        + field("Light 1",  "hue_light_1",  cfg.get("hue_light_1",""), "Light 1")
        + field("Light 2",  "hue_light_2",  cfg.get("hue_light_2",""), "Light 2")
        + field("Light 3",  "hue_light_3",  cfg.get("hue_light_3",""), "Light 3")
        + field("Light 4",  "hue_light_4",  cfg.get("hue_light_4",""), "Light 4")
    )

    tab_localserver = (
        field("Server Name",      "srv_name",    cfg.get("srv_name",""),    "LocalServer")
        + field("Server IP",      "server_ip",   cfg.get("server_ip",""),   "192.168.1.x")
        + field("Glances Port",   "server_port", cfg.get("server_port",""), "61208")
        + field("Uptime Kuma Port","uk_port",    cfg.get("uk_port",""),     "3001")
        + field("Beszel Port",    "beszel_port", cfg.get("beszel_port",""), "8090")
        + field("Beszel User",    "beszel_user", cfg.get("beszel_user",""))
        + pwd_field("Beszel Password", "beszel_password", cfg.get("beszel_password",""))
    )

    tab_proxy = (
        field("Proxy IP",   "proxy_ip",   cfg.get("proxy_ip",""),   "192.168.1.x")
        + field("Proxy Port", "proxy_port", cfg.get("proxy_port",""), "8765")
    )

    tab_launcher = (
        field("Nome 1", "lnch_name_1",    cfg.get("lnch_name_1",""),    "GitHub")
        + field("URL 1", "launcher_url_1", cfg.get("launcher_url_1",""))
        + field("Nome 2", "lnch_name_2",   cfg.get("lnch_name_2",""),   "Strava")
        + field("URL 2", "launcher_url_2", cfg.get("launcher_url_2",""))
        + field("Nome 3", "lnch_name_3",   cfg.get("lnch_name_3",""),   "Garmin")
        + field("URL 3", "launcher_url_3", cfg.get("launcher_url_3",""))
        + field("Nome 4", "lnch_name_4",   cfg.get("lnch_name_4",""),   "Intervals")
        + field("URL 4", "launcher_url_4", cfg.get("launcher_url_4",""))
    )

    tab_weather = (
        field("OWM API Key", "owm_api_key",  cfg.get("owm_api_key",""),  "32 caratteri")
        + field("Location",  "owm_location", cfg.get("owm_location",""), "es. Firenze, IT")
        + field("Latitude",  "owm_lat",      cfg.get("owm_lat",""),      "es. 43.7711")
        + field("Longitude", "owm_lon",      cfg.get("owm_lon",""),      "es. 11.2486")
        + select_field("Units", "owm_units", cfg.get("owm_units","metric"),
                       [("metric","metric (°C, km/h)"), ("imperial","imperial (°F, mph)")])
    )

    tab_traffic = (
        field("Google Maps API Key", "gmaps_api_key", cfg.get("gmaps_api_key",""))
        + _build_route_fields(cfg.get("traffic_routes", DEFAULT_CONFIG["traffic_routes"]))
    )

    tabs = [
        ("hue",         "Hue",         tab_hue),
        ("localserver", "LocalServer", tab_localserver),
        ("proxy",       "Proxy",       tab_proxy),
        ("launcher",    "Launcher",    tab_launcher),
        ("weather",     "Weather",     tab_weather),
        ("traffic",     "Traffic",     tab_traffic),
    ]

    tab_buttons = "".join(
        f'<button type="button" class="tab-btn{" active" if i == 0 else ""}" data-tab="{tid}">{tlabel}</button>'
        for i, (tid, tlabel, _) in enumerate(tabs)
    )
    tab_panels = "".join(
        f'<div class="tab-panel{" active" if i == 0 else ""}" id="tab-{tid}">{tcontent}</div>'
        for i, (tid, _, tcontent) in enumerate(tabs)
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
      padding: 24px 16px;
    }}
    h1 {{ font-size: 1.4rem; color: #7ec8a0; margin-bottom: 4px; }}
    .subtitle {{ font-size: 0.8rem; color: #666; margin-bottom: 24px; }}
    h3.route-hdr {{
      font-size: 1rem;
      color: #7ec8e0;
      margin: 16px 0 8px;
      font-weight: bold;
    }}
    .field-check label {{ display: inline-flex; align-items: center; gap: 8px; color: #ccc; font-size: 0.85rem; }}
    .field-check input[type="checkbox"] {{ width: auto; }}
    .tab-bar {{
      display: flex;
      gap: 4px;
      border-bottom: 2px solid #2a2a3a;
      margin-bottom: 24px;
      flex-wrap: wrap;
    }}
    .tab-btn {{
      padding: 8px 18px;
      background: none;
      border: none;
      border-bottom: 2px solid transparent;
      margin-bottom: -2px;
      color: #888;
      font-size: 1rem;
      cursor: pointer;
      font-weight: 500;
      transition: color .15s, border-color .15s;
    }}
    .tab-btn:hover {{ color: #ccc; }}
    .tab-btn.active {{ color: #7ec8e0; border-bottom-color: #7ec8e0; }}
    .tab-panel {{ display: none; max-width: 480px; }}
    .tab-panel.active {{ display: block; }}
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
    input[type="text"]:focus, input[type="password"]:focus, select:focus {{ border-color: #7ec8e0; }}
    select option {{ background: #1e1e2e; }}
    .actions {{ margin-top: 28px; display: flex; align-items: center; gap: 16px; }}
    button#btn-save {{
      padding: 10px 32px;
      border: none;
      border-radius: 6px;
      font-size: 0.9rem;
      cursor: pointer;
      font-weight: 600;
      background: #529d53;
      color: #fff;
      transition: opacity .15s;
    }}
    button#btn-save:hover {{ opacity: 0.85; }}
    #status-msg {{
      font-size: 0.85rem;
      min-height: 1.2em;
      color: #7ec8a0;
    }}
  </style>
</head>
<body>
  <h1>SenseDeck Config</h1>
  <div class="subtitle">sensecap_indicator_customdeck/config.json</div>

  <form id="cfg-form">
    {hidden}
    <div class="tab-bar">{tab_buttons}</div>
    {tab_panels}
    <div class="actions">
      <button type="button" id="btn-save">Salva tutto</button>
      <span id="status-msg"></span>
    </div>
  </form>

  <script>
    document.querySelectorAll('.tab-btn').forEach(btn => {{
      btn.addEventListener('click', () => {{
        document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
        document.querySelectorAll('.tab-panel').forEach(p => p.classList.remove('active'));
        btn.classList.add('active');
        document.getElementById('tab-' + btn.dataset.tab).classList.add('active');
      }});
    }});

    document.getElementById('btn-save').addEventListener('click', async () => {{
      const form = document.getElementById('cfg-form');
      const data = {{}};
      form.querySelectorAll('input, select').forEach(el => {{
        if (el.name) {{
          data[el.name] = el.type === 'checkbox' ? (el.checked ? 'true' : 'false') : el.value;
        }}
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

            # Ricostruisce traffic_routes da campi flat del form
            if any(f"tr_{i}_origin" in data for i in range(2)):
                routes = []
                for i in range(2):
                    enabled_raw = data.pop(f"tr_{i}_enabled", "false")
                    routes.append({
                        "name":        data.pop(f"tr_{i}_name",        f"Route {i+1}"),
                        "origin":      data.pop(f"tr_{i}_origin",      ""),
                        "destination": data.pop(f"tr_{i}_destination", ""),
                        "mode":        data.pop(f"tr_{i}_mode",        "driving"),
                        "enabled":     enabled_raw in ("true", "1", "on"),
                    })
                data["traffic_routes"] = routes
            # Rimuove eventuali campi legacy flat-traffic
            for _k in ("traffic_origin", "traffic_destination", "traffic_mode"):
                data.pop(_k, None)

            # Merge ricorsivo con defaults per campi mancanti
            merged = _merge_config(DEFAULT_CONFIG, data)

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
