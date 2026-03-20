#!/usr/bin/env python3
"""
═══════════════════════════════════════════════════════════
  COMPANION OS - Python Controller v1.0
  
  Controls ESP8266 desk companion
  Integrates Spotify, GitHub, and Musixmatch APIs
═══════════════════════════════════════════════════════════
"""

import os
import sys
import json
import time
import socket
import threading
import requests
from io import BytesIO
from PIL import Image

# Check dependencies
try:
    import spotipy
    from spotipy.oauth2 import SpotifyOAuth
except ImportError:
    print("ERROR: spotipy not installed")
    print("Run: pip install spotipy")
    sys.exit(1)

try:
    from PIL import Image
except ImportError:
    print("ERROR: Pillow not installed")
    print("Run: pip install pillow")
    sys.exit(1)

# ═══════════════════════════════════════════════════════════
# CONFIGURATION
# ═══════════════════════════════════════════════════════════

CONFIG_FILE = "config.json"

def load_config():
    """Load configuration from JSON file"""
    if not os.path.exists(CONFIG_FILE):
        print(f"ERROR: {CONFIG_FILE} not found!")
        print("Please create config.json with your API keys")
        sys.exit(1)
    
    with open(CONFIG_FILE, 'r') as f:
        return json.load(f)

config = load_config()

# Extract settings
SPOTIFY_CLIENT_ID = config['spotify']['client_id']
SPOTIFY_CLIENT_SECRET = config['spotify']['client_secret']
SPOTIFY_REDIRECT_URI = config['spotify']['redirect_uri']
SPOTIFY_SCOPE = config['spotify']['scope']

MUSIXMATCH_KEY = config['musixmatch']['api_key']

GITHUB_USERNAME = config['github']['username']
GITHUB_TOKEN = config['github'].get('token', '')  # Optional

ESP_IP = config['network']['esp_ip']
ESP_PORT_RX = config['network']['esp_port_rx']
ESP_PORT_TX = config['network']['esp_port_tx']
PC_PORT_RX = config['network']['pc_port_rx']

SPOTIFY_ENABLED = config['features']['spotify_enabled']
GITHUB_ENABLED = config['features']['github_enabled']
LYRICS_ENABLED = config['features']['lyrics_enabled']

SPOTIFY_POLL = config['update_intervals']['spotify_poll_seconds']
GITHUB_REFRESH = config['update_intervals']['github_refresh_minutes'] * 60

# ═══════════════════════════════════════════════════════════
# GLOBAL STATE
# ═══════════════════════════════════════════════════════════

spotify_client = None
current_track_id = None
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# ═══════════════════════════════════════════════════════════
# SPOTIFY FUNCTIONS
# ═══════════════════════════════════════════════════════════

def setup_spotify():
    """Initialize Spotify API"""
    global spotify_client
    
    print("Setting up Spotify...", end=" ")
    
    try:
        spotify_client = spotipy.Spotify(
            auth_manager=SpotifyOAuth(
                client_id=SPOTIFY_CLIENT_ID,
                client_secret=SPOTIFY_CLIENT_SECRET,
                redirect_uri=SPOTIFY_REDIRECT_URI,
                scope=SPOTIFY_SCOPE
            )
        )
        
        # Test connection
        spotify_client.current_user()
        print("✓")
        return True
        
    except Exception as e:
        print(f"✗\nError: {e}")
        return False

def get_current_track():
    """Get currently playing track info"""
    try:
        current = spotify_client.current_playback()
        
        if not current or not current['is_playing']:
            return None
        
        track = current['item']
        
        return {
            'id': track['id'],
            'name': track['name'],
            'artist': track['artists'][0]['name'],
            'album': track['album']['name'],
            'duration_ms': track['duration_ms'],
            'progress_ms': current['progress_ms'],
            'is_playing': current['is_playing'],
            'album_art_url': track['album']['images'][0]['url'] if track['album']['images'] else None
        }
    
    except Exception as e:
        print(f"Spotify error: {e}")
        return None

def control_playback(command):
    """Control Spotify playback"""
    try:
        if command == "PLAY_PAUSE":
            current = spotify_client.current_playback()
            if current and current['is_playing']:
                spotify_client.pause_playback()
                print("Paused")
            else:
                spotify_client.start_playback()
                print("Playing")
        
        elif command == "NEXT":
            spotify_client.next_track()
            print("Next track")
        
        elif command == "PREV":
            spotify_client.previous_track()
            print("Previous track")
        
        elif command.startswith("VOLUME:"):
            volume = int(command.split(':')[1])
            spotify_client.volume(volume)
            print(f"Volume: {volume}%")
        
        return True
    
    except Exception as e:
        print(f"Playback control error: {e}")
        return False

# ═══════════════════════════════════════════════════════════
# LYRICS FUNCTIONS
# ═══════════════════════════════════════════════════════════

def get_lyrics(track_name, artist_name):
    """Fetch lyrics from Musixmatch"""
    
    if not LYRICS_ENABLED or not MUSIXMATCH_KEY:
        return ["♪ Lyrics unavailable ♪"]
    
    try:
        url = "https://api.musixmatch.com/ws/1.1/matcher.lyrics.get"
        params = {
            'q_track': track_name,
            'q_artist': artist_name,
            'apikey': MUSIXMATCH_KEY
        }
        
        response = requests.get(url, params=params, timeout=5)
        data = response.json()
        
        if data['message']['header']['status_code'] == 200:
            lyrics_body = data['message']['body']['lyrics']['lyrics_body']
            
            # Parse into lines
            lines = lyrics_body.split('\n')
            lines = [line.strip() for line in lines if line.strip()]
            
            # Remove Musixmatch footer
            lines = [l for l in lines if not l.startswith('***')]
            
            if lines:
                print(f"  Lyrics: {len(lines)} lines")
                return lines
        
        return ["♪ Instrumental ♪"]
    
    except Exception as e:
        print(f"  Lyrics error: {e}")
        return ["♪ Lyrics unavailable ♪"]

# ═══════════════════════════════════════════════════════════
# ALBUM ART FUNCTIONS
# ═══════════════════════════════════════════════════════════

def process_album_art(image_url, size=120):
    """Download and convert album art to RGB565"""
    
    try:
        print("  Album art...", end=" ")
        
        # Download image
        response = requests.get(image_url, timeout=10)
        img = Image.open(BytesIO(response.content))
        
        # Resize to square
        img = img.resize((size, size), Image.Resampling.LANCZOS)
        img = img.convert('RGB')
        
        # Convert to RGB565 format
        pixels = []
        for y in range(size):
            for x in range(size):
                r, g, b = img.getpixel((x, y))
                # RGB565: RRRRR GGGGGG BBBBB
                rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                pixels.append(rgb565 >> 8)    # High byte
                pixels.append(rgb565 & 0xFF)  # Low byte
        
        print("✓")
        return bytes(pixels)
    
    except Exception as e:
        print(f"✗ ({e})")
        return None

def send_album_art(art_data, chunk_size=400):
    """Stream album art to ESP in chunks"""
    
    if not art_data:
        return
    
    total_chunks = (len(art_data) + chunk_size - 1) // chunk_size
    print(f"  Streaming {len(art_data)} bytes in {total_chunks} chunks...", end=" ")
    
    # Send header
    send_udp(f"ART_START:{total_chunks}")
    time.sleep(0.02)
    
    # Send chunks
    for i in range(total_chunks):
        start = i * chunk_size
        end = min(start + chunk_size, len(art_data))
        chunk = art_data[start:end]
        
        # Encode as hex to avoid binary data issues
        chunk_hex = chunk.hex()
        packet = f"ART_CHUNK:{i}:{chunk_hex}"
        
        sock.sendto(packet.encode(), (ESP_IP, ESP_PORT_RX))
        time.sleep(0.02)  # Rate limit
    
    # Complete signal
    send_udp("ART_COMPLETE")
    print("✓")

# ═══════════════════════════════════════════════════════════
# GITHUB FUNCTIONS
# ═══════════════════════════════════════════════════════════

def get_github_profile():
    """Fetch GitHub profile stats"""
    
    if not GITHUB_ENABLED:
        return None
    
    try:
        print("GitHub profile...", end=" ")
        
        headers = {}
        if GITHUB_TOKEN:
            headers['Authorization'] = f'token {GITHUB_TOKEN}'
        
        url = f"https://api.github.com/users/{GITHUB_USERNAME}"
        response = requests.get(url, headers=headers, timeout=5)
        data = response.json()
        
        profile = {
            'username': data['login'],
            'name': data.get('name', ''),
            'repos': data['public_repos'],
            'followers': data['followers'],
            'following': data['following'],
            'avatar_url': data['avatar_url']
        }
        
        print("✓")
        return profile
    
    except Exception as e:
        print(f"✗ ({e})")
        return None

# ═══════════════════════════════════════════════════════════
# UDP COMMUNICATION
# ═══════════════════════════════════════════════════════════

def send_udp(message):
    """Send UDP packet to ESP"""
    try:
        sock.sendto(message.encode(), (ESP_IP, ESP_PORT_RX))
    except Exception as e:
        print(f"UDP send error: {e}")

def command_listener():
    """Listen for commands from ESP"""
    
    listen_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    listen_sock.bind(('0.0.0.0', PC_PORT_RX))
    
    print(f"Listening on port {PC_PORT_RX}...")
    
    while True:
        try:
            data, addr = listen_sock.recvfrom(1024)
            command = data.decode().strip()
            
            print(f"← {command}")
            
            if command in ["PLAY_PAUSE", "NEXT", "PREV"]:
                control_playback(command)
            
            elif command.startswith("VOLUME:"):
                control_playback(command)
        
        except Exception as e:
            print(f"Command error: {e}")

# ═══════════════════════════════════════════════════════════
# MAIN LOOP
# ═══════════════════════════════════════════════════════════

def main():
    """Main controller loop"""
    global current_track_id
    
    print("\n╔════════════════════════════════════════════════╗")
    print("║  COMPANION OS - Controller v1.0                ║")
    print("╚════════════════════════════════════════════════╝\n")
    
    # Setup Spotify
    if SPOTIFY_ENABLED:
        if not setup_spotify():
            print("Spotify disabled")
            SPOTIFY_ENABLED = False
    
    # Start command listener thread
    listener = threading.Thread(target=command_listener, daemon=True)
    listener.start()
    
    print(f"\nConnected to ESP at {ESP_IP}:{ESP_PORT_RX}")
    print("Monitoring Spotify playback...")
    print("(Play a song to see it on the companion)\n")
    
    # Main loop
    while True:
        try:
            if SPOTIFY_ENABLED:
                # Get current track
                track = get_current_track()
                
                if track and track['id'] != current_track_id:
                    # New track detected
                    print(f"\n🎵 {track['name']}")
                    print(f"   {track['artist']}")
                    print(f"   {track['album']}")
                    
                    current_track_id = track['id']
                    
                    # Send track info
                    info = {
                        'track': track['name'],
                        'artist': track['artist'],
                        'album': track['album'],
                        'duration': track['duration_ms']
                    }
                    send_udp(f"TRACK:{json.dumps(info)}")
                    
                    # Get and send lyrics
                    if LYRICS_ENABLED:
                        lyrics = get_lyrics(track['name'], track['artist'])
                        send_udp(f"LYRICS:{json.dumps(lyrics)}")
                    
                    # Process and send album art
                    if track['album_art_url']:
                        art_data = process_album_art(track['album_art_url'])
                        if art_data:
                            send_album_art(art_data)
                    
                    print()
                
                # Send playback state
                if track:
                    state = {
                        'playing': track['is_playing'],
                        'progress': track['progress_ms']
                    }
                    send_udp(f"STATE:{json.dumps(state)}")
            
            time.sleep(SPOTIFY_POLL)
        
        except KeyboardInterrupt:
            print("\n\nShutting down...")
            break
        except Exception as e:
            print(f"Error: {e}")
            time.sleep(5)

if __name__ == "__main__":
    main()
