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

def fetch_heavy_assets(track_name, artist_name, track_id, album_art_url):
    """Background thread to process lyrics and album art without blocking UDP STATE updates."""
    global current_lyrics
    
    # 1. Fetch Lyrics (Slow HTTP Call to LRCLib)
    if spotify_service.lyrics_enabled:
        current_lyrics = spotify_service.get_lyrics(track_name, artist_name, track_id)
    else:
        current_lyrics = []
        
    # 2. Fetch and Process Album Art (Slow HTTP Image CDN + Resize Array Math)
    if album_art_url:
        try:
            album_size = 96
            pixels = spotify_service.process_album_art(album_art_url, size=album_size)
            if not pixels: return
            
            send_udp("ART_START:")
            time.sleep(0.05)   # Allow ESP memory clear margin
            
            pixels_per_chunk = album_size * 2 # EXACTLY 2 full visual rows of pixels
            
            # Convert normal RGB565 to raw byte array
            flat_bytes = []
            for c in pixels:
                flat_bytes.append((c >> 8) & 0xFF) # High byte
                flat_bytes.append(c & 0xFF)        # Low byte
            byte_array = bytes(flat_bytes)
            
            for i in range(0, len(byte_array), pixels_per_chunk * 2):
                chunk_data = byte_array[i:i+(pixels_per_chunk*2)]  # type: ignore
                idx = i // (pixels_per_chunk * 2)
                
                # Custom Binary Packet Header: 0xFE identifies raw binary over UDP
                packet = bytes([0xFE, (idx >> 8) & 0xFF, idx & 0xFF]) + chunk_data
                send_udp_bytes(packet)
                time.sleep(0.005)  
                
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
                        notifications.clear()
                        # Use cast and type ignore to satisfy picky linter for slicing dynamic JSON
                        notifications.extend(cast(List[Dict[str, Any]], notifs[-10:]))  # type: ignore
                        
                        # Write to file for persistence
                        with open(notif_file, 'w') as f:
                            json.dump(list(notifications), f)
                        
                        # Send to ESP
                        summary = []
                        # Cast and type ignore for slicing with static analysis
                        for n in list(notifications)[:3]:  # type: ignore
                            summary.append({
                                'app': str(n.get('app', '?'))[:10],  # type: ignore
                                'title': str(n.get('title', ''))[:20],  # type: ignore
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


def start_notes_server():
    """Start Flask web server for Quick Notes"""
    try:
        from flask import Flask, request, redirect  # type: ignore
        app = Flask(__name__)
        notes_file = os.path.join(SCRIPT_DIR, "notes.txt")
        port = config['features'].get('notes_web_port', 5555)
        
        @app.route('/')
        def index():
            notes = ""
            if os.path.exists(notes_file):
                with open(notes_file, 'r') as f:
                    notes = f.read()
            return f"""
            <html>
            <head><title>CompanionOS Notes</title>
            <style>
                body {{ font-family: 'Segoe UI', sans-serif; background: #1a1a2e; color: #eee; 
                       display: flex; justify-content: center; padding: 40px; }}
                .container {{ max-width: 500px; width: 100%; }}
                h1 {{ color: #00d4ff; }}
                textarea {{ width: 100%; height: 150px; background: #16213e; color: #eee; 
                           border: 1px solid #0f3460; border-radius: 8px; padding: 12px; 
                           font-size: 16px; resize: vertical; }}
                button {{ background: #00d4ff; color: #000; border: none; padding: 12px 24px;
                         border-radius: 8px; font-size: 16px; cursor: pointer; margin-top: 12px; }}
                button:hover {{ background: #00b8d4; }}
                .current {{ background: #16213e; padding: 16px; border-radius: 8px; margin-top: 20px; }}
                .current pre {{ color: #a8d8ea; white-space: pre-wrap; }}
            </style></head>
            <body><div class="container">
                <h1>📝 CompanionOS Quick Notes</h1>
                <form action="/add" method="POST">
                    <textarea name="note" placeholder="Type a note... (first 4 lines show on device)"></textarea>
                    <button type="submit">Send to Companion</button>
                </form>
                <div class="current">
                    <h3>Current Notes:</h3>
                    <pre>{notes}</pre>
                </div>
            </div></body></html>
            """
        
        @app.route('/add', methods=['POST'])
        def add_note():
            note = request.form.get('note', '').strip()
            if note:
                with open(notes_file, 'w') as f:
                    f.write(note)
                print(f"📝 Note updated via web: {note[:50]}...")
            return redirect('/')
        
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
                
                # Append and safe dump
                existing.append({
                    'app': data.get('app', 'Custom'),
                    'title': data.get('title', 'Notification'),
                    'body': data.get('body', ''),
                    'time': datetime.now().strftime('%H:%M')
                })
                
                with open(notif_file, 'w') as f:
                    json.dump(existing[-20:], f)  # type: ignore
                    
                return {'status': 'ok'}
            return {'status': 'error'}, 400
        
        # V4: Antigravity Agent Status Webhook
        @app.route('/api/agent', methods=['POST'])
        def api_agent():
            """Receives AI agent status and forwards to ESP"""
            data = request.get_json()
            if data:
                status = data.get('status', 'thinking')  # thinking, done, error
                text = data.get('text', '')[:60]  # Truncate for ESP display
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
                img = Image.open(io.BytesIO(file.read()))
                img = img.resize((96, 96), Image.LANCZOS)
                img = img.convert('RGB')
                
                pixels = []
                for y in range(96):
                    for x in range(96):
                        r, g, b = img.getpixel((x, y))
                        rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
                        pixels.append(rgb565)
                
                # Stream using same binary protocol as album art  
                send_udp("ART_START:")
                time.sleep(0.05)
                
                pixels_per_chunk = 96 * 2
                flat_bytes = []
                for c in pixels:
                    flat_bytes.append((c >> 8) & 0xFF)
                    flat_bytes.append(c & 0xFF)
                byte_array = bytes(flat_bytes)
                
                # Use bytearray and type ignore for slicing in static analysis
                raw_bytes = bytearray(byte_array)
                for i in range(0, len(raw_bytes), pixels_per_chunk * 2):
                    chunk_data = raw_bytes[i:i+(pixels_per_chunk*2)]  # type: ignore
                    idx = i // (pixels_per_chunk * 2)
                    packet = bytes([0xFE, (idx >> 8) & 0xFF, idx & 0xFF]) + chunk_data
                    send_udp_bytes(packet)
                    time.sleep(0.005)
                
                send_udp("ART_COMPLETE:")
                print(f"🖼️ Gallery image pushed to ESP")
                return {'status': 'ok'}
            except ImportError:
                return {'status': 'error', 'msg': 'PIL not installed'}, 500
            except Exception as e:
                return {'status': 'error', 'msg': str(e)}, 500
        
        print(f"📝 Notes web server starting on http://0.0.0.0:{port}")
        print(f"  🤖 Agent webhook: POST /api/agent")
        print(f"  🖼️ Gallery push:  POST /api/gallery")
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
    interval = config.get('stocks', {}).get('update_interval_seconds', 60)
    # Initial delay to let network settle
    time.sleep(8)
    while True:
        try:
            payload = stock_service.get_esp_payload()
            msg = f"STOCKS:{json.dumps(payload)}"
            send_udp(msg)
            print(f"📊 Stock sync: {payload.get('symbol', '?')} ${payload.get('price', '?')}")
        except Exception as e:
            print(f"Stock feed error: {e}")
        time.sleep(interval)


def gaming_feed_loop():
    """Periodically check game state and send to ESP."""
    interval = config.get('gaming', {}).get('update_interval_seconds', 30)
    time.sleep(5)
    while True:
        try:
            state = steam_service.get_gaming_state()
            msg = f"GAMING:{json.dumps(state)}"
            send_udp(msg)
            if state.get('active'):
                print(f"🎮 Playing: {state['title']} ({state['session']})")
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


def main():
    print("\n╔════════════════════════════════════════════════╗")
    print("║  COMPANION OS - Controller v6.0               ║")
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
                    threading.Thread(
                        target=fetch_heavy_assets, 
                        args=(track['name'], track['artist'], track['id'], track['album_art_url']),
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
