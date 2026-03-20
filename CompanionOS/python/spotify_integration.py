"""
spotify_integration.py
Modular logic for handling Spotify API, playback state, and lyrics processing.
Lyrics are fetched from a local Spotify Lyrics API proxy (akashrchandran/spotify-lyrics-api).
"""

import os
import sys
import requests
import json
from io import BytesIO
from PIL import Image
import spotipy
from spotipy.oauth2 import SpotifyOAuth


class SpotifyIntegration:
    def __init__(
        self,
        client_id: str,
        client_secret: str,
        redirect_uri: str,
        scope: str,
        lyrics_enabled: bool = True,
    ):
        self.enabled: bool = False
        self.client_id = client_id
        self.client_secret = client_secret
        self.redirect_uri = redirect_uri
        self.scope = scope
        self.lyrics_enabled = lyrics_enabled
        self.client: spotipy.Spotify | None = None
        self.current_track_id: str | None = None

        # Test for dummy placeholders
        if "abc123def456" in client_id or "pqr678stu901" in client_secret:
            print("Spotify: Placeholders detected! Skipping authentication until keys are updated.")
            return

        self.setup_client()

    def setup_client(self) -> None:
        try:
            sp = spotipy.Spotify(
                auth_manager=SpotifyOAuth(
                    client_id=self.client_id,
                    client_secret=self.client_secret,
                    redirect_uri=self.redirect_uri,
                    scope=self.scope,
                    open_browser=False,
                )
            )
            sp.current_user()
            self.client = sp
            self.enabled = True
            print("Spotify Integration Online")
        except Exception as e:
            print(f"Spotify Setup Error: {e}")

    def get_current_track(self) -> dict | None:
        if not self.enabled or self.client is None:
            return None
        try:
            current = self.client.current_playback()
            if not current or not current.get("item") or not current["is_playing"]:
                return None
            track = current["item"]
            return {
                "id": track["id"],
                "name": track["name"],
                "artist": track["artists"][0]["name"],
                "album": track["album"]["name"],
                "duration_ms": track["duration_ms"],
                "progress_ms": current["progress_ms"],
                "is_playing": current["is_playing"],
                "album_art_url": (
                    track["album"]["images"][0]["url"]
                    if track["album"]["images"]
                    else None
                ),
            }
        except Exception as e:
            print(f"Spotify Track Error: {e}")
            return None

    def control_playback(self, command: str) -> bool:
        if not self.enabled or self.client is None:
            return False

        try:
            if command == "PLAY_PAUSE":
                current = self.client.current_playback()
                if current and current.get("is_playing"):
                    self.client.pause_playback()
                else:
                    self.client.start_playback()
            elif command == "NEXT":
                self.client.next_track()
            elif command == "PREV":
                self.client.previous_track()
            elif command.startswith("VOLUME:"):
                volume = int(command.split(":")[1])
                self.client.volume(volume)
            return True
        except Exception as e:
            print(f"Spotify Playback Control Error: {e}")
            return False

    def get_lyrics(
        self, track_name: str, artist_name: str, track_id: str | None = None
    ) -> list[str]:
        """Fetch lyrics from a local spotify-lyrics-api proxy (akashrchandran/spotify-lyrics-api)."""
        if not self.lyrics_enabled or not track_id:
            return ["♪ Lyrics unavailable ♪ (Disabled or Missing ID)"]

        try:
            url = "http://localhost:8080/"
            params = {"trackid": track_id, "format": "lrc"}
            response = requests.get(url, params=params, timeout=5)

            if response.status_code == 200:
                data = response.json()
                if not data.get("error"):
                    lines = data.get("lines", [])
                    if lines:
                        return [
                            line.get("words", "")
                            for line in lines
                            if line.get("words")
                        ]
            return ["♪ Instrumental ♪"]
        except Exception as e:
            print(f"Lyrics Engine (Local API) Error: {e}")
            return ["♪ Lyrics unavailable ♪"]

    def process_album_art(self, image_url: str, size: int = 120) -> bytes | None:
        try:
            response = requests.get(image_url, timeout=10)
            img = Image.open(BytesIO(response.content))
            img = img.resize((size, size), Image.Resampling.LANCZOS)
            img = img.convert("RGB")

            pixels: list[int] = []
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
