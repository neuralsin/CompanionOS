# PART 4: PYTHON CONTROLLER SETUP (COMPLETE)

## 4.1 PYTHON INSTALLATION - EVERY STEP

### **Why Python?**

The Python controller runs on your PC and:
- Connects to Spotify API (gets track info, controls playback)
- Fetches lyrics from Musixmatch
- Gets GitHub stats
- Processes album art images
- Sends all data to ESP8266 via UDP

**ESP8266 alone can't do this because:**
- Spotify requires OAuth (complex authentication)
- Image processing needs lots of RAM
- API calls need reliable network
- PC has more power for heavy tasks

### **Windows Installation:**

**Step 1: Download Python**
```
1. Open web browser
2. Go to: https://www.python.org/downloads/
3. Page shows: "Download Python 3.11.x"
4. Click the big yellow "Download Python" button
5. Save python-3.11.x-amd64.exe to Downloads
6. Wait for download (~25 MB)
```

**Step 2: Install Python**
```
1. Open Downloads folder
2. Double-click python-3.11.x-amd64.exe
3. Installation window opens

4. ⚠️ CRITICAL: Check these boxes:
   [✓] Install launcher for all users
   [✓] Add python.exe to PATH  ← VERY IMPORTANT!
   
5. Click "Install Now"
6. May ask for administrator permission - click "Yes"
7. Installation progress shows:
   - Installing pip
   - Installing documentation
   - Installing IDLE
   - etc.
8. "Setup was successful" appears
9. Click "Close"
```

**Step 3: Verify Installation**
```
1. Press Windows key + R
2. Type: cmd
3. Press Enter (Command Prompt opens)
4. Type: python --version
5. Should show: Python 3.11.x
6. Type: pip --version
7. Should show: pip 23.x.x from...
8. If both work, installation successful! ✓
```

### **macOS Installation:**

**Step 1: Check if Python already installed**
```
1. Open Terminal (Cmd+Space, type "terminal")
2. Type: python3 --version
3. If shows Python 3.8+, you're good! ✓
4. If not found, continue to install
```

**Step 2: Download and Install**
```
1. Go to: https://www.python.org/downloads/
2. Click "Download Python 3.11.x for macOS"
3. Download python-3.11.x-macos11.pkg
4. Open Downloads, double-click .pkg file
5. Installer opens:
   - Click "Continue"
   - Click "Agree" to license
   - Click "Install"
   - Enter password when asked
   - Wait for installation
6. Click "Close" when done
```

**Step 3: Verify**
```
1. Open Terminal
2. Type: python3 --version
3. Should show: Python 3.11.x ✓
4. Type: pip3 --version
5. Should show pip version ✓
```

**macOS Note:**
Use `python3` and `pip3` commands (not `python` and `pip`).

### **Linux Installation (Ubuntu/Debian):**

**Step 1: Update Package List**
```
sudo apt update
```

**Step 2: Install Python**
```
sudo apt install python3 python3-pip python3-venv -y
```

**Step 3: Verify**
```
python3 --version  # Should show 3.8+
pip3 --version     # Should show pip version
```

---

## 4.2 SETTING UP PROJECT FOLDER

Let's create a proper project structure.

### **Windows:**

**Step 1: Create Project Folder**
```
1. Open File Explorer
2. Navigate to: C:\Users\[YourName]\Documents\
3. Right-click empty space
4. New → Folder
5. Name it: CompanionOS
6. Press Enter
```

**Step 2: Create Subfolders**
```
Inside CompanionOS folder, create:
1. Right-click → New → Folder → Name: python
2. Right-click → New → Folder → Name: arduino
3. Right-click → New → Folder → Name: docs

Structure should be:
C:\Users\[YourName]\Documents\CompanionOS\
├── python\
├── arduino\
└── docs\
```

### **macOS/Linux:**

**Using Terminal:**
```
cd ~/Documents
mkdir -p CompanionOS/{python,arduino,docs}
cd CompanionOS
```

Or use Finder/File Manager to create folders manually.

---

## 4.3 CREATING PYTHON VIRTUAL ENVIRONMENT

**Why virtual environment?**
- Keeps project dependencies isolated
- Prevents version conflicts
- Easy to clean up and start over
- Professional best practice

### **Windows:**

**Step 1: Open Command Prompt in Project Folder**
```
1. Navigate to: C:\Users\[YourName]\Documents\CompanionOS\python\
2. Hold Shift + Right-click in empty space
3. Select "Open PowerShell window here"
   OR
   "Open Command Prompt window here"
4. Terminal opens in this folder
```

**Step 2: Create Virtual Environment**
```
Type this command:
python -m venv venv

Explanation:
- python: Run Python
- -m venv: Use venv module
- venv: Name of virtual environment folder

Wait a few seconds. Creates venv\ folder with Python environment.
```

**Step 3: Activate Virtual Environment**
```
Type:
venv\Scripts\activate

Your prompt changes to:
(venv) C:\Users\...\CompanionOS\python>

The (venv) prefix means environment is active! ✓
```

### **macOS/Linux:**

**Step 1: Open Terminal in Project Folder**
```
cd ~/Documents/CompanionOS/python
```

**Step 2: Create Virtual Environment**
```
python3 -m venv venv
```

**Step 3: Activate**
```
source venv/bin/activate

Prompt changes to:
(venv) username@computer:~/Documents/CompanionOS/python$
```

### **Virtual Environment Commands:**

**Activate (use before working on project):**
```
Windows:   venv\Scripts\activate
Mac/Linux: source venv/bin/activate
```

**Deactivate (when done working):**
```
deactivate
```

**Delete and recreate (if problems):**
```
# Deactivate first
deactivate

# Delete folder
Windows:   rmdir /s venv
Mac/Linux: rm -rf venv

# Recreate
python -m venv venv
```

---

## 4.4 INSTALLING REQUIRED PACKAGES

Make sure virtual environment is activated (see (venv) in prompt)!

### **Package List:**

We need these Python packages:
```
spotipy     - Spotify API wrapper
pillow      - Image processing (album art)
requests    - HTTP requests
python-dotenv - Environment variables (optional)
```

### **Method 1: Install One by One**

```bash
pip install spotipy
pip install pillow
pip install requests
pip install python-dotenv
```

Each install shows:
```
Collecting spotipy
  Downloading spotipy-2.23.0-py3-none-any.whl
Installing collected packages: spotipy
Successfully installed spotipy-2.23.0
```

### **Method 2: Create requirements.txt**

**Step 1: Create file**

In python\ folder, create file: requirements.txt

Content:
```
spotipy==2.23.0
pillow==10.0.0
requests==2.31.0
python-dotenv==1.0.0
```

**Step 2: Install all at once**
```
pip install -r requirements.txt
```

Installs everything in one command!

### **Verify Installation:**

```
pip list
```

Should show:
```
Package         Version
--------------- -------
certifi         2023.7.22
charset-normalizer 3.2.0
idna            3.4
Pillow          10.0.0
python-dotenv   1.0.0
requests        2.31.0
six             1.16.0
spotipy         2.23.0
urllib3         2.0.4
```

If you see spotipy, pillow, and requests, you're ready! ✓

---

## 4.5 GETTING API KEYS (DETAILED)

You need 3 API keys:
1. Spotify Client ID + Secret
2. Musixmatch API Key
3. GitHub Token (optional)

### **SPOTIFY API KEYS - STEP BY STEP**

**Step 1: Create Spotify Account** (if don't have one)
```
1. Go to: https://www.spotify.com/
2. Click "Sign Up"
3. Create free account
4. Verify email
5. Log in

Note: Premium recommended for full API access, but Free works for basic features
```

**Step 2: Go to Spotify Developer Dashboard**
```
1. Open new browser tab
2. Go to: https://developer.spotify.com/dashboard
3. Click "Log In" (top right)
4. Log in with your Spotify credentials
5. Dashboard loads
```

**Step 3: Create New App**
```
1. Click green "Create app" button
2. Fill in form:

   App name: Desk Companion
   App description: Personal desk companion system
   Website: http://localhost (or leave blank)
   Redirect URI: http://localhost:8888/callback
   
   ⚠️ Redirect URI must be EXACTLY: http://localhost:8888/callback
   
   Which API/SDKs are you planning to use?
   [✓] Web API
   
   I understand and agree with Spotify's Developer Terms of Service and Design Guidelines
   [✓] Check this box
   
3. Click "Save"
4. App created! ✓
```

**Step 4: Get Client ID and Secret**
```
1. Your new app appears in dashboard
2. Click on app name "Desk Companion"
3. Settings page opens
4. You'll see:

   Client ID: abc123def456ghi789jkl012mno345pq
   
   Click "View client secret"
   
   Client Secret: pqr678stu901vwx234yz567abc890def
   
5. Copy both values!
   - Keep in text file temporarily
   - We'll use these shortly
   
6. Scroll down to "Redirect URIs"
7. Verify it shows: http://localhost:8888/callback
8. If not, click "Edit Settings" and add it
```

**Step 5: Save Keys Securely**
```
Create text file: spotify_keys.txt

Content:
Client ID: abc123def456ghi789jkl012mno345pq
Client Secret: pqr678stu901vwx234yz567abc890def

Save in safe location (don't share publicly!)
```

### **MUSIXMATCH API KEY - STEP BY STEP**

**Step 1: Create Musixmatch Account**
```
1. Go to: https://developer.musixmatch.com/
2. Click "Sign Up" (top right)
3. Fill in registration:
   - Email
   - Password
   - Confirm password
4. Check "I agree to Terms"
5. Click "Create Account"
6. Check email for verification link
7. Click link to verify
8. Log in to developer dashboard
```

**Step 2: Get API Key**
```
1. After login, dashboard shows
2. Look for "Dashboard" or "Applications"
3. Click "Create New Application" or "Get API Key"
4. Fill in:
   Application Name: Desk Companion
   Application Description: Personal project
   Application Category: Personal
5. Click "Create" or "Submit"
6. API Key appears:
   
   API Key: 1a2b3c4d5e6f7g8h9i0j1k2l3m4n5o6p
   
7. Copy this key!
```

**Step 3: Test API Key**
```
Optional: Test key works

Go to:
https://api.musixmatch.com/ws/1.1/chart.artists.get?apikey=YOUR_KEY_HERE

Replace YOUR_KEY_HERE with your actual key.

If works, shows JSON data with top artists.
If error, check key copied correctly.
```

**Step 4: Note Usage Limits**
```
Free tier limits:
- 2,000 requests per day
- 500 requests per hour
- Non-commercial use only

For our desk companion:
- Fetches lyrics when song changes
- ~10-50 requests per day typical
- Well within free limits! ✓
```

### **GITHUB TOKEN (OPTIONAL) - STEP BY STEP**

Only needed if you want GitHub dashboard feature.

**Step 1: Log in to GitHub**
```
1. Go to: https://github.com/
2. Click "Sign in" (top right)
3. Enter credentials
4. Log in
```

**Step 2: Go to Settings**
```
1. Click your profile picture (top right)
2. Click "Settings" from dropdown
3. Settings page opens
```

**Step 3: Developer Settings**
```
1. Scroll down left sidebar
2. Click "Developer settings" (near bottom)
3. Developer settings page opens
```

**Step 4: Personal Access Tokens**
```
1. Left sidebar: Click "Personal access tokens"
2. Click "Tokens (classic)"
3. Click "Generate new token" button
4. Click "Generate new token (classic)"
```

**Step 5: Configure Token**
```
1. Note field: Desk Companion
2. Expiration: 90 days (or custom)
3. Select scopes (permissions):
   [✓] repo (all repo access)
   [✓] read:user
   [✓] user:email
   
4. Scroll to bottom
5. Click green "Generate token" button
```

**Step 6: Save Token**
```
1. Token appears: ghp_abc123def456ghi789jkl012mno345pqr678
2. ⚠️ COPY IT NOW! You can't see it again!
3. Paste in text file: github_token.txt
4. Keep secure (like a password)
```

**Alternative: Use without token**
```
GitHub public API works without token
Limits: 60 requests/hour (enough for our use)
Dashboard will show public repos only
```

---

## 4.6 CREATING CONFIGURATION FILE

Now let's put all API keys in a config file.

### **Create config.json**

In python\ folder, create file: config.json

**Content:**
```json
{
  "_comment": "Companion OS Configuration - Edit with your keys",
  
  "spotify": {
    "client_id": "abc123def456ghi789jkl012mno345pq",
    "client_secret": "pqr678stu901vwx234yz567abc890def",
    "redirect_uri": "http://localhost:8888/callback",
    "scope": "user-read-playback-state user-modify-playback-state"
  },
  
  "musixmatch": {
    "api_key": "1a2b3c4d5e6f7g8h9i0j1k2l3m4n5o6p"
  },
  
  "github": {
    "username": "yourusername",
    "token": "ghp_abc123def456ghi789jkl012mno345pqr678"
  },
  
  "network": {
    "esp_ip": "192.168.1.123",
    "esp_port_rx": 8888,
    "esp_port_tx": 8889,
    "pc_port_rx": 8889
  },
  
  "features": {
    "spotify_enabled": true,
    "github_enabled": true,
    "lyrics_enabled": true
  },
  
  "update_intervals": {
    "spotify_poll_seconds": 1,
    "github_refresh_minutes": 60
  }
}
```

**Fill in YOUR values:**
```
1. Replace "abc123..." with your actual Spotify Client ID
2. Replace "pqr678..." with your actual Spotify Client Secret
3. Replace "1a2b3c..." with your Musixmatch API key
4. Replace "yourusername" with your GitHub username
5. Replace "ghp_abc..." with your GitHub token (if have one)
6. Replace "192.168.1.123" with your ESP8266's IP address
   (get from Arduino Serial Monitor)
```

**Save the file!**

---

## 4.7 COMPANION CONTROLLER - COMPLETE PYTHON CODE

Create file: companion_controller.py

**Full working controller:**

```python
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
```

---

## 4.8 RUNNING THE CONTROLLER

### **Step 1: Activate Virtual Environment**

```
Windows:   cd C:\Users\...\CompanionOS\python
           venv\Scripts\activate

Mac/Linux: cd ~/Documents/CompanionOS/python
           source venv/bin/activate
```

Prompt shows (venv) prefix.

### **Step 2: Verify Config File**

```
Make sure config.json exists in python\ folder
Contains your actual API keys
ESP IP address matches your ESP8266
```

### **Step 3: Run Controller**

```
python companion_controller.py
```

### **Expected Output:**

```
╔════════════════════════════════════════════════╗
║  COMPANION OS - Controller v1.0                ║
╚════════════════════════════════════════════════╝

Setting up Spotify... ✓
Listening on port 8889...

Connected to ESP at 192.168.1.123:8888
Monitoring Spotify playback...
(Play a song to see it on the companion)
```

**First run - Authentication:**
```
1. Browser opens automatically
2. Shows Spotify login page
3. Log in with your Spotify account
4. Page says "Companion OS wants to:"
   - View your Spotify account data
   - View your activity
   - Control playback
5. Click "Agree" or "Accept"
6. Page says "You can close this page"
7. Close browser
8. Controller continues running
```

**When song plays:**
```
🎵 Never Gonna Give You Up
   Rick Astley
   Whenever You Need Somebody
  Lyrics: 42 lines
  Album art... ✓
  Streaming 28800 bytes in 72 chunks... ✓
```

**ESP8266 should now show:**
- Track name
- Artist
- Album art
- Lyrics scrolling

---

## 4.9 TROUBLESHOOTING PYTHON

### **Error: "ModuleNotFoundError: No module named 'spotipy'"**

**Solution:**
```
1. Make sure virtual environment is activated
   - See (venv) in prompt
2. Install packages:
   pip install spotipy pillow requests
3. Verify with: pip list
```

### **Error: "config.json not found"**

**Solution:**
```
1. Make sure you're in python\ folder
2. Create config.json (see section 4.6)
3. Fill in your actual API keys
```

### **Error: "Invalid client credentials"**

**Spotify authentication fails**

**Solution:**
```
1. Verify Client ID and Secret are correct
2. No extra spaces in config.json
3. Redirect URI must be: http://localhost:8888/callback
4. Check caps lock (credentials are case-sensitive)
```

### **Error: "Connection refused" or "No route to host"**

**Can't connect to ESP**

**Solution:**
```
1. Verify ESP IP in config.json matches Arduino Serial Monitor
2. Ping ESP:
   ping 192.168.1.123  (use your ESP's IP)
3. Check PC and ESP on same WiFi network
4. Check firewall allows UDP ports 8888, 8889
5. Try disabling firewall temporarily to test
```

### **Warning: "Lyrics unavailable"**

**Musixmatch not working**

**Solution:**
```
1. Check Musixmatch API key is correct
2. Verify not exceeded free tier limits (2000/day)
3. Some songs don't have lyrics in database
4. Try different song
```

### **No data shown on ESP**

**Python runs but ESP doesn't update**

**Solution:**
```
1. Check ESP is running (Serial Monitor shows "Ready")
2. Verify ESP IP in both:
   - config.json (Python)
   - Arduino code WiFi connection
3. Check UDP ports match:
   - ESP receives on 8888
   - PC sends to 8888
4. Test with simple UDP:
   Python: send_udp("TEST:Hello")
   ESP should log receipt in Serial Monitor
```

---

## 4.10 TESTING FULL SYSTEM

**Complete System Test:**

```
1. ESP8266 powered on
   - Serial Monitor shows "Ready"
   - Display shows eyes (or welcome screen)

2. Python controller running
   - No errors in console
   - Shows "Monitoring Spotify..."

3. Play song in Spotify
   - Desktop app, phone, web player - any works
   - Play a song you know has lyrics

4. Within 1-2 seconds:
   - Python console shows track info
   - ESP display updates with:
     * Track name
     * Artist name
     * Album art appears
     * Lyrics visible

5. Test controls:
   - Touch left sensor → Previous track
   - Touch right sensor → Next track
   - Tap screen buttons → Control playback

6. Success! ✓
```

**If working, you have a functional Companion OS!**

---

**PYTHON CONTROLLER COMPLETE!** ✓

Your system is now ready to use. Next sections will cover:
- Feature customization
- Adding new emotions
- Creating custom pages
- Advanced features

Would you like me to continue with more sections?
