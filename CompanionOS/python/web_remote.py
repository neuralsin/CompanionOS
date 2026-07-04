"""
═══════════════════════════════════════════════════════════
COMPANION OS v7.0 — WEB REMOTE CONTROL
Flask server serving a virtual button interface for
navigating CompanionOS without a touchscreen.

Sends UDP commands to the ESP device simulating physical
button presses (LEFT, RIGHT, SELECT) plus direct page
navigation and thought injection.

Usage:
    python web_remote.py

Then open http://localhost:5000 in your browser or phone.
═══════════════════════════════════════════════════════════
"""

import socket
import json
import time
import threading
from flask import Flask, render_template, request, jsonify

app = Flask(__name__)

# ═══════════════════════════════════════════════════════════
# CONFIG
# ═══════════════════════════════════════════════════════════

ESP_IP = "255.255.255.255"  # Broadcast (auto-discover)
ESP_PORT = 8888
LOCAL_PORT = 8889

# State tracking (updated by listener thread)
current_state = {
    "page": 0,
    "page_name": "Eyes",
    "emotion": "NEUTRAL",
    "wifi": False,
    "bt": False,
    "track": "",
    "artist": "",
    "uptime": 0
}

PAGE_NAMES = [
    "Eyes", "Spotify", "Pomodoro", "Weather",
    "Notifications", "Notes", "Stocks", "Gaming",
    "Social", "Productivity", "Network", "Settings",
    "Dr.Hack"
]

# ═══════════════════════════════════════════════════════════
# UDP TRANSPORT
# ═══════════════════════════════════════════════════════════

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
sock.settimeout(0.1)

discovered_ip = None


def send_to_esp(message: str):
    """Send a UDP message to the ESP device."""
    global discovered_ip
    target = discovered_ip if discovered_ip else ESP_IP
    try:
        sock.sendto(message.encode(), (target, ESP_PORT))
    except Exception as e:
        print(f"[UDP] Send error: {e}")


def discover_esp():
    """Listen for HELLO_COMPANION broadcasts to auto-discover ESP IP."""
    global discovered_ip
    listen_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    listen_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listen_sock.bind(("", LOCAL_PORT))
    listen_sock.settimeout(1.0)
    
    print("[Discovery] Listening for CompanionOS broadcasts on port", LOCAL_PORT)
    
    while True:
        try:
            data, addr = listen_sock.recvfrom(2048)
            msg = data.decode("utf-8", errors="ignore").strip()
            
            if msg == "HELLO_COMPANION":
                if discovered_ip != addr[0]:
                    discovered_ip = addr[0]
                    print(f"[Discovery] Found CompanionOS at {discovered_ip}")
                    
        except socket.timeout:
            pass
        except Exception as e:
            print(f"[Discovery] Error: {e}")
            time.sleep(1)


# ═══════════════════════════════════════════════════════════
# FLASK ROUTES
# ═══════════════════════════════════════════════════════════

@app.route("/")
def index():
    return render_template("web_remote.html")


@app.route("/api/btn/<button>", methods=["POST"])
def press_button(button):
    """Simulate a physical button press."""
    valid = ["LEFT", "RIGHT", "SELECT", "HOME"]
    if button.upper() in valid:
        send_to_esp(f"BTN:{button.upper()}")
        return jsonify({"ok": True, "action": button.upper()})
    return jsonify({"ok": False, "error": "invalid button"}), 400


@app.route("/api/page/<int:page_id>", methods=["POST"])
def go_to_page(page_id):
    """Navigate directly to a specific page."""
    if 0 <= page_id < len(PAGE_NAMES):
        send_to_esp(f"PAGE:{page_id}")
        current_state["page"] = page_id
        current_state["page_name"] = PAGE_NAMES[page_id]
        return jsonify({"ok": True, "page": PAGE_NAMES[page_id]})
    return jsonify({"ok": False, "error": "invalid page"}), 400


@app.route("/api/emotion/<emotion>", methods=["POST"])
def set_emotion(emotion):
    """Set the pet's emotion."""
    valid = ["NEUTRAL", "HAPPY", "SAD", "EXCITED", "LOVE", "SLEEPY", "ANGRY", "SURPRISED"]
    if emotion.upper() in valid:
        send_to_esp(f"EMOTION:{emotion.upper()}")
        current_state["emotion"] = emotion.upper()
        return jsonify({"ok": True, "emotion": emotion.upper()})
    return jsonify({"ok": False, "error": "invalid emotion"}), 400


@app.route("/api/thought", methods=["POST"])
def send_thought():
    """Push a custom thought bubble to the ESP."""
    data = request.get_json(silent=True) or {}
    text = data.get("text", "").strip()
    if text and len(text) < 80:
        send_to_esp(f"THOUGHT:{text}")
        return jsonify({"ok": True, "thought": text})
    return jsonify({"ok": False, "error": "text required (max 79 chars)"}), 400


@app.route("/api/command", methods=["POST"])
def send_raw_command():
    """Send a raw command string to the ESP."""
    data = request.get_json(silent=True) or {}
    cmd = data.get("cmd", "").strip()
    if cmd:
        send_to_esp(cmd)
        return jsonify({"ok": True, "cmd": cmd})
    return jsonify({"ok": False, "error": "cmd required"}), 400


@app.route("/api/nrf_check", methods=["POST"])
def nrf_check():
    """Trigger an nRF hardware check on the ESP."""
    send_to_esp("CMD:NRF_CHECK")
    return jsonify({"ok": True, "action": "NRF_CHECK"})


@app.route("/api/status", methods=["GET"])
def get_status():
    """Return current device state."""
    current_state["esp_ip"] = discovered_ip or "searching..."
    return jsonify(current_state)


@app.route("/api/spotify/<action>", methods=["POST"])
def spotify_control(action):
    """Spotify media controls."""
    valid = ["PLAY_PAUSE", "NEXT", "PREV", "SHUFFLE:TOGGLE", "REPEAT:TOGGLE"]
    if action.upper() in valid:
        send_to_esp(action.upper())
        return jsonify({"ok": True, "action": action.upper()})
    return jsonify({"ok": False, "error": "invalid action"}), 400


# 🟡 GAP-03 FIX: Server-Sent Events for real-time status
# Browser can use EventSource('/api/events') for push updates.
from flask import Response

@app.route("/api/events")
def sse_stream():
    """Server-Sent Events stream for real-time status updates."""
    def generate():
        while True:
            current_state["esp_ip"] = discovered_ip or "searching..."
            yield f"data: {json.dumps(current_state)}\n\n"
            time.sleep(1)
    return Response(generate(), mimetype='text/event-stream',
                    headers={'Cache-Control': 'no-cache', 'X-Accel-Buffering': 'no'})


# ═══════════════════════════════════════════════════════════
# MAIN
# ═══════════════════════════════════════════════════════════

if __name__ == "__main__":
    # Start ESP discovery in background
    discovery_thread = threading.Thread(target=discover_esp, daemon=True)
    discovery_thread.start()

    print("\n╔════════════════════════════════════════╗")
    print("║  CompanionOS Web Remote v7.0           ║")
    print("║  Open http://localhost:5000             ║")
    print("╚════════════════════════════════════════╝\n")

    app.run(host="0.0.0.0", port=5000, debug=False)
