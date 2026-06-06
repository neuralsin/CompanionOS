"""
spotify_integration.py
Modular logic for handling Spotify API, playback state, and lyrics processing.
Lyrics are fetched from a local Spotify Lyrics API proxy (akashrchandran/spotify-lyrics-api).
"""

import os
import sys
import requests  # type: ignore
import json
from io import BytesIO
from PIL import Image  # type: ignore
import spotipy  # type: ignore
from spotipy.oauth2 import SpotifyOAuth  # type: ignore


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
            import os
            cache_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), '.cache')
            sp = spotipy.Spotify(
                auth_manager=SpotifyOAuth(
                    client_id=self.client_id,
                    client_secret=self.client_secret,
                    redirect_uri=self.redirect_uri,
                    scope=self.scope,
                    open_browser=False,
                    cache_path=cache_path,
                    requests_timeout=15
                ),
                requests_timeout=15,
                retries=3,
                backoff_factor=0.3
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
            if not current or not current.get("item"):
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
            elif command.startswith("SEEK:"):
                pos_ms = int(command.split(":")[1])
                self.client.seek_track(pos_ms)
            return True
        except Exception as e:
            print(f"Spotify Playback Control Error: {e}")
            return False

    def get_lyrics(
        self, track_name: str, artist_name: str, track_id: str | None = None
    ) -> list[dict]:
        """Fetch lyrics dynamically from the public LRCLIB network."""
        import time
        for attempt in range(3):
            try:
                url = "https://lrclib.net/api/get"
                params = {"track_name": track_name, "artist_name": artist_name}
                response = requests.get(url, params=params, timeout=15)

                if response.status_code == 200:
                    data = response.json()
                    synced = data.get("syncedLyrics")
                    if synced:
                        parsed = []
                        lines = synced.split("\n")
                        for l in lines:
                            if l.startswith("[") and "]" in l:
                                parts = l.split("]", 1)
                                time_str = parts[0][1:]
                                words = parts[1].strip()
                                if words:
                                    try:
                                        m, s = time_str.split(":")
                                        sec, ms_val = s.split(".") if "." in s else (s, "0")
                                        time_ms = int(m)*60000 + int(sec)*1000 + int(str(ms_val).ljust(3, "0")[:3])  # type: ignore
                                        parsed.append({"time": time_ms, "words": words})
                                    except Exception:
                                        pass
                        if parsed:
                            return parsed
                    elif data.get("plainLyrics"):
                        return [{"time": 0, "words": "♪ Unsynced lyrics available ♪"}]
                return [{"time": 0, "words": "♪ Instrumental ♪"}]
            except requests.exceptions.Timeout:
                if attempt < 2:
                    time.sleep(1)
                    continue
                print(f"Lyrics Engine (LRCLIB) Error: Request timed out after 3 attempts.")
                return [{"time": 0, "words": "♪ Lyrics unavailable ♪"}]
            except Exception as e:
                print(f"Lyrics Engine (LRCLIB) Error: {e}")
                return [{"time": 0, "words": "♪ Lyrics unavailable ♪"}]
        return [{"time": 0, "words": "♪ Lyrics unavailable ♪"}]

    def process_album_art(self, image_url: str, size: int = 96) -> list[int] | None:
        """Returns a list of RGB565 integer pixels."""
        try:
            response = requests.get(image_url, timeout=10)
            img = Image.open(BytesIO(response.content))
            img = img.resize((size, size), Image.Resampling.LANCZOS)
            img = img.convert("RGB")

            pixels = []
            for y in range(size):
                for x in range(size):
                    r, g, b = img.getpixel((x, y))  # type: ignore
                    rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                    pixels.append(rgb565)
            return pixels
        except Exception as e:
            print(f"Album Art Processing Error: {e}")
            return None
