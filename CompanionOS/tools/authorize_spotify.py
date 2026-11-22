"""One-time Spotify browser sign-in using the controller's existing app settings."""

import argparse
import json
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "python"))

from spotify_integration import SpotifyIntegration


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-browser", action="store_true")
    args = parser.parse_args()
    config = json.loads((ROOT / "python" / "config.json").read_text(encoding="utf-8"))["spotify"]
    service = SpotifyIntegration(
        os.getenv("SPOTIFY_CLIENT_ID", config["client_id"]),
        os.getenv("SPOTIFY_CLIENT_SECRET", config["client_secret"]),
        config["redirect_uri"], config["scope"], lyrics_enabled=False,
    )
    if not service.request_authorization(open_browser=not args.no_browser):
        print("Could not start Spotify authorization. Check app settings or the callback port.", flush=True)
        return 1
    print("Open " + service.authorization.login_url, flush=True)
    service.authorization.done.wait(320)
    print("Spotify authorization: " + service.authorization.status, flush=True)
    return 0 if service.authorization.status == "authorized" else 1


if __name__ == "__main__":
    raise SystemExit(main())
