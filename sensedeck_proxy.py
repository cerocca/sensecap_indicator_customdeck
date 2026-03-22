#!/usr/bin/env python3
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
    "proxy_ip":       "192.168.1.70",
    "proxy_port":     "8765",
    "launcher_url_1": "https://github.com/cerocca/",
    "launcher_url_2": "https://www.strava.com",
    "launcher_url_3": "https://connect.garmin.com/",
    "launcher_url_4": "https://intervals.icu/",
}

UPTIME_KUMA_URL = "http://192.168.1.69:3010/api/status-page/heartbeat/active"
LISTEN_PORT     = 8765

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
        return f"""
        <div class="field">
          <label for="{name}">{label}</label>
          <input type="text" id="{name}" name="{name}" value="{value}" placeholder="{placeholder}">
        </div>"""

    def hint(text):
        return f'<p class="hint">{text}</p>'

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
      margin: 28px 0 12px;
      padding-bottom: 6px;
      border-bottom: 1px solid #2a2a3a;
    }}
    .field {{ margin-bottom: 14px; }}
    label {{
      display: block;
      font-size: 0.78rem;
      color: #999;
      margin-bottom: 4px;
    }}
    input[type="text"] {{
      width: 100%;
      max-width: 480px;
      background: #1e1e2e;
      border: 1px solid #333;
      border-radius: 6px;
      color: #e0e0e0;
      padding: 8px 10px;
      font-size: 0.9rem;
      outline: none;
      transition: border-color .15s;
    }}
    input[type="text"]:focus {{ border-color: #7ec8a0; }}
    .actions {{
      margin-top: 32px;
      display: flex;
      gap: 12px;
      flex-wrap: wrap;
    }}
    button {{
      padding: 10px 24px;
      border: none;
      border-radius: 6px;
      font-size: 0.9rem;
      cursor: pointer;
      font-weight: 600;
      transition: opacity .15s;
    }}
    button:hover {{ opacity: 0.85; }}
    #btn-save   {{ background: #529d53; color: #fff; }}
    #btn-send   {{ background: #2a2a3a; color: #888; border: 1px solid #444; cursor: not-allowed; }}
    #status-msg {{
      margin-top: 16px;
      font-size: 0.85rem;
      min-height: 1.2em;
      color: #7ec8a0;
    }}
    .hint {{
      font-size: 0.72rem;
      color: #555;
      margin-top: -10px;
      margin-bottom: 14px;
    }}
  </style>
</head>
<body>
  <h1>SenseDeck Config</h1>
  <div class="subtitle">Configurazione dispositivo — sensecap_indicator_customdeck/config.json</div>

  <form id="cfg-form">
    <h2>Hue Bridge</h2>
    {field("IP Bridge", "hue_bridge_ip", cfg.get("hue_bridge_ip",""), "192.168.1.x")}
    {field("API Key", "hue_api_key", cfg.get("hue_api_key",""))}
    {field("Luce 1 — Nome", "hue_light_1", cfg.get("hue_light_1",""), "Light 1")}
    {hint("Nome visualizzato sul device (libero, non deve corrispondere al nome in Hue)")}
    {field("Luce 1 — ID", "hue_light_1_id", cfg.get("hue_light_1_id",""), "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx")}
    {hint("ID univoco della luce (UUID dal Hue Bridge)")}
    {field("Luce 2 — Nome", "hue_light_2", cfg.get("hue_light_2",""), "Light 2")}
    {hint("Nome visualizzato sul device (libero, non deve corrispondere al nome in Hue)")}
    {field("Luce 2 — ID", "hue_light_2_id", cfg.get("hue_light_2_id",""), "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx")}
    {hint("ID univoco della luce (UUID dal Hue Bridge)")}
    {field("Luce 3 — Nome", "hue_light_3", cfg.get("hue_light_3",""), "Light 3")}
    {hint("Nome visualizzato sul device (libero, non deve corrispondere al nome in Hue)")}
    {field("Luce 3 — ID", "hue_light_3_id", cfg.get("hue_light_3_id",""), "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx")}
    {hint("ID univoco della luce (UUID dal Hue Bridge)")}
    {field("Luce 4 — Nome", "hue_light_4", cfg.get("hue_light_4",""), "Light 4")}
    {hint("Nome visualizzato sul device (libero, non deve corrispondere al nome in Hue)")}
    {field("Luce 4 — ID", "hue_light_4_id", cfg.get("hue_light_4_id",""), "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx")}
    {hint("ID univoco della luce (UUID dal Hue Bridge)")}

    <h2>LocalServer (Glances)</h2>
    {field("IP Server", "server_ip", cfg.get("server_ip",""), "192.168.1.x")}
    {field("Porta Glances", "server_port", cfg.get("server_port",""), "61208")}

    <h2>Proxy Mac</h2>
    {field("IP Proxy", "proxy_ip", cfg.get("proxy_ip",""), "192.168.1.x")}
    {field("Porta", "proxy_port", cfg.get("proxy_port",""), "8765")}

    <h2>Launcher</h2>
    {field("URL 1", "launcher_url_1", cfg.get("launcher_url_1",""))}
    {field("URL 2", "launcher_url_2", cfg.get("launcher_url_2",""))}
    {field("URL 3", "launcher_url_3", cfg.get("launcher_url_3",""))}
    {field("URL 4", "launcher_url_4", cfg.get("launcher_url_4",""))}

    <div class="actions">
      <button type="button" id="btn-save">Salva</button>
      <button type="button" id="btn-send" disabled title="Non ancora implementato">
        Invia al device
      </button>
    </div>
    <div id="status-msg"></div>
  </form>

  <script>
    document.getElementById('btn-save').addEventListener('click', async () => {{
      const form = document.getElementById('cfg-form');
      const data = {{}};
      form.querySelectorAll('input[type="text"]').forEach(el => {{
        data[el.name] = el.value;
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
          msg.textContent = '✓ Salvato';
        }} else {{
          msg.style.color = '#e07070';
          msg.textContent = '✗ Errore ' + res.status;
        }}
      }} catch(e) {{
        msg.style.color = '#e07070';
        msg.textContent = '✗ ' + e.message;
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
                req = urllib.request.Request(UPTIME_KUMA_URL)
                with urllib.request.urlopen(req, timeout=5) as resp:
                    raw = json.loads(resp.read())

                result = []
                for monitor_id, heartbeats in raw.get("heartbeatList", {}).items():
                    if not heartbeats:
                        continue
                    last = heartbeats[-1]
                    result.append({
                        "id":     int(monitor_id),
                        "status": last["status"],
                        "ping":   last.get("ping", 0),
                    })

                self.send_json({"monitors": result})

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
    print(f"  /uptime     → Uptime Kuma {UPTIME_KUMA_URL}")
    for k in ["1", "2", "3", "4"]:
        print(f"  /open/{k}     → {cfg.get(f'launcher_url_{k}', '?')}")
    print(f"  /config     → GET config / POST save")
    print(f"  /config/ui  → Web UI ({CONFIG_PATH})")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStop.")
