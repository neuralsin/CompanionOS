"""Read-only reachability checks. Never print OAuth credentials or HTTP bodies."""

import argparse
import json
import re
from pathlib import Path

import requests


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--refresh", action="store_true",
                        help="Check OAuth refresh and test a temporary access token; never change playback")
    parser.add_argument("--source", choices=("firmware", "python"), default="firmware",
                        help="Read credentials from firmware or the existing local controller configuration")
    args = parser.parse_args()
    folder = Path(__file__).resolve().parents[1] / "arduino" / "CompanionOS_Main"
    private = folder / "spotify_credentials.local.h"
    public = (folder / "config.h").read_text(encoding="utf-8")
    config = private.read_text(encoding="utf-8") if private.exists() else public
    def setting(name):
        match = re.search(r'(?m)^\s*#define\s+' + re.escape(name) + r'\s+"([^"]*)"', config)
        if not match:
            match = re.search(re.escape(name) + r'\s*=\s*"([^"]*)"', config)
        return match.group(1) if match else ""

    token = setting("DEFAULT_SPOTIFY_ACCESS_TOKEN")
    refresh = setting("DEFAULT_SPOTIFY_REFRESH_TOKEN")
    client_id = setting("SPOTIFY_CLIENT_ID")
    client_secret = setting("SPOTIFY_CLIENT_SECRET")
    if args.source == "python":
        folder = Path(__file__).resolve().parents[1] / "python"
        spotify = json.loads((folder / "config.json").read_text(encoding="utf-8")).get("spotify", {})
        cache = json.loads((folder / ".cache").read_text(encoding="utf-8"))
        token, refresh = cache.get("access_token", ""), cache.get("refresh_token", "")
        client_id, client_secret = spotify.get("client_id", ""), spotify.get("client_secret", "")
    if args.refresh:
        try:
            response = requests.post(
                "https://accounts.spotify.com/api/token",
                data={"grant_type": "refresh_token",
                      "refresh_token": refresh},
                auth=(client_id, client_secret),
                timeout=(5, 10),
            )
            payload = response.json()
            print(f"Spotify refresh: HTTP {response.status_code}; "
                  f"result={'ok' if response.ok and payload.get('access_token') else 'rejected'}")
            error = payload.get("error")
            if error in {"invalid_grant", "invalid_client", "invalid_request", "unsupported_grant_type"}:
                print(f"Spotify authorization: {error}")
            if response.ok:
                token = payload.get("access_token", "")
        except (requests.RequestException, ValueError) as exc:
            print(f"Spotify refresh: {type(exc).__name__}")
    def coordinate(name):
        match = re.search(r'(?m)^\s*#define\s+' + name + r'\s+(-?[0-9]+(?:\.[0-9]+)?)', public)
        if not match:
            raise ValueError("Missing firmware weather coordinate")
        return match.group(1)

    weather_url = ("https://api.open-meteo.com/v1/forecast?latitude=" + coordinate("WEATHER_LATITUDE")
                   + "&longitude=" + coordinate("WEATHER_LONGITUDE")
                   + "&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m"
                   + "&daily=temperature_2m_max,temperature_2m_min,sunrise,sunset&forecast_days=1&timezone=auto")
    urls = [
        ("Weather", weather_url, {}),
        ("Spotify", "https://api.spotify.com/v1/me/player",
         {"Authorization": "Bearer " + token} if token else {}),
    ]
    for name, url, headers in urls:
        try:
            headers["Accept-Encoding"] = "identity"
            response = requests.get(url, headers=headers, timeout=(5, 10))
            print(f"{name}: HTTP {response.status_code}; "
                  f"transfer={response.headers.get('Transfer-Encoding', 'length/close')}; "
                  f"encoding={response.headers.get('Content-Encoding', 'identity')}")
        except requests.RequestException as exc:
            print(f"{name}: {type(exc).__name__}")


if __name__ == "__main__":
    main()
