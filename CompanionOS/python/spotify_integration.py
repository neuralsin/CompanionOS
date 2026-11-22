"""
spotify_integration.py
Modular logic for handling Spotify API, playback state, and lyrics processing.
Lyrics are fetched from LRCLIB, independently of Spotify authorization.
"""

import os
import threading
from io import BytesIO

import requests  # type: ignore
import spotipy  # type: ignore
from PIL import Image  # type: ignore
from spotify_authorization import BrowserAuthorization
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
        self.authorization = None
        self.auth_manager = None
        self._authorization_lock = threading.Lock()
        self._authorization_callbacks = []
        self._authorization_result = None

        # Test for dummy placeholders
        if "abc123def456" in client_id or "pqr678stu901" in client_secret:
            print("Spotify: Placeholders detected! Skipping authentication until keys are updated.")
            return

        self.setup_client()

    def setup_client(self) -> None:
        try:
            cache_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), '.cache')
            self.auth_manager = SpotifyOAuth(
                    client_id=self.client_id,
                    client_secret=self.client_secret,
                    redirect_uri=self.redirect_uri,
                    scope=self.scope,
                    open_browser=False,
                    cache_path=cache_path,
                    requests_timeout=15
                )
            token = self.auth_manager.validate_token(self.auth_manager.cache_handler.get_cached_token())
            if not token:
                print("Spotify: browser authorization required")
                return
            sp = spotipy.Spotify(
                auth_manager=self.auth_manager,
                requests_timeout=15,
                retries=3,
                backoff_factor=0.3
            )
            self.client = sp
            self.enabled = True
            print("Spotify Integration Online")
        except Exception:
            print("Spotify: cached authorization unavailable; browser sign-in can reconnect it")

    def request_authorization(self, on_success=None, *, open_browser=True):
        """Start one browser flow without blocking controller networking or prompting stdin."""
        if self.auth_manager is None:
            return False
        def complete(token):
            self.client = spotipy.Spotify(auth_manager=self.auth_manager, requests_timeout=15, retries=1)
            self.enabled = True
            with self._authorization_lock:
                self._authorization_result = token
                callbacks = self._authorization_callbacks[:]
                self._authorization_callbacks.clear()
            for callback in callbacks:
                try:
                    callback(token)
                except Exception:
                    print("Spotify: authorized; device sync will retry on the next connection")

        with self._authorization_lock:
            active = self.authorization is not None and not self.authorization.done.is_set()
            if active and self._authorization_result is not None:
                token = self._authorization_result
            else:
                token = None
            if not active:
                self._authorization_callbacks.clear()
                self._authorization_result = None
            if not token and on_success and on_success not in self._authorization_callbacks:
                self._authorization_callbacks.append(on_success)
            if active and token is None:
                return True
            if not active:
                self.authorization = BrowserAuthorization(self.auth_manager, complete, open_browser=open_browser)
                return self.authorization.start()
        if on_success:
            on_success(token)
        return True

    def get_token_info(self) -> dict | None:
        if not self.enabled or self.client is None:
            return None
        try:
            if hasattr(self.client, 'auth_manager'):
                manager = self.client.auth_manager
                return manager.validate_token(manager.cache_handler.get_cached_token())
        except Exception:
            print("Spotify: token refresh requires browser authorization")
        return None

    def get_current_track(self) -> dict | None:
        self.playback_sample_valid = False
        if not self.enabled or self.client is None:
            return None
        try:
            current = self.client.current_playback()
            self.playback_sample_valid = True
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
            self.playback_sample_valid = False
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
        """Fetch LRCLIB lyrics; search fallback must match both title and artist."""
        import re

        def _decode(data: dict) -> list[dict]:
            if data.get("instrumental") is True:
                return [{"time": 0, "words": "", "status": "Instrumental"}]
            synced = data.get("syncedLyrics")
            if isinstance(synced, str) and synced:
                parsed = _parse_synced(synced)
                if parsed:
                    return parsed
            plain = data.get("plainLyrics")
            if isinstance(plain, str) and plain:
                return [{"time": i * 6000, "words": line, "synced": False}
                        for i, line in enumerate(line for line in plain.splitlines() if line.strip())]
            return []

        def _parse_synced(synced: str) -> list[dict]:
            parsed = []
            offset_tag = re.search(r"(?m)^\[offset:([+-]?\d{1,7})\]", synced)
            offset = int(offset_tag[1]) if offset_tag else 0
            tag = re.compile(r"\[(\d{1,3}):([0-5]\d)(?:\.(\d{1,3})\d*)?\]")
            for line in synced.splitlines():
                times, end = [], 0
                match = tag.match(line, end)
                while match:
                    times.append(int(match[1]) * 60000 + int(match[2]) * 1000 +
                                 int((match[3] or '0').ljust(3, '0')) - offset)
                    end = match.end()
                    match = tag.match(line, end)
                # Empty lines are real timed breaks, not missing lyrics.
                words = re.sub(r'<\d{1,3}:[0-5]\d(?:\.\d+)?>', '', line[end:]).strip()
                parsed.extend({'time': time_ms, 'words': words} for time_ms in times)
            return sorted(parsed, key=lambda line: line['time'])

        # Clean track title: strip (feat. ...), - Remastered, etc.
        clean_title = re.sub(r"\(feat\..*?\)", "", track_name, flags=re.IGNORECASE)
        clean_title = re.sub(r"\(with.*?\)", "", clean_title, flags=re.IGNORECASE)
        clean_title = re.sub(r"- \d{4} Remaster.*", "", clean_title, flags=re.IGNORECASE)
        clean_title = re.sub(r"- Remaster.*", "", clean_title, flags=re.IGNORECASE)
        clean_title = re.sub(r"- Radio Edit.*", "", clean_title, flags=re.IGNORECASE)
        clean_title = re.sub(r"- Single Version.*", "", clean_title, flags=re.IGNORECASE)
        clean_title = re.sub(r"\[.*?\]", "", clean_title)
        clean_title = clean_title.strip()

        for query_title in [track_name, clean_title] if clean_title != track_name else [track_name]:
            try:
                url = "https://lrclib.net/api/get"
                params = {"track_name": query_title, "artist_name": artist_name}
                response = requests.get(url, params=params, timeout=10,
                                        headers={'User-Agent': 'CompanionOS/7.0 (lyrics bridge)'})

                if response.status_code == 200:
                    data = response.json()
                    if isinstance(data, dict):
                        parsed = _decode(data)
                        if parsed:
                            return parsed
            except Exception:
                pass

        # Fallback to search query
        try:
            url = "https://lrclib.net/api/search"
            params = {"q": f"{clean_title} {artist_name}"}
            response = requests.get(url, params=params, timeout=10,
                                    headers={'User-Agent': 'CompanionOS/7.0 (lyrics bridge)'})
            if response.status_code == 200:
                results = response.json()
                if isinstance(results, list):
                    def normalize(text):
                        return re.sub(r"[^\w]", "", str(text).casefold())
                    for item in results:
                        if not isinstance(item, dict):
                            continue
                        if normalize(item.get('artistName')) != normalize(artist_name):
                            continue
                        if normalize(item.get('trackName')) not in {normalize(track_name), normalize(clean_title)}:
                            continue
                        parsed = _decode(item)
                        if parsed:
                            return parsed
        except Exception as e:
            print(f"LRCLIB Search Fallback Error: {e}")

        return [{"time": 0, "words": "", "status": "Lyrics unavailable"}]

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
