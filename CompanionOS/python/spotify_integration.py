"""
spotify_integration.py
Modular logic for handling Spotify API, playback state, and lyrics processing.
Eliminates hardcoded strings - utilizes .env structure falling back on config.json
"""

import os
import requests
import json
from io import BytesIO
from PIL import Image

try:
    import spotipy
    from spotipy.oauth2 import SpotifyOAuth
except ImportError:
    spotipy = None

class SpotifyIntegration:
    def __init__(self, client_id, client_secret, redirect_uri, scope, musixmatch_key, lyrics_enabled=True):
        self.enabled = False
        self.client_id = client_id
        self.client_secret = client_secret
        self.redirect_uri = redirect_uri
        self.scope = scope
        self.musixmatch_key = musixmatch_key
        self.lyrics_enabled = lyrics_enabled
        self.client = None
        self.current_track_id = None
        
        # Test for dummy placeholders
        if "abc123def456" in client_id or "pqr678stu901" in client_secret:
            print("Spotify: Placeholders detected! Skipping authentication until keys are updated.")
            return
            
        if spotipy:
            self.setup_client()

    def setup_client(self):
        try:
            self.client = spotipy.Spotify(
                auth_manager=SpotifyOAuth(
                    client_id=self.client_id,
                    client_secret=self.client_secret,
                    redirect_uri=self.redirect_uri,
                    scope=self.scope,
                    open_browser=False
                )
            )
            self.client.current_user()
            self.enabled = True
            print("Spotify Integration Online")
        except Exception as e:
            print(f"Spotify Setup Error: {e}")

    def get_current_track(self):
        if not self.enabled:
            return None
        try:
            current = self.client.current_playback()
            if not current or not current.get('item') or not current['is_playing']:
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
            print(f"Spotify Track Error: {e}")
            return None

    def control_playback(self, command):
        if not self.enabled:
            return False
            
        try:
            if command == "PLAY_PAUSE":
                current = self.client.current_playback()
                if current and current.get('is_playing'):
                    self.client.pause_playback()
                else:
                    self.client.start_playback()
            elif command == "NEXT":
                self.client.next_track()
            elif command == "PREV":
                self.client.previous_track()
            elif command.startswith("VOLUME:"):
                volume = int(command.split(':')[1])
                self.client.volume(volume)
            return True
        except Exception as e:
            print(f"Spotify Playback Control Error: {e}")
            return False

    def get_lyrics(self, track_name, artist_name):
        if not self.lyrics_enabled or not self.musixmatch_key or "1a2b3c" in self.musixmatch_key:
            return ["♪ Lyrics unavailable ♪ (Missing Auth)"]
        
        try:
            url = "https://api.musixmatch.com/ws/1.1/matcher.lyrics.get"
            params = {
                'q_track': track_name,
                'q_artist': artist_name,
                'apikey': self.musixmatch_key
            }
            response = requests.get(url, params=params, timeout=5)
            data = response.json()
            
            if data.get('message', {}).get('header', {}).get('status_code') == 200:
                lyrics_body = data['message']['body']['lyrics']['lyrics_body']
                lines = [line.strip() for line in lyrics_body.split('\n') if line.strip()]
                lines = [l for l in lines if not l.startswith('***')]
                if lines:
                    return lines
            return ["♪ Instrumental ♪"]
        except Exception as e:
            print(f"Lyrics Engine Error: {e}")
            return ["♪ Lyrics unavailable ♪"]

    def process_album_art(self, image_url, size=120):
        try:
            response = requests.get(image_url, timeout=10)
            img = Image.open(BytesIO(response.content))
            img = img.resize((size, size), Image.Resampling.LANCZOS)
            img = img.convert('RGB')
            
            pixels = []
            for y in range(size):
                for x in range(size):
                    r, g, b = img.getpixel((x, y))
                    rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                    pixels.append(rgb565 >> 8)
                    pixels.append(rgb565 & 0xFF)
            return bytes(pixels)
        except Exception as e:
            print(f"Album Art Processing Error: {e}")
            return None
