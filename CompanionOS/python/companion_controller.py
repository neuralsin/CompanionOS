#!/usr/bin/env python3
"""
═══════════════════════════════════════════════════════════
  COMPANION OS - Python Controller v2.0 (MODULAR)
  
  Controls ESP8266 desk companion
  Integrates Spotify, GitHub, and Musixmatch APIs
  Uses modular architecture and dotenv for secure secrets.
═══════════════════════════════════════════════════════════
"""

import os
import sys
import json
import time
import socket
from datetime import datetime
import threading

# Use python-dotenv to override config with secure local overrides
try:
    from dotenv import load_dotenv
    load_dotenv()
except ImportError:
    pass

from spotify_integration import SpotifyIntegration
from github_integration import GitHubIntegration

CONFIG_FILE = "config.json"

def load_config():
    if not os.path.exists(CONFIG_FILE):
        print(f"ERROR: {CONFIG_FILE} not found! Run tools/install.py first.")
        sys.exit(1)
    with open(CONFIG_FILE, 'r') as f:
        return json.load(f)

config = load_config()

# Load secrets preferentially from environment variables (eliminates hardcoded placeholders)
SPOTIFY_CLIENT_ID = os.getenv("SPOTIFY_CLIENT_ID", config['spotify']['client_id'])
SPOTIFY_CLIENT_SECRET = os.getenv("SPOTIFY_CLIENT_SECRET", config['spotify']['client_secret'])
SPOTIFY_REDIRECT_URI = config['spotify']['redirect_uri']
SPOTIFY_SCOPE = config['spotify']['scope']

MUSIXMATCH_KEY = os.getenv("MUSIXMATCH_KEY", config['musixmatch']['api_key'])

GITHUB_USERNAME = os.getenv("GITHUB_USERNAME", config['github']['username'])
GITHUB_TOKEN = os.getenv("GITHUB_TOKEN", config['github'].get('token', ''))

# Dynamic IP resolution: If ESP IP is a placeholder, default listener on all interfaces.
# We receive the ESP IP dynamically when it connects to our UDP socket.
ESP_IP = config['network']['esp_ip']
ESP_PORT_RX = config['network']['esp_port_rx']
ESP_PORT_TX = config['network']['esp_port_tx']
PC_PORT_RX = config['network']['pc_port_rx']

SPOTIFY_POLL = config['update_intervals']['spotify_poll_seconds']
GITHUB_REFRESH = config['update_intervals']['github_refresh_minutes'] * 60

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
# We track dynamic ESP IP to eliminate hardcoded assumptions
active_esp_ip = ESP_IP if "192.168.1" not in ESP_IP else None

spotify_service = SpotifyIntegration(
    client_id=SPOTIFY_CLIENT_ID,
    client_secret=SPOTIFY_CLIENT_SECRET,
    redirect_uri=SPOTIFY_REDIRECT_URI,
    scope=SPOTIFY_SCOPE,
    musixmatch_key=MUSIXMATCH_KEY,
    lyrics_enabled=config['features']['lyrics_enabled']
)

github_service = GitHubIntegration(
    username=GITHUB_USERNAME,
    token=GITHUB_TOKEN
)

def send_udp(message):
    target_ip = active_esp_ip if active_esp_ip else ESP_IP
    try:
        sock.sendto(message.encode(), (target_ip, ESP_PORT_RX))
    except Exception as e:
        print(f"UDP send error to {target_ip}: {e}")

def command_listener():
    global active_esp_ip
    listen_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    listen_sock.bind(('0.0.0.0', PC_PORT_RX))
    print(f"Listening on port {PC_PORT_RX}...")
    
    while True:
        try:
            data, addr = listen_sock.recvfrom(1024)
            active_esp_ip = addr[0]
            command = data.decode().strip()
            
            if command == "HELLO_COMPANION":
                print(f"👋 Companion Device Discovered at {active_esp_ip}!")
            else:
                print(f"← [{addr[0]}] {command}")
                if command in ["PLAY_PAUSE", "NEXT", "PREV"] or command.startswith("VOLUME:"):
                    spotify_service.control_playback(command)
        except Exception as e:
            print(f"Command error: {e}")

def main():
    print("\n╔════════════════════════════════════════════════╗")
    print("║  COMPANION OS - Controller v2.0 Modular        ║")
    print("╚════════════════════════════════════════════════╝\n")
    
    threading.Thread(target=command_listener, daemon=True).start()
    print("Monitoring playback & connections...")
    
    current_track_id = None
    last_github_check = 0
    last_notes_check = 0
    last_notes_content = []
    last_time_sync = 0
    
    notes_file = os.path.join(os.path.dirname(__file__), "notes.txt")
    if not os.path.exists(notes_file):
        with open(notes_file, "w") as f:
            f.write("Welcome to CompanionOS!\nEdit notes.txt to update.\n")
    
    while True:
        try:
            now = time.time()
            
            # --- Time Sync (every 60s) ---
            if now - last_time_sync > 60 or last_time_sync == 0:
                last_time_sync = now
                dt = datetime.now()
                send_udp(f'TIME:{{"h":{dt.hour},"m":{dt.minute}}}')
            
            # --- Notes Sync ---
            if now - last_notes_check > 5:
                last_notes_check = now
                try:
                    with open(notes_file, "r") as f:
                        lines = [l.strip() for l in f.readlines() if l.strip()][:4]
                        if lines != last_notes_content:
                            last_notes_content = lines
                            send_udp(f"NOTES:{json.dumps(lines)}")
                            print("📝 Synced Notes to Device")
                except Exception as e:
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
                    
                    if spotify_service.lyrics_enabled:
                        lyrics = spotify_service.get_lyrics(track['name'], track['artist'])
                        # Fix UDP Overflow: Only send the active synchronized lines, not the whole sheet
                        send_udp(f"LYRICS:{json.dumps(lyrics[:2])}")
                    
                    if track['album_art_url']:
                        art_data = spotify_service.process_album_art(track['album_art_url'])
                        if art_data:
                            chunk_size = 400
                            total_chunks = (len(art_data) + chunk_size - 1) // chunk_size
                            send_udp(f"ART_START:{total_chunks}")
                            time.sleep(0.02)
                            for i in range(total_chunks):
                                start = i * chunk_size
                                chunk = art_data[start:start + chunk_size]
                                send_udp(f"ART_CHUNK:{i}:{chunk.hex()}")
                                time.sleep(0.02)
                            send_udp("ART_COMPLETE")
                
                if track:
                    state = {'playing': track['is_playing'], 'progress': track['progress_ms']}
                    send_udp(f"STATE:{json.dumps(state)}")
            
            time.sleep(SPOTIFY_POLL)
            
        except KeyboardInterrupt:
            print("\nShutting down...")
            break
        except Exception as e:
            print(f"Error: {e}")
            time.sleep(5)

if __name__ == "__main__":
    main()
