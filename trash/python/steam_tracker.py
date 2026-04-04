#!/usr/bin/env python3
"""
═══════════════════════════════════════════════════════════
  STEAM TRACKER — V6 CompanionOS Gaming Module

  Dual-source game tracking:
  1. Steam Web API — game details, achievements, friends
  2. Local process detection — currently running games
═══════════════════════════════════════════════════════════
"""

import os
import json
import time
import psutil
import requests
from datetime import datetime

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


class SteamTracker:
    """Dual-source game tracking: Steam API + local process detection."""

    # Common game executable names → friendly titles
    KNOWN_GAMES = {
        'csgo.exe': 'Counter-Strike 2',
        'cs2.exe': 'Counter-Strike 2',
        'dota2.exe': 'Dota 2',
        'VALORANT-Win64-Shipping.exe': 'VALORANT',
        'GenshinImpact.exe': 'Genshin Impact',
        'Minecraft.exe': 'Minecraft',
        'javaw.exe': 'Minecraft',
        'RocketLeague.exe': 'Rocket League',
        'FortniteClient-Win64-Shipping.exe': 'Fortnite',
        'eldenring.exe': 'Elden Ring',
        'Overwatch.exe': 'Overwatch 2',
        'RainbowSix.exe': 'Rainbow Six Siege',
        'GTA5.exe': 'GTA V',
        'GTAV.exe': 'GTA V',
        'Cyberpunk2077.exe': 'Cyberpunk 2077',
        'Baldur.exe': 'Baldur\'s Gate 3',
        'bg3.exe': 'Baldur\'s Gate 3',
        'EpicGamesLauncher.exe': None,  # Skip launchers
        'steam.exe': None,
        'steamwebhelper.exe': None,
    }

    def __init__(self, steam_api_key='', steam_id=''):
        self.api_key = steam_api_key
        self.steam_id = steam_id
        self.enabled = bool(steam_api_key and steam_id)
        self.session_start = None
        self.current_game = ''
        self.last_game = ''

    def _steam_get(self, interface, method, version='v0001', **params):
        """Make a Steam Web API call."""
        if not self.enabled:
            return None
        url = f"https://api.steampowered.com/{interface}/{method}/{version}/"
        params['key'] = self.api_key
        try:
            resp = requests.get(url, params=params, timeout=10)
            if resp.status_code == 200:
                return resp.json()
        except Exception as e:
            print(f"Steam API error: {e}")
        return None

    def get_player_summary(self):
        """Get player profile and currently playing game via Steam API."""
        data = self._steam_get(
            'ISteamUser', 'GetPlayerSummaries', 'v0002',
            steamids=self.steam_id
        )
        if data and 'response' in data and 'players' in data['response']:
            players = data['response']['players']
            if players:
                return players[0]
        return None

    def get_friends_online(self):
        """Count online friends."""
        data = self._steam_get(
            'ISteamUser', 'GetFriendList', 'v0001',
            steamid=self.steam_id, relationship='friend'
        )
        if not data or 'friendslist' not in data:
            return 0

        friend_ids = [f['steamid'] for f in data['friendslist']['friends']]
        if not friend_ids:
            return 0

        # Batch query up to 100 friends
        batch = ','.join(friend_ids[:100])
        summaries = self._steam_get(
            'ISteamUser', 'GetPlayerSummaries', 'v0002',
            steamids=batch
        )
        if not summaries or 'response' not in summaries:
            return 0

        online_count = sum(
            1 for p in summaries['response'].get('players', [])
            if p.get('personastate', 0) > 0
        )
        return online_count

    def get_achievements(self, app_id):
        """Get achievement completion percentage for a game."""
        data = self._steam_get(
            'ISteamUserStats', 'GetPlayerAchievements', 'v0001',
            steamid=self.steam_id, appid=str(app_id)
        )
        if not data or 'playerstats' not in data:
            return 0

        achievements = data['playerstats'].get('achievements', [])
        if not achievements:
            return 0

        achieved = sum(1 for a in achievements if a.get('achieved', 0) == 1)
        return int(achieved / len(achievements) * 100)

    def get_recently_played(self):
        """Get recently played games for weekly stats."""
        data = self._steam_get(
            'IPlayerService', 'GetRecentlyPlayedGames', 'v0001',
            steamid=self.steam_id, count=5
        )
        if data and 'response' in data:
            return data['response'].get('games', [])
        return []

    def detect_local_game(self):
        """Detect running game from local processes."""
        for proc in psutil.process_iter(['name']):
            try:
                name = proc.info['name']
                if name in self.KNOWN_GAMES:
                    title = self.KNOWN_GAMES[name]
                    if title:  # Skip None entries (launchers)
                        return title
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                continue
        return ''

    def get_session_time(self):
        """Get formatted session duration."""
        if not self.session_start:
            return '00:00'
        elapsed = int(time.time() - self.session_start)
        hours = elapsed // 3600
        minutes = (elapsed % 3600) // 60
        if hours > 0:
            return f'{hours}:{minutes:02d}:{elapsed % 60:02d}'
        return f'{minutes:02d}:{elapsed % 60:02d}'

    def get_game_cover_url(self, title):
        """Fetch capsule image url from Steam Store API by title. No token required."""
        try:
            url = "https://store.steampowered.com/api/storesearch/"
            params = {"term": title, "l": "english", "cc": "US"}
            resp = requests.get(url, params=params, timeout=5)
            if resp.status_code == 200:
                data = resp.json()
                if data.get('total', 0) > 0 and 'items' in data:
                    img_url = data['items'][0].get('tiny_image')
                    if img_url:
                        # Convert to https if it's protocol relative or http
                        if img_url.startswith('//'):
                            return 'https:' + img_url
                        return img_url
        except Exception as e:
            print(f"Error fetching game cover for {title}: {e}")
        return None

    def get_gaming_state(self):
        """
        Get complete gaming state for ESP display.
        Returns dict ready for JSON serialization.
        """
        result = {
            'title': '',
            'session': '00:00',
            'achieve': 0,
            'friends': 0,
            'active': False,
            'status': 'Offline'
        }

        # 1. Steam API check (gets game title, state, friends)
        steam_game = ''
        steam_appid = 0
        if self.enabled:
            player = self.get_player_summary()
            if player:
                steam_game = player.get('gameextrainfo', '')
                steam_appid = player.get('gameid', 0)
                if steam_appid:
                    steam_appid = int(steam_appid)

                state = player.get('personastate', 0)
                if steam_game:
                    result['status'] = 'In-Game'
                elif state == 1:
                    result['status'] = 'Online'
                elif state == 3:
                    result['status'] = 'Away'
                else:
                    result['status'] = 'Offline'

            result['friends'] = min(self.get_friends_online(), 255)

        # 2. Local process fallback
        local_game = self.detect_local_game()

        # Prefer Steam API data, fallback to local detection
        active_game = steam_game if steam_game else local_game

        if active_game:
            result['title'] = active_game[:23]
            result['active'] = True
            if not steam_game:
                result['status'] = 'Playing'

            # Track session start
            if active_game != self.current_game:
                self.session_start = time.time()
                self.current_game = active_game
            result['session'] = self.get_session_time()

            # Achievements (only if we have Steam appid)
            if steam_appid and self.enabled:
                try:
                    result['achieve'] = self.get_achievements(steam_appid)
                except Exception:
                    result['achieve'] = 0
        else:
            # No game running
            if self.current_game:
                self.last_game = self.current_game
                self.current_game = ''
                self.session_start = None
            result['status'] = 'Online' if self.enabled else 'Offline'

        return result


# ── Standalone test ───────────────────────────────────────
if __name__ == '__main__':
    config_file = os.path.join(SCRIPT_DIR, 'config.json')
    if os.path.exists(config_file):
        with open(config_file) as f:
            cfg = json.load(f)
        gaming = cfg.get('gaming', {})
        tracker = SteamTracker(
            steam_api_key=gaming.get('steam_api_key', ''),
            steam_id=gaming.get('steam_id', '')
        )
    else:
        tracker = SteamTracker()

    print("Steam Tracker Test")
    print("=" * 40)
    while True:
        state = tracker.get_gaming_state()
        print(f"\r{json.dumps(state)}", end='', flush=True)
        time.sleep(5)
