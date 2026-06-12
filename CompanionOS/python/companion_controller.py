#!/usr/bin/env python3
"""
═══════════════════════════════════════════════════════════
  COMPANION OS - Python Controller v3.0

  Controls ESP8266 desk companion.
  Integrates Spotify, Weather, Notifications, Pomodoro, Notes.
═══════════════════════════════════════════════════════════
"""

import os
import sys
import json
import time
import socket
import subprocess
from datetime import datetime
import threading
import struct

# Ensure local modules are importable
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)

from spotify_integration import SpotifyIntegration  # type: ignore # noqa: E402
from github_integration import GitHubIntegration  # type: ignore # noqa: E402
from steam_tracker import SteamTracker  # type: ignore # noqa: E402
from google_cal import GoogleCalendar, LocalTaskList  # type: ignore # noqa: E402
from stock_manager import StockManager  # type: ignore # noqa: E402
from theme2_bridge import theme2_spotify_feed  # type: ignore # noqa: E402
from ruview_processor import (  # type: ignore # noqa: E402
    ruview_listener_loop, ruview_push_loop, zone_manager,
    ruview_enabled
)

CONFIG_FILE = os.path.join(SCRIPT_DIR, "config.json")

def load_config():
    if not os.path.exists(CONFIG_FILE):
        print(f"ERROR: {CONFIG_FILE} not found! Run tools/install.py first.")
        sys.exit(1)
    with open(CONFIG_FILE, 'r') as f:
        return json.load(f)

config = load_config()

# Load secrets
SPOTIFY_CLIENT_ID = os.getenv("SPOTIFY_CLIENT_ID", config['spotify']['client_id'])
SPOTIFY_CLIENT_SECRET = os.getenv("SPOTIFY_CLIENT_SECRET", config['spotify']['client_secret'])
SPOTIFY_REDIRECT_URI = config['spotify']['redirect_uri']
SPOTIFY_SCOPE = config['spotify']['scope']

GITHUB_USERNAME = os.getenv("GITHUB_USERNAME", config['github']['username'])
GITHUB_TOKEN = os.getenv("GITHUB_TOKEN", config['github'].get('token', ''))

gaming_cfg = config.get('gaming', {})
if gaming_cfg.get('twitch_client_id'):
    os.environ['TWITCH_CLIENT_ID'] = gaming_cfg.get('twitch_client_id', '')
if gaming_cfg.get('twitch_client_secret'):
    os.environ['TWITCH_CLIENT_SECRET'] = gaming_cfg.get('twitch_client_secret', '')


WEATHER_KEY = config.get('weather', {}).get('api_key', '')
WEATHER_CITY = config.get('weather', {}).get('city', 'auto:ip')

ESP_IP = config['network']['esp_ip']
ESP_PORT_RX = config['network']['esp_port_rx']
ESP_PORT_TX = config['network']['esp_port_tx']
PC_PORT_RX = config['network']['pc_port_rx']

SPOTIFY_POLL = config['update_intervals']['spotify_poll_seconds']
GITHUB_REFRESH = config['update_intervals']['github_refresh_minutes'] * 60
WEATHER_REFRESH = config.get('update_intervals', {}).get('weather_refresh_minutes', 15) * 60

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
active_esp_ip = ESP_IP if "192.168.1" not in ESP_IP else None
force_resync = False
fast_poll_now = False

# Timer state
pomodoro_active = False
pomodoro_is_break = False
pomodoro_duration = 25 * 60
pomodoro_break_duration = 5 * 60
pomodoro_remaining = pomodoro_duration
pomodoro_last_tick = 0.0
pomodoro_sessions = 0

# Notification storage
from typing import List, Dict, Any, cast
notifications: List[Dict[str, Any]] = []

spotify_service = SpotifyIntegration(
    client_id=SPOTIFY_CLIENT_ID,
    client_secret=SPOTIFY_CLIENT_SECRET,
    redirect_uri=SPOTIFY_REDIRECT_URI,
    scope=SPOTIFY_SCOPE,
    lyrics_enabled=config['features'].get('lyrics_enabled', True)
)

github_service = GitHubIntegration(username=GITHUB_USERNAME, token=GITHUB_TOKEN)

# ── V6: New service instances ──────────────────────────
gaming_cfg = config.get('gaming', {})
steam_service = SteamTracker(
    steam_api_key=gaming_cfg.get('steam_api_key', ''),
    steam_id=gaming_cfg.get('steam_id', '')
)

prod_cfg = config.get('productivity', {})
gcal_creds = prod_cfg.get('google_credentials', '')
if gcal_creds and os.path.exists(os.path.join(SCRIPT_DIR, gcal_creds)):
    productivity_service = GoogleCalendar(os.path.join(SCRIPT_DIR, gcal_creds))
else:
    productivity_service = LocalTaskList()

stock_service = StockManager()


def send_udp(message):
    target_ip = active_esp_ip if active_esp_ip else ESP_IP
    try:
        sock.sendto(message.encode(), (target_ip, ESP_PORT_RX))
    except Exception as e:
        print(f"UDP send error to {target_ip}: {e}")

def send_udp_bytes(packet):
    target_ip = active_esp_ip if active_esp_ip else ESP_IP
    try:
        sock.sendto(packet, (target_ip, ESP_PORT_RX))
    except Exception as e:
        pass

current_lyrics = []  # Make global so background thread can update it
last_sent_lyrics = None
_asset_generation = 0
_asset_generation_lock = threading.Lock()

def _next_asset_generation():
    global _asset_generation
    with _asset_generation_lock:
        _asset_generation += 1
        return _asset_generation

def _is_current_asset_generation(generation):
    with _asset_generation_lock:
        return generation == _asset_generation

def fetch_heavy_assets(track_name, artist_name, track_id, album_art_url, generation):
    """Background thread to process lyrics and album art without blocking UDP STATE updates."""
    global current_lyrics
    
    # 1. Fetch Lyrics (Slow HTTP Call to LRCLib)
    if spotify_service.lyrics_enabled:
        lyrics = spotify_service.get_lyrics(track_name, artist_name, track_id)
        if not _is_current_asset_generation(generation):
            return
        current_lyrics = lyrics
    else:
        current_lyrics = []
        
    # 2. Fetch and Process Album Art (Slow HTTP Image CDN + Resize Array Math)
    if album_art_url:
        try:
            album_size = 64
            pixels = spotify_service.process_album_art(album_art_url, size=album_size)
            if not pixels: return
            if not _is_current_asset_generation(generation):
                return
            
            send_udp("ART_START:")
            time.sleep(0.1)   # Allow ESP memory clear margin
            if not _is_current_asset_generation(generation):
                return
            
            pixels_per_chunk = 64 * 4  # 4 rows of 64 pixels = 256 pixels = 512 bytes
            
            # Convert normal RGB565 to raw byte array
            flat_bytes = []
            for c in pixels:
                flat_bytes.append((c >> 8) & 0xFF) # High byte
                flat_bytes.append(c & 0xFF)        # Low byte
            byte_array = bytes(flat_bytes)
            
            for i in range(0, len(byte_array), pixels_per_chunk * 2):
                if not _is_current_asset_generation(generation):
                    return
                chunk_data = byte_array[i:i+(pixels_per_chunk*2)]  # type: ignore
                idx = i // (64 * 2) # keep idx aligned with rows (each row is 64 pixels)
                
                # Custom Binary Packet Header: 0xFE identifies raw binary over UDP
                packet = bytes([0xFE, (idx >> 8) & 0xFF, idx & 0xFF]) + chunk_data
                send_udp_bytes(packet)
                time.sleep(0.02)  # Give ESP32 time to render via SPI without dropping next UDP packet
                
            send_udp("ART_COMPLETE:")
        except Exception as e:
            print(f"Album art transmission error: {e}")


def fetch_weather():
    """Fetch weather from weatherapi.com"""
    if not WEATHER_KEY:
        return None
    try:
        import requests  # type: ignore
        url = f"http://api.weatherapi.com/v1/forecast.json?key={WEATHER_KEY}&q={WEATHER_CITY}&days=1&aqi=no"
        resp = requests.get(url, timeout=10)
        if resp.status_code == 200:
            data = resp.json()
            current = data['current']
            forecast = data['forecast']['forecastday'][0]
            astro = forecast['astro']
            result = {
                'temp': round(current['temp_c']),
                'feels': round(current['feelslike_c']),
                'humidity': current['humidity'],
                'condition': current['condition']['text'][:20],
                'code': current['condition']['code'],
                'wind': round(current['wind_kph']),
                'sunrise': astro['sunrise'],
                'sunset': astro['sunset'],
                'high': round(forecast['day']['maxtemp_c']),
                'low': round(forecast['day']['mintemp_c']),
                'city': data['location']['name'][:15]
            }
            return result
    except Exception as e:
        print(f"Weather API Error: {e}")
    return None


def capture_windows_notifications():
    """V4: Capture Windows 10/11 toast notifications using PowerShell"""
    notif_file = os.path.join(SCRIPT_DIR, "notifications.json")
    
    # PowerShell script to extract toast notifications from Action Center
    ps_script = r'''
    try {
        [Windows.UI.Notifications.ToastNotificationManager, Windows.UI.Notifications, ContentType = WindowsRuntime] | Out-Null
        [Windows.Data.Xml.Dom.XmlDocument, Windows.Data.Xml.Dom.XmlDocument, ContentType = WindowsRuntime] | Out-Null
        $notifications = [Windows.UI.Notifications.ToastNotificationManager]::History.GetHistory()
        $results = @()
        foreach ($n in $notifications) {
            if ($n.AppId -eq $null) { continue }
            $texts = $n.Content.GetElementsByTagName('text')
            if ($texts.Count -eq 0 -or $texts[0].InnerText -eq $null) { continue }
            $results += @{
                app = $n.AppId.Substring(0, [Math]::Min($n.AppId.Length, 15))
                title = $texts[0].InnerText.Substring(0, [Math]::Min($texts[0].InnerText.Length, 30))
                time = $n.ExpirationTime.ToString('HH:mm')
            }
        }
        $results | ConvertTo-Json -Compress
    } catch {
        '[]'
    }
    '''
    
    while True:
        try:
            result = subprocess.run(
                ['powershell', '-Command', ps_script],
                capture_output=True, text=True, timeout=15
            )
            if result.stdout.strip():
                try:
                    notifs = json.loads(result.stdout.strip())
                    if isinstance(notifs, dict):  # Single notification comes as dict
                        notifs = [notifs]
                    if notifs and isinstance(notifs, list):
                        new_os_notifs = cast(List[Dict[str, Any]], notifs[-10:])
                        
                        # Read existing to not overwrite API pushes
                        existing = []
                        if os.path.exists(notif_file):
                            try:
                                with open(notif_file, 'r') as f:
                                    loaded = json.load(f)
                                    if isinstance(loaded, list):
                                        existing = loaded
                            except Exception:
                                pass
                        
                        # Add new OS notifs if they don't already exist (simple check by title/time)
                        for n in new_os_notifs:
                            is_dup = False
                            for e in existing:
                                if e.get('title') == n.get('title') and e.get('time') == n.get('time'):
                                    is_dup = True
                                    break
                            if not is_dup:
                                existing.append(n)
                        
                        notifications.clear()
                        notifications.extend(existing[-20:])
                        
                        # Write to file for persistence
                        with open(notif_file, 'w') as f:
                            json.dump(list(notifications), f)
                        
                        # Send to ESP
                        summary = []
                        for n in list(notifications)[-3:]:  # Send latest 3
                            summary.append({
                                'app': str(n.get('app', '?'))[:10],
                                'title': str(n.get('title', ''))[:20],
                                'time': str(n.get('time', ''))
                            })
                        send_udp(f"NOTIF:{json.dumps(summary)}")
                        print(f"📬 Scraped {len(notifs)} Windows notifications")
                except json.JSONDecodeError:
                    pass
            time.sleep(10)  # Poll every 10 seconds
        except subprocess.TimeoutExpired:
            time.sleep(15)
        except Exception as e:
            # Fallback to file-based method if PowerShell fails
            try:
                if os.path.exists(notif_file):
                    with open(notif_file, 'r') as f:
                        notifs = json.load(f)
                    if notifs:
                        summary = []
                        for n in list(notifs)[-3:]:  # type: ignore
                            summary.append({
                                'app': str(n.get('app', '?'))[:10],  # type: ignore
                                'title': str(n.get('title', ''))[:20],  # type: ignore
                                'time': str(n.get('time', ''))
                            })
                        send_udp(f"NOTIF:{json.dumps(summary)}")
            except Exception:
                pass
            time.sleep(10)



# ═══════════════════════════════════════════════════════════
# RUVIEW CSI PRESENCE MAP — HTML UI
# ═══════════════════════════════════════════════════════════

RUVIEW_HTML = """<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>RuView — CSI Presence Map</title>
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap" rel="stylesheet">
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Inter', sans-serif;
            background: #0a0a12;
            color: #e0e0e0;
            min-height: 100vh;
            overflow-x: hidden;
        }

        /* Header */
        .header {
            background: linear-gradient(135deg, #12121e 0%, #1a1a2e 100%);
            border-bottom: 1px solid rgba(120, 0, 255, 0.3);
            padding: 16px 24px;
            display: flex;
            align-items: center;
            justify-content: space-between;
        }
        .header h1 {
            font-size: 20px;
            font-weight: 700;
            background: linear-gradient(135deg, #a855f7, #6366f1);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }
        .header .subtitle { font-size: 11px; color: #666; margin-top: 2px; }
        .header .back-link {
            color: #888;
            text-decoration: none;
            font-size: 13px;
            transition: color 0.2s;
        }
        .header .back-link:hover { color: #a855f7; }

        /* Controls bar */
        .controls {
            display: flex;
            gap: 10px;
            padding: 12px 24px;
            background: #0d0d18;
            border-bottom: 1px solid #1a1a2e;
            flex-wrap: wrap;
        }
        .btn {
            padding: 8px 16px;
            border: 1px solid #2a2a3e;
            border-radius: 8px;
            background: #14141f;
            color: #ccc;
            font-family: 'Inter', sans-serif;
            font-size: 12px;
            font-weight: 500;
            cursor: pointer;
            transition: all 0.2s;
        }
        .btn:hover { border-color: #a855f7; color: #a855f7; }
        .btn.active { background: #a855f7; color: white; border-color: #a855f7; }
        .btn.danger { border-color: #ef4444; color: #ef4444; }
        .btn.danger:hover { background: #ef4444; color: white; }
        .btn.success { border-color: #22c55e; color: #22c55e; }
        .btn.success:hover { background: #22c55e; color: white; }

        /* Main grid */
        .container { padding: 20px 24px; }

        /* Zone card */
        .zone-card {
            background: linear-gradient(135deg, #12121e 0%, #1a1a2e 100%);
            border: 1px solid #2a2a3e;
            border-radius: 16px;
            padding: 24px;
            margin-bottom: 16px;
            transition: all 0.3s;
        }
        .zone-card.motion { border-color: #ef4444; box-shadow: 0 0 20px rgba(239, 68, 68, 0.15); }
        .zone-card.occupied { border-color: #22c55e; box-shadow: 0 0 20px rgba(34, 197, 94, 0.15); }
        .zone-card.calibrating { border-color: #eab308; box-shadow: 0 0 20px rgba(234, 179, 8, 0.15); }
        .zone-card.empty { border-color: #2a2a3e; }

        .zone-header {
            display: flex;
            align-items: center;
            justify-content: space-between;
            margin-bottom: 16px;
        }
        .zone-label {
            font-size: 18px;
            font-weight: 600;
            color: #e0e0e0;
            cursor: pointer;
            border: none;
            background: transparent;
            font-family: 'Inter', sans-serif;
            padding: 2px 6px;
            border-radius: 4px;
        }
        .zone-label:hover { background: #1a1a2e; }
        .zone-label:focus { outline: 1px solid #a855f7; background: #1a1a2e; }

        /* Status indicator */
        .status-indicator {
            display: flex;
            align-items: center;
            gap: 8px;
        }
        .status-dot {
            width: 12px;
            height: 12px;
            border-radius: 50%;
            background: #444;
            transition: all 0.3s;
        }
        .status-dot.motion { background: #ef4444; animation: pulse 1s ease-in-out infinite; }
        .status-dot.occupied { background: #22c55e; }
        .status-dot.calibrating { background: #eab308; animation: pulse 1.5s ease-in-out infinite; }
        .status-dot.empty { background: #444; }

        @keyframes pulse {
            0%, 100% { opacity: 1; transform: scale(1); }
            50% { opacity: 0.5; transform: scale(1.3); }
        }

        .status-text { font-size: 14px; font-weight: 600; }
        .status-text.motion { color: #ef4444; }
        .status-text.occupied { color: #22c55e; }
        .status-text.calibrating { color: #eab308; }
        .status-text.empty { color: #666; }

        /* Confidence bar */
        .conf-row {
            display: flex;
            align-items: center;
            gap: 12px;
            margin-bottom: 16px;
        }
        .conf-label { font-size: 11px; color: #666; width: 60px; }
        .conf-bar-bg {
            flex: 1;
            height: 6px;
            background: #1a1a2e;
            border-radius: 3px;
            overflow: hidden;
        }
        .conf-bar-fill {
            height: 100%;
            border-radius: 3px;
            transition: width 0.5s ease, background 0.3s;
            background: #22c55e;
        }
        .conf-value { font-size: 12px; color: #aaa; width: 36px; text-align: right; }

        /* Metrics grid */
        .metrics {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(130px, 1fr));
            gap: 12px;
        }
        .metric {
            background: #0a0a14;
            border: 1px solid #1a1a2e;
            border-radius: 10px;
            padding: 12px;
            text-align: center;
        }
        .metric-value {
            font-size: 20px;
            font-weight: 700;
            color: #e0e0e0;
        }
        .metric-label {
            font-size: 10px;
            color: #666;
            margin-top: 4px;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }
        .metric-value.cyan { color: #06b6d4; }
        .metric-value.green { color: #22c55e; }
        .metric-value.yellow { color: #eab308; }
        .metric-value.red { color: #ef4444; }

        /* Warnings */
        .warnings {
            margin-top: 20px;
            padding: 16px;
            background: #14141f;
            border: 1px solid #2a2a3e;
            border-radius: 12px;
        }
        .warnings h3 {
            font-size: 13px;
            color: #eab308;
            margin-bottom: 8px;
        }
        .warnings ul {
            list-style: none;
            padding: 0;
        }
        .warnings li {
            font-size: 12px;
            color: #888;
            padding: 4px 0;
            padding-left: 16px;
            position: relative;
        }
        .warnings li::before {
            content: '⚠';
            position: absolute;
            left: 0;
        }

        /* No zones placeholder */
        .no-zones {
            text-align: center;
            padding: 60px 20px;
            color: #444;
        }
        .no-zones .icon { font-size: 48px; margin-bottom: 16px; }
        .no-zones p { font-size: 14px; max-width: 400px; margin: 0 auto; line-height: 1.6; }
    </style>
</head>
<body>
    <div class="header">
        <div>
            <h1>📡 RuView Presence Map</h1>
            <div class="subtitle">WiFi CSI Presence & Motion Detection</div>
        </div>
        <a href="/" class="back-link">← Back to Remote</a>
    </div>

    <div class="controls">
        <button class="btn" id="toggleBtn" onclick="toggleCSI()">⏸ Disable</button>
        <button class="btn success" onclick="recalibrate()">🔄 Recalibrate (60s)</button>
        <span style="flex:1"></span>
        <span style="font-size:11px; color:#666; align-self:center;" id="updateTime">—</span>
    </div>

    <div class="container">
        <div id="zonesContainer">
            <div class="no-zones">
                <div class="icon">📡</div>
                <p>Waiting for CSI data from ESP32 node...<br>
                Make sure a CSI node is flashed and broadcasting ADR-018 frames on UDP port 8890.</p>
            </div>
        </div>

        <div class="warnings">
            <h3>Known False-Positive Sources</h3>
            <ul>
                <li>Microwave ovens near the CSI node antenna</li>
                <li>Large oscillating fans in the sensing zone</li>
                <li>Neighboring AP power fluctuations or channel changes</li>
                <li>Metallic objects moving (e.g., ceiling fan, automated blinds)</li>
                <li>Pets (cats/dogs) — detected as presence/motion</li>
            </ul>
        </div>
    </div>

    <script>
        let csiEnabled = true;

        async function fetchState() {
            try {
                const res = await fetch('/api/ruview/state');
                const data = await res.json();
                csiEnabled = data.enabled;

                const btn = document.getElementById('toggleBtn');
                btn.textContent = csiEnabled ? '⏸ Disable' : '▶ Enable';
                btn.className = csiEnabled ? 'btn danger' : 'btn success';

                document.getElementById('updateTime').textContent =
                    'Updated: ' + new Date().toLocaleTimeString();

                renderZones(data.zones || []);
            } catch (e) {
                console.error('Fetch error:', e);
            }
        }

        function renderZones(zones) {
            const container = document.getElementById('zonesContainer');

            if (!zones.length) {
                container.innerHTML = `
                    <div class="no-zones">
                        <div class="icon">📡</div>
                        <p>Waiting for CSI data from ESP32 node...<br>
                        Make sure a CSI node is flashed and broadcasting ADR-018 frames on UDP port 8890.</p>
                    </div>`;
                return;
            }

            let html = '';
            for (const z of zones) {
                const cls = z.calibrating ? 'calibrating' :
                            z.motion ? 'motion' :
                            z.occupied ? 'occupied' : 'empty';

                const confColor = z.confidence > 70 ? '#ef4444' :
                                  z.confidence > 40 ? '#eab308' : '#22c55e';

                const rssiColor = z.rssi >= -60 ? 'green' :
                                  z.rssi >= -75 ? 'yellow' : 'red';

                html += `
                <div class="zone-card ${cls}">
                    <div class="zone-header">
                        <input class="zone-label" value="${z.label || 'Room'}"
                               onchange="renameZone(${z.node_id}, this.value)" />
                        <div class="status-indicator">
                            <div class="status-dot ${cls}"></div>
                            <span class="status-text ${cls}">${z.status || 'Unknown'}</span>
                        </div>
                    </div>

                    <div class="conf-row">
                        <span class="conf-label">Confidence</span>
                        <div class="conf-bar-bg">
                            <div class="conf-bar-fill" style="width:${z.confidence || 0}%; background:${confColor}"></div>
                        </div>
                        <span class="conf-value">${(z.confidence || 0).toFixed(0)}%</span>
                    </div>

                    <div class="metrics">
                        <div class="metric">
                            <div class="metric-value">${(z.variance || 0).toFixed(3)}</div>
                            <div class="metric-label">Variance</div>
                        </div>
                        <div class="metric">
                            <div class="metric-value ${rssiColor}">${z.rssi || 0} dBm</div>
                            <div class="metric-label">RSSI</div>
                        </div>
                        <div class="metric">
                            <div class="metric-value cyan">${(z.pps || 0).toFixed(0)}</div>
                            <div class="metric-label">Packets/sec</div>
                        </div>
                        <div class="metric">
                            <div class="metric-value">${z.subcarriers || 0}</div>
                            <div class="metric-label">Subcarriers</div>
                        </div>
                        <div class="metric">
                            <div class="metric-value green">${z.thresh_presence || '—'}</div>
                            <div class="metric-label">Presence Thresh</div>
                        </div>
                        <div class="metric">
                            <div class="metric-value red">${z.thresh_motion || '—'}</div>
                            <div class="metric-label">Motion Thresh</div>
                        </div>
                    </div>
                </div>`;
            }
            container.innerHTML = html;
        }

        async function toggleCSI() {
            await fetch('/api/ruview/toggle', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({enabled: !csiEnabled})
            });
            fetchState();
        }

        async function recalibrate() {
            if (!confirm('Start 60-second recalibration?\\nLeave the room EMPTY during calibration.')) return;
            await fetch('/api/ruview/recalibrate', {method: 'POST'});
            fetchState();
        }

        async function renameZone(nodeId, label) {
            await fetch('/api/ruview/rename', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({node_id: nodeId, label: label})
            });
        }

        // Poll every 2 seconds
        fetchState();
        setInterval(fetchState, 2000);
    </script>
</body>
</html>"""


def start_notes_server():
    """Start Flask web server for Web Remote + Quick Notes"""
    try:
        from flask import Flask, request, redirect, jsonify, send_from_directory  # type: ignore
        app = Flask(__name__)
        notes_file = os.path.join(SCRIPT_DIR, "notes.txt")
        port = config['features'].get('notes_web_port', 5555)
        ruview_ui_dir = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "..", "RuView", "ui"))
        
        @app.route('/')
        def index():
            notes = ""
            if os.path.exists(notes_file):
                with open(notes_file, 'r') as f:
                    notes = f.read()
            esp_ip = active_esp_ip if active_esp_ip else ESP_IP
            wifi_status = "Connected" if active_esp_ip else "Searching..."
            return f"""
            <!DOCTYPE html>
            <html lang="en">
            <head>
                <meta charset="UTF-8">
                <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
                <title>CompanionOS Remote</title>
                <link rel="preconnect" href="https://fonts.googleapis.com">
                <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;600;700&display=swap" rel="stylesheet">
                <style>
                    * {{ margin: 0; padding: 0; box-sizing: border-box; }}
                    body {{
                        font-family: 'Inter', 'Segoe UI', sans-serif;
                        background: #0a0a1a;
                        color: #e0e0e0;
                        min-height: 100vh;
                        display: flex;
                        flex-direction: column;
                        align-items: center;
                        padding: 20px 16px;
                    }}
                    .header {{
                        text-align: center;
                        margin-bottom: 24px;
                    }}
                    .header h1 {{
                        font-size: 1.5rem;
                        font-weight: 700;
                        background: linear-gradient(135deg, #00d4ff, #7b2ff7);
                        -webkit-background-clip: text;
                        -webkit-text-fill-color: transparent;
                        background-clip: text;
                    }}
                    .header .status {{
                        font-size: 0.75rem;
                        color: #666;
                        margin-top: 4px;
                    }}
                    .header .status .dot {{
                        display: inline-block;
                        width: 8px; height: 8px;
                        border-radius: 50%;
                        background: {'#00e676' if active_esp_ip else '#ff5252'};
                        margin-right: 4px;
                        vertical-align: middle;
                    }}

                    /* ── REMOTE SECTION ── */
                    .remote {{
                        background: linear-gradient(145deg, #111128, #0d0d20);
                        border: 1px solid #1a1a3e;
                        border-radius: 20px;
                        padding: 28px 20px;
                        width: 100%;
                        max-width: 400px;
                        margin-bottom: 24px;
                        box-shadow: 0 8px 32px rgba(0, 0, 0, 0.6);
                    }}
                    .remote-title {{
                        text-align: center;
                        font-size: 0.7rem;
                        text-transform: uppercase;
                        letter-spacing: 3px;
                        color: #555;
                        margin-bottom: 20px;
                    }}
                    .btn-row {{
                        display: flex;
                        justify-content: center;
                        align-items: center;
                        gap: 12px;
                        margin-bottom: 16px;
                    }}
                    .btn {{
                        border: none;
                        border-radius: 16px;
                        cursor: pointer;
                        font-family: 'Inter', sans-serif;
                        font-weight: 600;
                        transition: all 0.15s ease;
                        user-select: none;
                        -webkit-tap-highlight-color: transparent;
                        position: relative;
                        overflow: hidden;
                    }}
                    .btn:active {{
                        transform: scale(0.92);
                    }}
                    .btn::after {{
                        content: '';
                        position: absolute;
                        inset: 0;
                        background: rgba(255,255,255,0.1);
                        opacity: 0;
                        transition: opacity 0.2s;
                        border-radius: inherit;
                    }}
                    .btn:hover::after {{ opacity: 1; }}

                    .btn-nav {{
                        width: 72px; height: 72px;
                        font-size: 1.6rem;
                        background: linear-gradient(145deg, #1a1a3e, #12122a);
                        color: #8888cc;
                        border: 1px solid #2a2a5e;
                        box-shadow: 0 4px 12px rgba(0,0,0,0.4);
                    }}
                    .btn-nav:active {{
                        background: linear-gradient(145deg, #2a2a5e, #1a1a3e);
                        color: #aaaaee;
                        box-shadow: 0 2px 6px rgba(0,0,0,0.6);
                    }}

                    .btn-select {{
                        width: 88px; height: 88px;
                        font-size: 1.1rem;
                        background: linear-gradient(145deg, #7b2ff7, #5a1fd4);
                        color: #fff;
                        border: 1px solid #9b4fff;
                        box-shadow: 0 4px 20px rgba(123, 47, 247, 0.3);
                        border-radius: 50%;
                    }}
                    .btn-select:active {{
                        background: linear-gradient(145deg, #9b4fff, #7b2ff7);
                        box-shadow: 0 2px 10px rgba(123, 47, 247, 0.5);
                    }}

                    .btn-home {{
                        width: 100%;
                        max-width: 240px;
                        height: 44px;
                        font-size: 0.85rem;
                        background: linear-gradient(145deg, #1a1a3e, #12122a);
                        color: #00d4ff;
                        border: 1px solid #0f3460;
                        box-shadow: 0 4px 12px rgba(0,0,0,0.3);
                        letter-spacing: 1px;
                    }}
                    .btn-home:active {{
                        background: linear-gradient(145deg, #0f3460, #1a1a3e);
                        color: #4de8ff;
                    }}

                    .btn-row-home {{
                        display: flex;
                        justify-content: center;
                        margin-top: 4px;
                    }}

                    .feedback {{
                        text-align: center;
                        font-size: 0.7rem;
                        color: #333;
                        margin-top: 12px;
                        min-height: 18px;
                        transition: color 0.3s;
                    }}
                    .feedback.active {{ color: #7b2ff7; }}

                    /* ── PAGE NAV SECTION ── */
                    .page-nav {{
                        display: flex;
                        flex-wrap: wrap;
                        justify-content: center;
                        gap: 8px;
                        width: 100%;
                        max-width: 400px;
                        margin-bottom: 24px;
                    }}
                    .page-btn {{
                        padding: 6px 12px;
                        font-size: 0.7rem;
                        background: #111128;
                        color: #666;
                        border: 1px solid #1a1a3e;
                        border-radius: 8px;
                        cursor: pointer;
                        font-family: 'Inter', sans-serif;
                        transition: all 0.15s;
                        user-select: none;
                        -webkit-tap-highlight-color: transparent;
                    }}
                    .page-btn:hover {{ background: #1a1a3e; color: #aaa; }}
                    .page-btn:active {{ background: #2a2a5e; color: #fff; transform: scale(0.95); }}

                    /* ── NOTES SECTION ── */
                    .notes {{
                        background: linear-gradient(145deg, #111128, #0d0d20);
                        border: 1px solid #1a1a3e;
                        border-radius: 16px;
                        padding: 20px;
                        width: 100%;
                        max-width: 400px;
                        box-shadow: 0 4px 16px rgba(0,0,0,0.4);
                    }}
                    .notes h2 {{
                        font-size: 0.9rem;
                        color: #00d4ff;
                        margin-bottom: 12px;
                    }}
                    textarea {{
                        width: 100%;
                        height: 100px;
                        background: #0a0a18;
                        color: #e0e0e0;
                        border: 1px solid #1a1a3e;
                        border-radius: 10px;
                        padding: 12px;
                        font-size: 14px;
                        font-family: 'Inter', sans-serif;
                        resize: vertical;
                        outline: none;
                    }}
                    textarea:focus {{ border-color: #7b2ff7; }}
                    .notes-submit {{
                        width: 100%;
                        margin-top: 10px;
                        padding: 10px;
                        background: linear-gradient(135deg, #00d4ff, #007bff);
                        color: #000;
                        font-weight: 600;
                        border: none;
                        border-radius: 10px;
                        font-size: 14px;
                        cursor: pointer;
                        font-family: 'Inter', sans-serif;
                    }}
                    .notes-submit:hover {{ opacity: 0.9; }}
                    .current-notes {{
                        background: #0a0a18;
                        padding: 12px;
                        border-radius: 8px;
                        margin-top: 12px;
                    }}
                    .current-notes pre {{
                        color: #8888cc;
                        white-space: pre-wrap;
                        font-size: 13px;
                        font-family: 'Inter', monospace;
                    }}
                    .current-notes h3 {{
                        font-size: 0.75rem;
                        color: #555;
                        margin-bottom: 8px;
                        text-transform: uppercase;
                        letter-spacing: 1px;
                    }}
                </style>
            </head>
            <body>
                <div class="header">
                    <h1>CompanionOS</h1>
                    <div class="status"><span class="dot"></span> ESP: {esp_ip} &middot; {wifi_status}</div>
                </div>

                <!-- WEB REMOTE -->
                <div class="remote">
                    <div class="remote-title">Virtual Remote</div>
                    <div class="btn-row">
                        <button class="btn btn-nav" onclick="press('UP')" id="btn-up" title="Up">&#9650;</button>
                    </div>
                    <div class="btn-row">
                        <button class="btn btn-nav" onmousedown="startPress('LEFT')" onmouseup="endPress('LEFT')" onmouseleave="cancelPress()" ontouchstart="startPress('LEFT')" ontouchend="endPress('LEFT')" id="btn-left" title="Previous Page">&#9664;</button>
                        <button class="btn btn-select" onmousedown="startPress('SELECT')" onmouseup="endPress('SELECT')" onmouseleave="cancelPress()" ontouchstart="startPress('SELECT')" ontouchend="endPress('SELECT')" id="btn-select" title="Select / Action">SEL</button>
                        <button class="btn btn-nav" onmousedown="startPress('RIGHT')" onmouseup="endPress('RIGHT')" onmouseleave="cancelPress()" ontouchstart="startPress('RIGHT')" ontouchend="endPress('RIGHT')" id="btn-right" title="Next Page">&#9654;</button>
                    </div>
                    <div class="btn-row">
                        <button class="btn btn-nav" onclick="press('DOWN')" id="btn-down" title="Down">&#9660;</button>
                    </div>
                    <div class="btn-row-home">
                        <button class="btn btn-home" onclick="press('HOME')" id="btn-home" title="Home / Eyes">&#8962; HOME</button>
                    </div>
                    <div class="feedback" id="feedback"></div>
                </div>

                <!-- QUICK PAGE JUMP -->
                <div class="page-nav">
                    <button class="page-btn" onclick="page(0)">Eyes</button>
                    <button class="page-btn" onclick="page(1)">Spotify</button>
                    <button class="page-btn" onclick="page(2)">Pomo</button>
                    <button class="page-btn" onclick="page(3)">Weather</button>
                    <button class="page-btn" onclick="page(4)">Notifs</button>
                    <button class="page-btn" onclick="page(5)">Notes</button>
                    <button class="page-btn" onclick="page(6)">Stocks</button>
                    <button class="page-btn" onclick="page(7)">Gaming</button>
                    <button class="page-btn" onclick="page(8)">Social</button>
                    <button class="page-btn" onclick="page(9)">Tasks</button>
                    <button class="page-btn" onclick="page(10)">Network</button>
                    <button class="page-btn" onclick="page(11)">Settings</button>
                    <button class="page-btn" onclick="page(12)" style="border-color:#a855f7;color:#a855f7">RuView</button>
                    <button class="page-btn" onclick="page(13)">Dr.Hack</button>
                    <a href="/ruview" style="font-size:11px;color:#a855f7;text-decoration:none;align-self:center;margin-left:8px">📡 Full Dashboard →</a>
                </div>

                <!-- NOTES -->
                <div class="notes">
                    <h2>&#128221; Quick Notes</h2>
                    <form action="/add" method="POST">
                        <textarea name="note" placeholder="Type a note... (first 4 lines show on device)"></textarea>
                        <button type="submit" class="notes-submit">Send to Companion</button>
                    </form>
                    <div class="current-notes">
                        <h3>Current Notes</h3>
                        <pre>{notes}</pre>
                    </div>
                </div>

                <!-- MEMORIES -->
                <div class="notes" style="margin-top: 24px;">
                    <h2>&#128247; Memories</h2>
                    <form id="eyeForm" style="display: flex; gap: 10px; margin-top: 10px;">
                        <input type="file" id="eyeImage" accept="image/*" style="flex: 1; padding: 5px; color: #fff;">
                        <button type="button" class="notes-submit" style="flex: 1; margin-top: 0;" onclick="uploadEye()">Upload Memory</button>
                    </form>
                    <div style="display: flex; gap: 10px; margin-top: 10px;">
                        <button class="notes-submit" style="flex: 1; background: #00d4ff; color: #111;" onclick="toggleEye(1)">Show Memory</button>
                        <button class="notes-submit" style="flex: 1; background: #ff416c; color: #fff;" onclick="toggleEye(0)">Revert to Eyes</button>
                    </div>
                </div>

                <!-- STOCKS SETTINGS -->
                <div class="notes" style="margin-top: 24px;">
                    <h2>&#128200; Stocks Settings</h2>
                    <form action="/api/stock" method="POST" style="display: flex; gap: 10px; margin-top: 10px;">
                        <input type="text" name="ticker" placeholder="Stock Ticker (e.g. AAPL)" style="flex: 1; padding: 10px; border-radius: 8px; border: none; background: #1a1a3e; color: #fff;" required>
                        <button type="submit" class="notes-submit" style="margin-top: 0;">Set Primary Stock</button>
                    </form>
                </div>

                <!-- POMODORO SETTINGS -->
                <div class="notes" style="margin-top: 24px;">
                    <h2>&#9201; Pomodoro Settings</h2>
                    <form action="/api/pomodoro" method="POST" style="display: flex; gap: 10px; margin-top: 10px;">
                        <input type="number" name="work" placeholder="Work (min)" style="flex: 1; padding: 10px; border-radius: 8px; border: none; background: #1a1a3e; color: #fff;" required>
                        <input type="number" name="break" placeholder="Break (min)" style="flex: 1; padding: 10px; border-radius: 8px; border: none; background: #1a1a3e; color: #fff;" required>
                        <button type="submit" class="notes-submit" style="flex: 1; margin-top: 0;">Update Timer</button>
                    </form>
                </div>

                <!-- DR HACK REMOTE -->
                <div class="notes" style="margin-top: 24px;">
                    <h2>&#128128; Dr. Hack Remote Execution</h2>
                    <div style="display: flex; gap: 10px; margin-top: 10px; flex-wrap: wrap;">
                        <button class="notes-submit" style="flex: 1; margin-top: 0; background: linear-gradient(135deg, #ff416c, #ff4b2b); color: white;" onclick="drhack('DEAUTH')">WiFi Deauth</button>
                        <button class="notes-submit" style="flex: 1; margin-top: 0; background: linear-gradient(135deg, #f7b733, #fc4a1a); color: white;" onclick="drhack('SPAM')">Beacon Spam</button>
                        <button class="notes-submit" style="flex: 1; margin-top: 0; background: linear-gradient(135deg, #00b4db, #0083b0); color: white;" onclick="drhack('MONITOR')">Pkt Monitor</button>
                    </div>
                </div>

                <!-- ESP32 PINOUT -->
                <div class="notes" style="margin-top: 24px; margin-bottom: 40px;">
                    <h2>&#128204; ESP32 Hardware Pinout</h2>
                    <div style="display: flex; justify-content: center; margin-top: 16px;">
                        <div style="background: #2a2a2a; border-radius: 8px; border: 2px solid #555; padding: 20px; position: relative; width: 220px; text-align: center;">
                            <div style="background: #111; color: #aaa; padding: 4px; border-radius: 4px; margin-bottom: 20px; font-family: monospace;">ESP32 DEVKIT V1</div>
                            
                            <div style="display: flex; justify-content: space-between; font-family: monospace; font-size: 11px;">
                                <div style="text-align: right; color: #00d4ff; line-height: 1.8;">
                                    <div>3V3 &mdash; VCC</div>
                                    <div>GND &mdash; GND</div>
                                    <div>D13 &mdash; BTN LEFT</div>
                                    <div>D14 &mdash; BTN RIGHT</div>
                                    <div>D27 &mdash; BTN SELECT</div>
                                </div>
                                <div style="text-align: left; color: #7b2ff7; line-height: 1.8;">
                                    <div>TFT SCK &mdash; D18</div>
                                    <div>TFT MOSI &mdash; D23</div>
                                    <div>TFT RES &mdash; D4</div>
                                    <div>TFT DC &mdash; D2</div>
                                    <div>TFT CS &mdash; D5</div>
                                </div>
                            </div>
                            <div style="margin-top: 20px; font-size: 11px; color: #888;">Note: All buttons pull to GND</div>
                        </div>
                    </div>
                </div>

                <script>
                    let pressTimer = null;
                    let pressStart = 0;

                    function startPress(btn) {{
                        pressStart = Date.now();
                        pressTimer = setTimeout(() => {{
                            const fb = document.getElementById('feedback');
                            fb.textContent = btn + ' holding...';
                            fb.className = 'feedback active';
                        }}, 500);
                    }}

                    function endPress(btn) {{
                        if (pressStart === 0) return;
                        clearTimeout(pressTimer);
                        let duration = Date.now() - pressStart;
                        pressStart = 0;
                        if (duration >= 600) {{
                            press(btn + '_LONG');  // firmware expects LEFT_LONG, RIGHT_LONG, SELECT_LONG
                        }} else {{
                            press(btn);
                        }}
                    }}

                    function cancelPress() {{
                        clearTimeout(pressTimer);
                        if (pressStart > 0) {{
                            const fb = document.getElementById('feedback');
                            fb.className = 'feedback';
                            fb.textContent = '';
                        }}
                        pressStart = 0;
                    }}

                    function press(btn) {{
                        const fb = document.getElementById('feedback');
                        fb.textContent = btn + ' pressed';
                        fb.className = 'feedback active';
                        setTimeout(() => {{ fb.className = 'feedback'; fb.textContent = ''; }}, 600);

                        fetch('/api/button', {{
                            method: 'POST',
                            headers: {{ 'Content-Type': 'application/json' }},
                            body: JSON.stringify({{ btn: btn }})
                        }}).catch(e => {{
                            fb.textContent = 'Error: ' + e;
                            fb.className = 'feedback active';
                        }});
                    }}

                    function drhack(action) {{
                        fetch('/api/drhack', {{
                            method: 'POST',
                            headers: {{ 'Content-Type': 'application/json' }},
                            body: JSON.stringify({{ action: action }})
                        }}).then(() => {{
                            const fb = document.getElementById('feedback');
                            fb.textContent = 'Dr.Hack: ' + action;
                            fb.className = 'feedback active';
                            setTimeout(() => {{ fb.className = 'feedback'; fb.textContent = ''; }}, 1000);
                        }});
                    }}

                    function uploadEye() {{
                        const input = document.getElementById('eyeImage');
                        if (!input.files[0]) return;
                        const formData = new FormData();
                        formData.append('image', input.files[0]);
                        
                        const fb = document.getElementById('feedback');
                        fb.textContent = 'Uploading... Please wait (~6s)';
                        fb.className = 'feedback active';
                        
                        fetch('/api/eye_upload', {{
                            method: 'POST',
                            body: formData
                        }}).then(r => r.json()).then(res => {{
                            fb.textContent = res.status;
                            setTimeout(() => {{ fb.className = 'feedback'; fb.textContent = ''; }}, 3000);
                        }}).catch(e => {{
                            fb.textContent = 'Upload Error';
                            setTimeout(() => {{ fb.className = 'feedback'; fb.textContent = ''; }}, 3000);
                        }});
                    }}

                    function toggleEye(state) {{
                        fetch('/api/eye_toggle', {{
                            method: 'POST',
                            headers: {{ 'Content-Type': 'application/json' }},
                            body: JSON.stringify({{ state: state }})
                        }});
                        const fb = document.getElementById('feedback');
                        fb.textContent = state ? 'Showing Memory' : 'Reverting to Eyes';
                        fb.className = 'feedback active';
                        setTimeout(() => {{ fb.className = 'feedback'; fb.textContent = ''; }}, 1500);
                    }}

                    function page(n) {{
                        fetch('/api/page', {{
                            method: 'POST',
                            headers: {{ 'Content-Type': 'application/json' }},
                            body: JSON.stringify({{ page: n }})
                        }});
                        const fb = document.getElementById('feedback');
                        fb.textContent = 'Jumped to page ' + n;
                        fb.className = 'feedback active';
                        setTimeout(() => {{ fb.className = 'feedback'; fb.textContent = ''; }}, 800);
                    }}

                    // Keyboard shortcuts
                    document.addEventListener('keydown', (e) => {{
                        if (e.target.tagName === 'TEXTAREA') return;
                        switch(e.key) {{
                            case 'ArrowLeft':  e.preventDefault(); press('LEFT'); break;
                            case 'ArrowRight': e.preventDefault(); press('RIGHT'); break;
                            case 'Enter':      e.preventDefault(); press('SELECT'); break;
                            case 'Escape':     e.preventDefault(); press('HOME'); break;
                            case 'h':          e.preventDefault(); press('HOME'); break;
                        }}
                    }});
                    
                    // Setup long press handlers for UI buttons
                    window.addEventListener('DOMContentLoaded', () => {{
                        document.querySelectorAll('.btn-nav, .btn-select, .btn-home').forEach(el => {{
                            const onclickAttr = el.getAttribute('onclick');
                            if (onclickAttr && onclickAttr.includes("press('")) {{
                                const btnType = onclickAttr.match(/'(.*?)'/)[1];
                                el.removeAttribute('onclick');
                                el.addEventListener('mousedown', () => startPress(btnType));
                                el.addEventListener('mouseup', () => endPress(btnType));
                                el.addEventListener('mouseleave', cancelPress);
                                el.addEventListener('touchstart', (e) => {{ e.preventDefault(); startPress(btnType); }}, {{passive: false}});
                                el.addEventListener('touchend', (e) => {{ e.preventDefault(); endPress(btnType); }}, {{passive: false}});
                                el.addEventListener('touchcancel', cancelPress);
                            }}
                        }});
                    }});
                </script>
            </body>
            </html>
            """
        
        @app.route('/add', methods=['POST'])
        def add_note():
            note = request.form.get('note', '').strip()
            if note:
                with open(notes_file, 'w') as f:
                    f.write(note)
                print(f"📝 Note updated via web: {note[:50]}...")
            return redirect('/')

        @app.route('/api/stock', methods=['POST'])
        def api_stock():
            ticker = request.form.get('ticker')
            if ticker:
                stock_service.set_primary(ticker)
                # optionally force an immediate update
                try:
                    payload = stock_service.get_esp_payload()
                    msg = f"STOCKS:{json.dumps(payload)}"
                    send_udp(msg)
                except Exception as e:
                    print(f"Error fetching new stock: {e}")
            return redirect('/')

        @app.route('/api/pomodoro', methods=['POST'])
        def api_pomodoro():
            global pomodoro_duration, pomodoro_break_duration, pomodoro_remaining, pomodoro_is_break, pomodoro_active
            try:
                work = int(request.form.get('work', 25))
                brk = int(request.form.get('break', 5))
                pomodoro_duration = work * 60
                pomodoro_break_duration = brk * 60
                if not pomodoro_active:
                    pomodoro_remaining = pomodoro_break_duration if pomodoro_is_break else pomodoro_duration
                print(f"🍅 Pomodoro settings updated: Work {work}m, Break {brk}m")
            except Exception as e:
                print(f"Pomodoro update error: {e}")
            return redirect('/')
        
        # ── V8: Virtual Button Remote ──────────────────────
        @app.route('/api/button', methods=['POST'])
        def api_button():
            """Receives virtual button press and relays to ESP via UDP."""
            data = request.get_json()
            if data and 'btn' in data:
                btn = str(data['btn']).upper().strip()
                valid = {'LEFT', 'RIGHT', 'SELECT', 'HOME', 'UP', 'DOWN',
                         'LEFT_LONG', 'RIGHT_LONG', 'SELECT_LONG', 'HOME_LONG', 'UP_LONG', 'DOWN_LONG'}
                if btn in valid:
                    send_udp(f"BTN:{btn}")
                    print(f"🎮 Virtual button: {btn}")
                    return jsonify({'status': 'ok', 'btn': btn})
                return jsonify({'status': 'error', 'msg': f'Invalid button: {btn}'}), 400
            return jsonify({'status': 'error', 'msg': 'Missing btn field'}), 400

        @app.route('/api/page', methods=['POST'])
        def api_page():
            """Direct page navigation — jumps to specific page index."""
            data = request.get_json()
            if data and 'page' in data:
                page_num = int(data['page'])
                send_udp(f"PAGE:{page_num}")
                print(f"📄 Page jump: {page_num}")
                return jsonify({'status': 'ok', 'page': page_num})
            return jsonify({'status': 'error', 'msg': 'Missing page field'}), 400

        @app.route('/api/drhack', methods=['POST'])
        def api_drhack():
            data = request.get_json()
            if data and 'action' in data:
                action = data['action']
                send_udp(f"DRHACK:{action}")
                return {'status': 'ok'}
            return {'status': 'error'}, 400

        @app.route('/api/notify', methods=['POST'])
        def api_notify():
            """API endpoint for pushing notifications"""
            data = request.get_json()
            if data:
                notif_file = os.path.join(SCRIPT_DIR, "notifications.json")
                existing = []
                if os.path.exists(notif_file):
                    with open(notif_file, 'r') as f:
                        loaded = json.load(f)
                        if isinstance(loaded, list):
                            existing = loaded
                
                existing.append({
                    'app': data.get('app', 'Custom'),
                    'title': data.get('title', 'Notification'),
                    'body': data.get('body', ''),
                    'time': datetime.now().strftime('%H:%M')
                })
                
                with open(notif_file, 'w') as f:
                    json.dump(existing[-20:], f)  # type: ignore
                    
                # Instantly update ESP
                summary = []
                for n in existing[-3:]:
                    summary.append({
                        'app': str(n.get('app', '?'))[:10],
                        'title': str(n.get('title', ''))[:20],
                        'time': str(n.get('time', ''))
                    })
                send_udp(f"NOTIF:{json.dumps(summary)}")
                    
                return {'status': 'ok'}
            return {'status': 'error'}, 400
        
        # V4: Antigravity Agent Status Webhook
        @app.route('/api/agent', methods=['POST'])
        def api_agent():
            """Receives AI agent status and forwards to ESP"""
            data = request.get_json()
            if data:
                status = data.get('status', 'thinking')
                text = data.get('text', '')[:60]
                agent_payload = json.dumps({'status': status, 'text': text})
                send_udp(f"AGENT:{agent_payload}")
                print(f"🤖 Agent: [{status}] {text[:40]}")
                return {'status': 'ok'}
            return {'status': 'error'}, 400
        
        # V4: Gallery Image Push
        @app.route('/api/gallery', methods=['POST'])
        def api_gallery():
            """Receives image upload, processes to 96x96 RGB565, streams to ESP"""
            try:
                from PIL import Image  # type: ignore
                import io
                
                if 'image' not in request.files:
                    return {'status': 'error', 'msg': 'No image file'}, 400
                
                file = request.files['image']
                img = Image.open(file.stream)
                # Resize strictly to 64x64 to match ESP32 ALBUM_ART_W and prevent array corruption
                img = img.resize((64, 64), getattr(Image, 'Resampling', Image).LANCZOS)
                img = img.convert('RGB')
                
                pixels = []
                for y in range(64):
                    for x in range(64):
                        r, g, b = img.getpixel((x, y))  # type: ignore
                        rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
                        pixels.append(rgb565)
                
                send_udp("ART_START:")
                time.sleep(0.05)
                
                pixels_per_chunk = 64 * 4
                flat_bytes = []
                for c in pixels:
                    flat_bytes.append((c >> 8) & 0xFF)
                    flat_bytes.append(c & 0xFF)
                byte_array = bytes(flat_bytes)
                
                raw_bytes = bytearray(byte_array)
                for i in range(0, len(raw_bytes), pixels_per_chunk * 2):
                    chunk_data = raw_bytes[i:i+(pixels_per_chunk*2)]  # type: ignore
                    idx = i // (64 * 2)
                    packet = bytes([0xFE, (idx >> 8) & 0xFF, idx & 0xFF]) + chunk_data
                    send_udp_bytes(packet)
                    time.sleep(0.02)
                
                send_udp("ART_COMPLETE:")
                print(f"🖼️ Gallery image pushed to ESP")
                return {'status': 'ok'}
            except ImportError:
                return {'status': 'error', 'msg': 'PIL not installed'}, 500
            except Exception as e:
                return {'status': 'error', 'msg': str(e)}, 500
        
        @app.route('/api/eye_upload', methods=['POST'])
        def api_eye_upload():
            if 'image' not in request.files:
                return jsonify({"status": "No file uploaded"}), 400
            
            file = request.files['image']
            try:
                from PIL import Image
                import time
                img = Image.open(file.stream)
                img = img.resize((160, 128), getattr(Image, 'Resampling', Image).LANCZOS).convert('RGB')
                
                send_udp("CUSTEYE:START")
                time.sleep(0.5)
                
                for row in range(128):
                    chunk_data = bytearray()
                    chunk_data.append(0xFD)
                    chunk_data.append((row >> 8) & 0xFF)
                    chunk_data.append(row & 0xFF)
                    
                    for col in range(160):
                        r, g, b = img.getpixel((col, row))
                        rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                        chunk_data.append((rgb565 >> 8) & 0xFF)
                        chunk_data.append(rgb565 & 0xFF)
                    
                    send_udp_bytes(bytes(chunk_data))
                    time.sleep(0.05)
                
                time.sleep(0.5)
                send_udp("CUSTEYE:DONE")
                send_udp("CUSTEYE:TOGGLE:1")
                return jsonify({"status": "Success!"})
            except Exception as e:
                print(f"Eye upload error: {e}")
                return jsonify({"status": f"Error: {e}"}), 500

        @app.route('/api/eye_toggle', methods=['POST'])
        def api_eye_toggle():
            data = request.get_json()
            if data and 'state' in data:
                state = 1 if data['state'] else 0
                send_udp(f"CUSTEYE:TOGGLE:{state}")
            return jsonify({"status": "ok"})

        # ── RuView CSI Presence Map ──────────────────────
        @app.route('/ruview')
        def ruview_redirect():
            return redirect('/ruview/')

        @app.route('/ruview/')
        def ruview_page():
            if os.path.isdir(ruview_ui_dir):
                return send_from_directory(ruview_ui_dir, "index.html")
            return RUVIEW_HTML

        @app.route('/ruview/<path:asset_path>')
        def ruview_asset(asset_path):
            if os.path.isdir(ruview_ui_dir):
                return send_from_directory(ruview_ui_dir, asset_path)
            return ("RuView UI assets not found", 404)

        @app.route('/api/ruview/state')
        def api_ruview_state():
            """Returns current zone states as JSON for the presence map."""
            import ruview_processor as rvp
            states = rvp.zone_manager.get_all_states()
            return jsonify({
                'enabled': rvp.ruview_enabled,
                'zones': states
            })

        @app.route('/api/ruview/recalibrate', methods=['POST'])
        def api_ruview_recalibrate():
            """Trigger 60s ambient recalibration for all zones."""
            import ruview_processor as rvp
            rvp.zone_manager.recalibrate_all()
            return jsonify({'status': 'ok', 'msg': 'Recalibration started — leave room empty for 60s'})

        @app.route('/api/ruview/toggle', methods=['POST'])
        def api_ruview_toggle():
            """Enable/disable CSI processing."""
            import ruview_processor as rvp
            data = request.get_json() or {}
            if 'enabled' in data:
                rvp.ruview_enabled = bool(data['enabled'])
            else:
                rvp.ruview_enabled = not rvp.ruview_enabled
            return jsonify({'status': 'ok', 'enabled': rvp.ruview_enabled})

        @app.route('/api/ruview/rename', methods=['POST'])
        def api_ruview_rename():
            """Rename a zone label."""
            import ruview_processor as rvp
            data = request.get_json() or {}
            node_id = data.get('node_id', 0)
            label = data.get('label', 'Room')[:15]
            rvp.zone_manager.set_zone_label(node_id, label)
            return jsonify({'status': 'ok'})
        
        print(f"🌐 CompanionOS Web Remote starting on http://0.0.0.0:{port}")
        print(f"  🎮 Virtual Remote: http://localhost:{port}")
        print(f"  🤖 Agent webhook: POST /api/agent")
        print(f"  🖼️ Gallery push:  POST /api/gallery")
        print(f"  🔘 Button API:    POST /api/button")
        print(f"  📡 RuView CSI:    http://localhost:{port}/ruview")
        app.run(host='0.0.0.0', port=port, debug=False, use_reloader=False)
    except ImportError:
        print("⚠️ Flask not installed. Run: pip install flask")
    except Exception as e:
        print(f"Notes server error: {e}")


def command_listener():
    global active_esp_ip, force_resync, pomodoro_active, pomodoro_remaining
    global pomodoro_is_break, pomodoro_sessions, pomodoro_last_tick
    global fast_poll_now
    
    listen_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    listen_sock.bind(('0.0.0.0', PC_PORT_RX))
    print(f"Listening on port {PC_PORT_RX}...")
    
    while True:
        try:
            data, addr = listen_sock.recvfrom(1024)
            active_esp_ip = addr[0]
            command = data.decode().strip()
            
            msg = data.decode('utf-8').strip()
            
            if msg == "HELLO_COMPANION":
                print(f"👋 Companion Device Discovered at {active_esp_ip}!")
                force_resync = True
            elif msg == "RESYNC":
                print(f"🔄 Resync requested by device at {active_esp_ip}!")
                force_resync = True
            else:
                print(f"← [{addr[0]}] {msg}")
                if msg.startswith("PLAY_PAUSE"):
                    spotify_service.control_playback("PLAY_PAUSE")
                    fast_poll_now = True
                elif msg.startswith("NEXT"):
                    spotify_service.control_playback("NEXT")
                    fast_poll_now = True
                elif msg.startswith("PREV"):
                    spotify_service.control_playback("PREV")
                    fast_poll_now = True
                elif msg.startswith("VOLUME:") or msg.startswith("SEEK:"):
                    spotify_service.control_playback(msg)
                elif msg == "SHUFFLE:TOGGLE":
                    try:
                        state = spotify_service.client.current_playback()
                        if state:
                            spotify_service.client.shuffle(not state['shuffle_state'])
                            print(f"🔀 Shuffle {'ON' if not state['shuffle_state'] else 'OFF'}")
                    except Exception as e:
                        print(f"Shuffle error: {e}")
                elif msg == "REPEAT:TOGGLE":
                    try:
                        state = spotify_service.client.current_playback()
                        if state:
                            states = ["off", "context", "track"]
                            idx = (states.index(state['repeat_state']) + 1) % 3
                            spotify_service.client.repeat(states[idx])
                            print(f"🔁 Repeat {states[idx].upper()}")
                    except Exception as e:
                        print(f"Repeat error: {e}")
                elif msg == "LIKE:TOGGLE":
                    try:
                        track = spotify_service.get_current_track()
                        if track and track.get('id'):
                            is_saved = spotify_service.client.current_user_saved_tracks_contains([track['id']])[0]
                            if is_saved:
                                spotify_service.client.current_user_saved_tracks_delete([track['id']])
                                print(f"💔 Unsaved {track['name']}")
                            else:
                                spotify_service.client.current_user_saved_tracks_add([track['id']])
                                print(f"❤️ Saved {track['name']}")
                    except Exception as e:
                        print(f"Like toggle error: {e}")
                elif msg == "POMO:START":
                    pomodoro_active = True
                    pomodoro_last_tick = time.time()
                    if not pomodoro_is_break: # If starting work, reset remaining
                        pomodoro_remaining = pomodoro_duration
                    else: # If starting break, reset remaining
                        pomodoro_remaining = pomodoro_break_duration
                    print("🍅 Pomodoro started!")
                elif msg == "POMO:PAUSE":
                    pomodoro_active = False
                    print("🍅 Pomodoro paused")
                elif msg == "POMO:SKIP":
                    if pomodoro_is_break:
                        pomodoro_is_break = False
                        pomodoro_remaining = pomodoro_duration
                        pomodoro_last_tick = time.time()
                    else:
                        pomodoro_is_break = True
                        pomodoro_remaining = pomodoro_break_duration
                        pomodoro_last_tick = time.time()
                        pomodoro_sessions += 1  # type: ignore
                    print(f"🍅 Skipped to {'break' if pomodoro_is_break else 'work'}")
                elif msg == "NOTIF:CLEAR":
                    notifications.clear()
                    notif_file = os.path.join(SCRIPT_DIR, "notifications.json")
                    with open(notif_file, 'w') as f:
                        json.dump([], f)
                    send_udp('NOTIF:[]')  # type: ignore
                    print("🔔 Notifications cleared")
                # ── V6: New page commands ──────────────────
                elif msg.startswith("STOCK:SET:"):
                    new_ticker = msg.split(":")[2].strip()
                    if new_ticker:
                        stock_service.set_primary(new_ticker)
                        print(f"📊 Primary ticker → {new_ticker}")
                elif msg == "SOCIAL:LIKE":
                    print("❤️ Social like from device")
                elif msg == "TASK:DONE":
                    print("✅ Task marked done from device")
        except Exception as e:
            print(f"Command error: {e}")


# ═══════════════════════════════════════════════════════════
# V6: DATA FEED LOOPS (run as daemon threads)
# ═══════════════════════════════════════════════════════════

def stock_feed_loop():
    """Periodically fetch stock data and send to ESP."""
    interval = config.get('stocks', {}).get('update_interval_seconds', 300)
    # Initial delay to let network settle
    time.sleep(8)
    while True:
        try:
            payload = stock_service.get_esp_payload()
            msg = f"STOCKS:{json.dumps(payload)}"
            send_udp(msg)
            print(f"📊 Stock sync: {payload.get('symbol', '?')} {payload.get('price', '?')}")
        except Exception as e:
            print(f"Stock feed error: {e}")
        time.sleep(interval)


def push_game_cover(title):
    """Fetch a game cover URL for cache warmup without touching Spotify art packets."""
    try:
        url = steam_service.get_game_cover_url(title)
        if not url: return
        print(f"🖼️ Game cover found for {title}; not streamed over Spotify art channel")
    except Exception as e:
        print(f"Game cover push error: {e}")

def gaming_feed_loop():
    """Periodically check game state and send to ESP."""
    interval = config.get('gaming', {}).get('update_interval_seconds', 30)
    time.sleep(5)
    last_pushed_title = ""
    while True:
        try:
            state = steam_service.get_gaming_state()
            msg = f"GAMING:{json.dumps(state)}"
            send_udp(msg)
            if state.get('active'):
                print(f"🎮 Playing: {state['title']} ({state['session']})")
                if state['title'] != last_pushed_title:
                    push_game_cover(state['title'])
                    last_pushed_title = state['title']
            else:
                last_pushed_title = ""
        except Exception as e:
            print(f"Gaming feed error: {e}")
        time.sleep(interval)


def social_feed_loop():
    """Reformat Windows notifications as social cards for ESP."""
    interval = config.get('social', {}).get('update_interval_seconds', 15)
    last_social_body = ''
    time.sleep(6)
    while True:
        try:
            # Read from the notification storage that capture_windows_notifications populates
            notif_file = os.path.join(SCRIPT_DIR, "notifications.json")
            if os.path.exists(notif_file):
                with open(notif_file, 'r') as f:
                    notifs = json.load(f)
                if notifs and isinstance(notifs, list):
                    # Pick the most recent notification
                    latest = notifs[-1]
                    app_name = str(latest.get('app', 'App'))[:11]
                    title = str(latest.get('title', ''))[:79]
                    time_str = str(latest.get('time', ''))[:7]

                    # Extract username from title (heuristic: first word before ':' or space)
                    user = ''
                    if ':' in title:
                        user = title.split(':')[0].strip()[:15]
                        body = title[title.index(':') + 1:].strip()
                    else:
                        parts = title.split(' ', 1)
                        user = parts[0][:15] if parts else app_name
                        body = parts[1] if len(parts) > 1 else title

                    if body != last_social_body:
                        social_payload = {
                            'user': user,
                            'app': app_name,
                            'body': body[:79],
                            'time': time_str,
                            'likes': 0,
                            'comments': 0
                        }
                        send_udp(f"SOCIAL:{json.dumps(social_payload)}")
                        last_social_body = body
        except Exception as e:
            print(f"Social feed error: {e}")
        time.sleep(interval)


def productivity_feed_loop():
    """Periodically fetch calendar/tasks and send to ESP."""
    interval = config.get('productivity', {}).get('update_interval_seconds', 60)
    time.sleep(7)
    while True:
        try:
            state = productivity_service.get_productivity_state()
            msg = f"TASKS:{json.dumps(state)}"
            send_udp(msg)
            if state.get('current'):
                print(f"📅 Task: {state['current']}")
        except Exception as e:
            print(f"Productivity feed error: {e}")
        time.sleep(interval)

# ═══════════════════════════════════════════════════════════
# V7: THOUGHT PUSH — PC→ESP override thought injection
# 🟠 CRIT-06 FIX: THOUGHT: is OUTGOING (PC→ESP), not incoming.
# This function generates context-aware thoughts and pushes
# them to override the ESP's next scheduled thought slot.
# ═══════════════════════════════════════════════════════════

import random as _random

_THOUGHT_CONTEXT_TEMPLATES = {
    'track_change': [
        "omg {artist} is goated",
        "{track} on repeat all day",
        "new {artist} just dropped? vibing",
        "this {track} hits different rn",
        "{artist} never misses fr",
    ],
    'weather_hot': [
        "it's {temp}°C... melting rn",
        "AC cranked to max",
        "too hot to think straight",
    ],
    'weather_cold': [
        "it's {temp}°C... need more layers",
        "blanket burrito time",
        "winter is here. literally.",
    ],
    'weather_rain': [
        "rain + lofi = perfect combo",
        "don't forget your umbrella!",
        "rainy vibes activated",
    ],
    'hourly': [
        "still going strong at {hour}",
        "time check: {hour}:00",
        "another hour, another vibe",
    ]
}

_last_thought_track = None
_last_thought_time = 0


def push_thought_to_esp(context: str, **kwargs):
    """Generate a context-aware thought and send THOUGHT: to ESP."""
    templates = _THOUGHT_CONTEXT_TEMPLATES.get(context, _THOUGHT_CONTEXT_TEMPLATES['hourly'])
    template = _random.choice(templates)
    try:
        thought = template.format(**kwargs)
    except (KeyError, IndexError):
        thought = template  # Use raw template if format fails
    
    if len(thought) > 79:
        thought = thought[:76] + "..."
    
    send_udp(f"THOUGHT:{thought}")
    print(f"💭 Pushed thought: {thought}")


def thought_push_loop():
    """Background thread: push context thoughts on events."""
    global _last_thought_track, _last_thought_time
    time.sleep(30)  # Wait for initial setup
    
    while True:
        try:
            now = time.time()
            
            # Check for track change → push music thought
            try:
                track = spotify_service.get_current_track()
                if track and track.get('id') != _last_thought_track:
                    _last_thought_track = track.get('id')
                    push_thought_to_esp('track_change',
                                       track=track.get('name', 'this song')[:20],
                                       artist=track.get('artist', 'them')[:15])
                    _last_thought_time = now
            except Exception:
                pass
            
            # Hourly thought (if no thought sent in the last 55 minutes)
            if now - _last_thought_time > 3300:
                hour = datetime.now().hour
                uptime_h = int((now - _last_thought_time) / 3600) + 1
                push_thought_to_esp('hourly', hour=hour, uptime=uptime_h)
                _last_thought_time = now
            
            time.sleep(60)  # Check every minute
            
        except Exception as e:
            print(f"Thought push error: {e}")
            time.sleep(120)


def main():
    print("\n╔════════════════════════════════════════════════╗")
    print("║  COMPANION OS - Controller v7.0               ║")
    print("╚════════════════════════════════════════════════╝\n")
    
    # Start background threads
    threading.Thread(target=command_listener, daemon=True).start()
    threading.Thread(target=capture_windows_notifications, daemon=True).start()
    threading.Thread(target=start_notes_server, daemon=True).start()
    # V6: New data feed threads
    threading.Thread(target=stock_feed_loop, daemon=True).start()
    threading.Thread(target=gaming_feed_loop, daemon=True).start()
    threading.Thread(target=social_feed_loop, daemon=True).start()
    threading.Thread(target=productivity_feed_loop, daemon=True).start()
    # V7: Thought push thread (PC→ESP override thoughts)
    threading.Thread(target=thought_push_loop, daemon=True).start()
    # Theme 2: Extended Spotify data bridge (reuses same spotify_service + API keys)
    threading.Thread(target=theme2_spotify_feed, args=(spotify_service, send_udp, config), daemon=True).start()
    # V8: RuView CSI Presence Detection threads
    threading.Thread(target=ruview_listener_loop, args=(8890,), daemon=True).start()
    threading.Thread(target=ruview_push_loop, args=(send_udp, 2.0), daemon=True).start()
    print("Monitoring playback & connections...")
    
    global current_lyrics, fast_poll_now
    fast_poll_now = False
    current_track_id = None
    last_sent_lyrics = None
    last_github_check: float = 0.0
    last_notes_check: float = 0.0
    last_notes_content = []
    last_time_sync: float = 0.0
    last_weather_check: float = 0.0
    last_pomodoro_sync: float = 0.0
    
    notes_file = os.path.join(SCRIPT_DIR, "notes.txt")
    if not os.path.exists(notes_file):
        with open(notes_file, "w") as f:
            f.write("Welcome to CompanionOS!\nEdit notes at http://YOUR_PC_IP:5555\n")
    
    global force_resync, pomodoro_active, pomodoro_remaining
    global pomodoro_is_break, pomodoro_sessions, pomodoro_last_tick
    
    while True:
        try:
            now = time.time()
            
            if force_resync or fast_poll_now:
                fast_poll_now = False
                
            if force_resync:
                current_track_id = None
                last_sent_lyrics = None
                last_github_check = 0.0
                last_notes_check = 0.0
                last_time_sync = 0.0
                last_weather_check = 0.0
                force_resync = False
                print("🔄 Triggered full state resync!")
            
            # --- Time Sync (every 60s) ---
            if now - last_time_sync > 60 or last_time_sync == 0:
                last_time_sync = now
                dt = datetime.now()
                # ESP expects {"time": "HH:MM"}
                time_str = f"{dt.hour:02d}:{dt.minute:02d}"
                send_udp(f'TIME:{{"time":"{time_str}"}}')
            
            # --- Weather Sync ---
            if now - last_weather_check > WEATHER_REFRESH or last_weather_check == 0:
                last_weather_check = now
                weather = fetch_weather()
                if weather:
                    send_udp(f"WEATHER:{json.dumps(weather)}")
                    print(f"🌤️ Weather: {weather['temp']}°C {weather['condition']} in {weather['city']}")
            
            # --- Pomodoro Timer ---
            if pomodoro_active:
                if now - last_pomodoro_sync > 1:
                    last_pomodoro_sync = now
                    # Deduct time properly
                    pomodoro_remaining -= (now - pomodoro_last_tick)  # type: ignore
                    pomodoro_last_tick = now
                    
                    if pomodoro_remaining <= 0:
                        # Auto-switch
                        if not pomodoro_is_break:
                            pomodoro_sessions += 1  # type: ignore
                            pomodoro_is_break = True
                            print(f"🍅 Work session #{pomodoro_sessions} complete! Break time!")
                            send_udp('EMOTION:HAPPY')
                        else:
                            pomodoro_is_break = False
                            print("🍅 Break over! Back to work!")
                            send_udp('EMOTION:NEUTRAL')
                        # Reset remaining time to the full duration of the new phase
                        pomodoro_remaining = pomodoro_break_duration if pomodoro_is_break else pomodoro_duration
                    
                    pomo_data = {
                        'remaining': int(max(0, pomodoro_remaining)),
                        'total': pomodoro_break_duration if pomodoro_is_break else pomodoro_duration,
                        'is_break': pomodoro_is_break,
                        'sessions': pomodoro_sessions,
                        'active': pomodoro_active
                    }
                    send_udp(f"POMO:{json.dumps(pomo_data)}")
            else:
                # Keep last tick updated so when we unpause, it doesn't instantly subtract hours of paused time
                pomodoro_last_tick = time.time()
                
                # Still sync state occasionally when paused so UI is responsive
                if now - last_pomodoro_sync > 1:
                    last_pomodoro_sync = now
                    pomo_data = {
                        'remaining': int(pomodoro_remaining),
                        'total': pomodoro_break_duration if pomodoro_is_break else pomodoro_duration,
                        'is_break': pomodoro_is_break,
                        'sessions': pomodoro_sessions,
                        'active': pomodoro_active
                    }
                    send_udp(f"POMO:{json.dumps(pomo_data)}")
            
            # --- Notes Sync ---
            if now - last_notes_check > 5:
                last_notes_check = now
                try:
                    with open(notes_file, "r") as f:
                        lines_raw = f.readlines()
                        lines = []
                        for l in lines_raw:
                            if l.strip():
                                lines.append(l.strip())
                                if len(lines) == 4:
                                    break
                        if lines != last_notes_content:
                            last_notes_content = lines
                            send_udp(f"NOTES:{json.dumps(lines)}")
                            print("📝 Synced Notes to Device")
                except Exception:
                    pass
            
            # --- GitHub Stats ---
            if github_service.enabled and (now - last_github_check > GITHUB_REFRESH or last_github_check == 0):
                last_github_check = now
                profile = github_service.get_profile()
                if profile:
                    send_udp(f"GITHUB:{json.dumps(profile)}")
                    print(f"📊 Sent GitHub stats for {profile['username']}")
            
            # --- Spotify Playback ---
            if spotify_service.enabled:
                track = spotify_service.get_current_track()
                if track and track['id'] != current_track_id:
                    print(f"\n🎵 {track['name']} - {track['artist']}")
                    current_track_id = track['id']
                    
                    info = {
                        'track': track['name'],
                        'artist': track['artist'],
                        'album': track['album'],
                        'duration': track['duration_ms']
                    }
                    send_udp(f"TRACK:{json.dumps(info)}")
                    

                    # Fire-and-forget background thread for slow LRCLib and Image CDN fetches!
                    # This prevents the 20-30 second delay where the progress bar freezes waiting for downloads.
                    current_lyrics = []  # Instantly wipe old lyrics from screen during transition
                    asset_generation = _next_asset_generation()
                    threading.Thread(
                        target=fetch_heavy_assets, 
                        args=(track['name'], track['artist'], track['id'], track['album_art_url'], asset_generation),
                        daemon=True
                    ).start()
                
                if track:
                    state = {'playing': track['is_playing'], 'progress': track['progress_ms']}
                    send_udp(f"STATE:{json.dumps(state)}")
                    
                    if spotify_service.lyrics_enabled and current_lyrics:
                        lines = current_lyrics # Renamed for clarity
                        
                        # Find the current lyric line
                        matching_indices = [i for i, line in enumerate(lines) if line['time'] <= track['progress_ms'] + 500]
                        
                        if matching_indices:
                            current_idx = matching_indices[-1]
                            prev_lyric = lines[current_idx - 1]['words'] if current_idx > 0 else ""
                            current_lyric = lines[current_idx]['words']
                            next_lyric = lines[current_idx + 1]['words'] if current_idx + 1 < len(lines) else ""
                            
                            # Only send if the set of 3 lines has changed
                            current_three_lines = [prev_lyric, current_lyric, next_lyric]
                            if current_three_lines != last_sent_lyrics:
                                last_sent_lyrics = current_three_lines
                                send_udp(f"LYRICS:{json.dumps(current_three_lines)}")
                        elif not lines: # No lyrics at all
                            if last_sent_lyrics != ["♪ Instrumental ♪", "", ""]:
                                last_sent_lyrics = ["♪ Instrumental ♪", "", ""]
                                send_udp(f"LYRICS:{json.dumps(last_sent_lyrics)}")
                        else: # Lyrics exist but no line is active yet (e.g., intro)
                            if last_sent_lyrics != ["", lines[0]['words'], lines[1]['words'] if len(lines) > 1 else ""]:
                                last_sent_lyrics = ["", lines[0]['words'], lines[1]['words'] if len(lines) > 1 else ""]
                                send_udp(f"LYRICS:{json.dumps(last_sent_lyrics)}")
            
            time.sleep(SPOTIFY_POLL)
            
        except KeyboardInterrupt:
            print("\nShutting down...")
            break
        except Exception as e:
            print(f"Error: {e}")
            time.sleep(5)

if __name__ == "__main__":
    main()
