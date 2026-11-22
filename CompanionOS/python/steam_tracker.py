#!/usr/bin/env python3
"""
═══════════════════════════════════════════════════════════
  STEAM TRACKER — V7 CompanionOS Gaming Module

  Triple-source game tracking:
  1. Steam Web API — game details, achievements, friends
  2. GPU DLL detection — like NVIDIA GeForce Experience,
     scans for any process using DirectX/Vulkan/OpenGL
  3. Directory heuristics — Steam/Epic/Xbox/GOG paths
═══════════════════════════════════════════════════════════
"""

import os
import ntpath
import json
import time
import ctypes
import ctypes.wintypes
import psutil
import requests

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# Windows API constants for module enumeration
PROCESS_QUERY_INFORMATION = 0x0400
PROCESS_VM_READ = 0x0010
LIST_MODULES_ALL = 0x03


class SteamTracker:
    """Triple-source game tracking: Steam API + GPU DLL scan + directory heuristics."""

    # Common game executable names → friendly titles (fast lookup)
    KNOWN_GAMES = {
        'csgo.exe': 'Counter-Strike 2',
        'cs2.exe': 'Counter-Strike 2',
        'dota2.exe': 'Dota 2',
        'VALORANT-Win64-Shipping.exe': 'VALORANT',
        'valorant.exe': 'VALORANT',
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
        'Baldur.exe': "Baldur's Gate 3",
        'bg3.exe': "Baldur's Gate 3",
        'r5apex.exe': 'Apex Legends',
        'destiny2.exe': 'Destiny 2',
        'witcher3.exe': 'The Witcher 3',
        'HogwartsLegacy.exe': 'Hogwarts Legacy',
        'Palworld-Win64-Shipping.exe': 'Palworld',
        'starfield.exe': 'Starfield',
        'sekiro.exe': 'Sekiro',
        'League of Legends.exe': 'League of Legends',
        'LeagueClient.exe': 'League of Legends',
        'RobloxPlayerBeta.exe': 'Roblox',
        'TslGame.exe': 'PUBG: BATTLEGROUNDS',
        'cod.exe': 'Call of Duty',
        'BlackOpsColdWar.exe': 'Call of Duty',
        'ModernWarfare.exe': 'Call of Duty',
        'eurotrucks2.exe': 'Euro Truck Simulator 2',
        'FarmingSimulator2022Game.exe': 'Farming Simulator 22',
        'rdr2.exe': 'Red Dead Redemption 2',
        'DOOMEternalx64vk.exe': 'DOOM Eternal',
        'Fallout4.exe': 'Fallout 4',
        'SkyrimSE.exe': 'Skyrim',
        'CivilizationVI.exe': 'Civilization VI',
        'deadbydaylight-win64-shipping.exe': 'Dead by Daylight',
        'warframe.x64.exe': 'Warframe',
        'RustClient.exe': 'Rust',
        'aces.exe': 'War Thunder',
        'ForzaHorizon5.exe': 'Forza Horizon 5',
        'HaloInfinite.exe': 'Halo Infinite',
        'NoMansSky.exe': 'No Man\'s Sky',
    }
    KNOWN_GAMES_LOWER = {name.lower(): title for name, title in KNOWN_GAMES.items()}

    # GPU-rendering DLLs (if a process loaded these, it's using the GPU)
    GPU_DLLS = {
        b'd3d9.dll', b'd3d10.dll', b'd3d11.dll', b'd3d12.dll',
        b'dxgi.dll', b'vulkan-1.dll',
    }

    # Non-game processes that also use GPU — skip these
    NON_GAME_GPU_APPS = {
        # Browsers
        'chrome.exe', 'msedge.exe', 'firefox.exe', 'opera.exe', 'brave.exe',
        'vivaldi.exe', 'arc.exe', 'chromium.exe',
        # System / Desktop
        'dwm.exe', 'explorer.exe', 'shellexperiencehost.exe',
        'searchhost.exe', 'startmenuexperiencehost.exe',
        'systemsettings.exe', 'textinputhost.exe', 'lockapp.exe',
        'widgets.exe', 'windowsterminal.exe', 'wt.exe', 'applicationframehost.exe',
        'shellhost.exe', 'crossdeviceresume.exe',
        # Dev tools
        'code.exe', 'devenv.exe', 'rider64.exe', 'clion64.exe',
        'idea64.exe', 'pycharm64.exe', 'androidstudio64.exe',
        'cursor.exe', 'antigravity ide.exe', 'antigravity.exe',
        # Communication
        'discord.exe', 'slack.exe', 'teams.exe', 'zoom.exe',
        'telegram.exe', 'whatsapp.exe', 'signal.exe',
        # Media
        'spotify.exe', 'vlc.exe', 'mpc-hc64.exe', 'obs64.exe',
        'obs32.exe', 'streamlabs obs.exe',
        # Launchers (NOT games themselves)
        'steam.exe', 'steamwebhelper.exe', 'steamservice.exe',
        'epicgameslauncher.exe', 'unrealcefsubprocess.exe',
        'eadesktop.exe', 'eabackgroundservice.exe',
        'gog galaxy.exe', 'galaxyclient.exe',
        'battle.net.exe', 'agent.exe',
        'ubisoftconnect.exe', 'upc.exe',
        'origin.exe', 'originwebhelperservice.exe',
        'riotclientservices.exe', 'riotclientux.exe',
        'playnite.desktopapp.exe', 'playnite.fullscreenapp.exe',
        # NVIDIA / AMD / GPU tools
        'nvidia share.exe', 'nvcontainer.exe', 'nvdisplay.container.exe',
        'nvcamera.exe', 'nvsphelper64.exe', 'nvbackend.exe',
        'geforce experience.exe', 'nvspc.dll',
        'amd software.exe', 'amddvr.exe', 'amdow.exe',
        'radeonoverlay.exe', 'radeonsoftware.exe',
        # Windows utilities
        'taskmgr.exe', 'mspaint.exe', 'snippingtool.exe',
        'photos.exe', 'video.ui.exe', 'gamebar.exe',
        'gamebarpresencewriter.exe', 'gamebarftserver.exe',
        'xboxgamebarsvc.exe', 'securityhealthsystray.exe',
        # Misc
        'wallpaperengine.exe', 'lively.exe', 'rainmeter.exe',
        'powertoys.exe', 'windowspackagemanagerserver.exe',
        'msedgewebview2.exe', 'cefsharp.browsersubprocess.exe',
        'crashhandler64.exe', 'crashreporter.exe', 'ds4windows.exe',
        'nahimicsvc64.exe', 'nahimicsvc32.exe', 'cloudflare.exe', 'cloudflare warp.exe',
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

    # ── GPU-Based Game Detection (NVIDIA-style) ───────────
    def _process_has_gpu_dlls(self, pid):
        """
        Check if a process has loaded DirectX/Vulkan DLLs.
        This is how NVIDIA GeForce Experience detects games:
        any process actively rendering with D3D11/D3D12/Vulkan = game.
        """
        try:
            kernel32 = ctypes.windll.kernel32
            psapi = ctypes.windll.psapi

            # Define argtypes to prevent 64-bit handle truncation in ctypes
            kernel32.OpenProcess.argtypes = [ctypes.wintypes.DWORD, ctypes.wintypes.BOOL, ctypes.wintypes.DWORD]
            kernel32.OpenProcess.restype = ctypes.wintypes.HANDLE
            kernel32.CloseHandle.argtypes = [ctypes.wintypes.HANDLE]
            kernel32.CloseHandle.restype = ctypes.wintypes.BOOL
            psapi.EnumProcessModulesEx.restype = ctypes.wintypes.BOOL
            psapi.EnumProcessModulesEx.argtypes = [
                ctypes.wintypes.HANDLE, ctypes.POINTER(ctypes.c_void_p),
                ctypes.wintypes.DWORD, ctypes.POINTER(ctypes.wintypes.DWORD),
                ctypes.wintypes.DWORD
            ]
            
            psapi.GetModuleBaseNameA.argtypes = [
                ctypes.wintypes.HANDLE, ctypes.c_void_p,
                ctypes.c_char_p, ctypes.wintypes.DWORD
            ]
            psapi.GetModuleBaseNameA.restype = ctypes.wintypes.DWORD

            hProcess = kernel32.OpenProcess(
                PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, pid
            )
            if not hProcess:
                return False

            try:
                hMods = (ctypes.c_void_p * 1024)()
                cbNeeded = ctypes.c_ulong()

                success = psapi.EnumProcessModulesEx(
                    hProcess, hMods,
                    ctypes.sizeof(hMods), ctypes.byref(cbNeeded),
                    LIST_MODULES_ALL
                )
                if not success:
                    return False

                num_mods = cbNeeded.value // ctypes.sizeof(ctypes.c_void_p)
                for i in range(min(num_mods, 1024)):
                    modname = ctypes.create_string_buffer(260)
                    psapi.GetModuleBaseNameA(
                        hProcess, ctypes.c_void_p(hMods[i]), modname, 260
                    )
                    if modname.value.lower() in self.GPU_DLLS:
                        return True
            finally:
                kernel32.CloseHandle(hProcess)
        except Exception:
            pass
        return False

    @staticmethod
    def _game_install_title(exe_path):
        """Recognize complete game-folder components, independent of DLL access."""
        if not exe_path:
            return None
        parts = ntpath.normpath(exe_path).split('\\')
        lowered = [part.lower() for part in parts]
        roots = [('steamapps', 'common'), ('epic games',),
                 ('xboxgames',), ('gog galaxy', 'games')]
        for root in roots:
            for index in range(len(parts) - len(root) - 1):
                if tuple(lowered[index:index + len(root)]) == root:
                    folder = parts[index + len(root)]
                    return folder.replace('_', ' ').title()
        return None

    @staticmethod
    def _is_minecraft_java(proc, exe_path):
        """Use local evidence only; command lines may contain private tokens."""
        def minecraft_path(value):
            return any(part.lower() in {'.minecraft', 'minecraft'}
                       for part in ntpath.normpath(value).split('\\'))

        if minecraft_path(exe_path):
            return True
        try:
            return any(
                arg.lower().startswith('net.minecraft.') or
                arg.lower() in {'net.fabricmc.loader.impl.launch.knot.knotclient',
                                'net.fabricmc.loader.launch.knot.knotclient'} or
                minecraft_path(arg)
                for arg in proc.cmdline()
            )
        except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess):
            return False

    def _title_from_exe_path(self, exe_path):
        """Extract a clean game title from the executable path."""
        if not exe_path:
            return None
        installed_title = self._game_install_title(exe_path)
        if installed_title:
            return installed_title
        # Generic: Program Files game folder
        parts = ntpath.normpath(exe_path).split('\\')
        if any(part.lower() in {'program files', 'program files (x86)'} for part in parts):
            # Find the folder after "Program Files" or "Program Files (x86)"
            for i, p in enumerate(parts):
                if p.lower() in {'program files', 'program files (x86)'} and i + 2 < len(parts):
                    return parts[i + 1].replace('_', ' ').title()

        # Fallback: use the exe name without extension
        basename = ntpath.splitext(ntpath.basename(exe_path))[0]
        # Clean up common suffixes
        for suffix in ['-Win64-Shipping', '-Win32-Shipping', 'Client',
                       'Launcher', '_BE', '-EAC', '_dx11', '_dx12']:
            basename = basename.replace(suffix, '')
        return basename.replace('_', ' ').replace('-', ' ').title().strip()

    def detect_local_game(self):
        """
        Detect games from known names, trusted paths, or GPU module evidence.
        
        Strategy (in priority order):
        Prefer an evidenced foreground game, then known names, trusted install
        paths, and GPU DLL candidates. Java requires Minecraft-specific evidence.
        """
        candidates = []
        foreground_pid = self._foreground_process_id()

        for proc in psutil.process_iter(['pid', 'name', 'exe']):
            try:
                name = (proc.info.get('name') or '').strip()
                exe = proc.info.get('exe') or ''
                pid = proc.info.get('pid', 0)
                name_lower = name.lower()

                # Skip empty / system
                if not name or pid < 100:
                    continue

                # Known names do not need module enumeration.
                if name_lower in self.KNOWN_GAMES_LOWER:
                    title = self.KNOWN_GAMES_LOWER[name_lower]
                    if name_lower == 'javaw.exe' and not self._is_minecraft_java(proc, exe):
                        continue
                    if title:
                        candidates.append((0, pid, title))
                    continue  # Skip launchers (None entries)

                # Skip known non-game GPU apps
                if name_lower in self.NON_GAME_GPU_APPS:
                    continue

                # A trusted installation path also works for protected processes
                # whose modules cannot be enumerated.
                installed_title = self._game_install_title(exe)
                if installed_title:
                    candidates.append((1, pid, installed_title))
                    continue

                # GPU rendering is the remaining evidence-based fallback.
                if self._process_has_gpu_dlls(pid):
                    # This process is rendering with DirectX/Vulkan
                    title = self._title_from_exe_path(exe or name)
                    if title:
                        candidates.append((2, pid, title))

            except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess):
                continue

        if candidates:
            # Strong game evidence outranks GPU-only applications. Within each
            # group, prefer the foreground candidate; ties retain process order.
            priority, pid, title = min(candidates, key=lambda item: (
                item[0] >= 2, item[1] != foreground_pid, item[0]))
            return title if priority == 0 else title[:23]

        return ''

    @staticmethod
    def _foreground_process_id():
        if os.name != 'nt':
            return 0
        try:
            user32 = ctypes.windll.user32
            user32.GetForegroundWindow.restype = ctypes.wintypes.HWND
            user32.GetWindowThreadProcessId.argtypes = [
                ctypes.wintypes.HWND, ctypes.POINTER(ctypes.wintypes.DWORD)]
            user32.GetWindowThreadProcessId.restype = ctypes.wintypes.DWORD
            hwnd = user32.GetForegroundWindow()
            pid = ctypes.wintypes.DWORD()
            if hwnd:
                user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
            return pid.value
        except Exception:
            return 0

    def _foreground_window_title(self):
        """Read the active title; a title alone is not evidence of a game."""
        if os.name != 'nt':
            return ''
        try:
            hwnd = ctypes.windll.user32.GetForegroundWindow()
            if not hwnd:
                return ''
            length = ctypes.windll.user32.GetWindowTextLengthW(hwnd)
            if length <= 0 or length > 256:
                return ''
            buffer = ctypes.create_unicode_buffer(length + 1)
            ctypes.windll.user32.GetWindowTextW(hwnd, buffer, length + 1)
            title = buffer.value.strip()
            if not title:
                return ''

            lowered = title.lower()
            blocked = [
                'chrome', 'edge', 'firefox', 'visual studio code', 'cursor',
                'discord', 'spotify', 'steam', 'settings', 'terminal',
                'file explorer', 'windows powershell', 'antigravity'
            ]
            if any(word in lowered for word in blocked):
                return ''
            return title
        except Exception:
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
        """
        V7 Cover Art Waterfall:
        1. Steam Store Search (free, no key)
        2. IGDB via Twitch OAuth (needs client_id + client_secret)
        3. None (ESP shows placeholder gradient)
        """
        # Tier 1: Steam Store Search (free, no API key)
        url = self._steam_cover_search(title)
        if url:
            return url

        # Tier 2: IGDB via Twitch OAuth
        url = self._igdb_cover_search(title)
        if url:
            return url

        return None

    def _steam_cover_search(self, title):
        """Tier 1: Steam Store search (no key required)."""
        try:
            url = "https://store.steampowered.com/api/storesearch/"
            params = {"term": title, "l": "english", "cc": "US"}
            resp = requests.get(url, params=params, timeout=5)
            if resp.status_code == 200:
                data = resp.json()
                if data.get('total', 0) > 0 and 'items' in data:
                    img_url = data['items'][0].get('tiny_image')
                    if img_url:
                        if img_url.startswith('//'):
                            return 'https:' + img_url
                        return img_url
        except Exception as e:
            print(f"Steam cover search error for '{title}': {e}")
        return None

    def _igdb_cover_search(self, title):
        """
        Tier 2: IGDB cover art via Twitch OAuth.
        Requires TWITCH_CLIENT_ID and TWITCH_CLIENT_SECRET env vars.
        GAP-02: These are prompted during install.py setup flow.
        """
        client_id = os.environ.get('TWITCH_CLIENT_ID', '')
        client_secret = os.environ.get('TWITCH_CLIENT_SECRET', '')
        if not client_id or not client_secret:
            return None

        try:
            # Get Twitch OAuth token (cached for 60 days)
            if not hasattr(self, '_igdb_token') or not self._igdb_token:
                token_resp = requests.post(
                    'https://id.twitch.tv/oauth2/token',
                    params={
                        'client_id': client_id,
                        'client_secret': client_secret,
                        'grant_type': 'client_credentials'
                    },
                    timeout=10
                )
                if token_resp.status_code == 200:
                    self._igdb_token = token_resp.json().get('access_token', '')
                else:
                    return None

            # Query IGDB for game cover
            headers = {
                'Client-ID': client_id,
                'Authorization': f'Bearer {self._igdb_token}'
            }
            body = f'search "{title}"; fields cover.url; limit 1;'
            resp = requests.post(
                'https://api.igdb.com/v4/games',
                headers=headers,
                data=body,
                timeout=10
            )
            if resp.status_code == 200:
                games = resp.json()
                if games and 'cover' in games[0]:
                    cover_url = games[0]['cover'].get('url', '')
                    if cover_url:
                        # IGDB returns //images.igdb.com/... → convert to https, upscale
                        cover_url = cover_url.replace('t_thumb', 't_cover_big')
                        if cover_url.startswith('//'):
                            cover_url = 'https:' + cover_url
                        return cover_url
        except Exception as e:
            print(f"IGDB cover search error for '{title}': {e}")
        return None

    def get_recently_played_formatted(self):
        """
        Get recently played games formatted for ESP recent games list.
        Returns list of {"name": str, "time": int (minutes)} dicts.
        """
        recent = self.get_recently_played()
        result = []
        for game in recent[:3]:
            result.append({
                'name': game.get('name', 'Unknown')[:23],
                'time': game.get('playtime_2weeks', 0)  # minutes
            })
        return result

    def get_gaming_state(self):
        """
        V7: Get complete gaming state for ESP display.
        Returns dict ready for JSON serialization.
        Now includes 'recent' array for ESP recent games list.
        """
        result = {
            'title': '',
            'session': '00:00',
            'achieve': 0,
            'friends': 0,
            'active': False,
            'status': 'Offline',
            'recent': []  # V7: recent games for gaming dashboard
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

            # V7: Get recently played for gaming dashboard
            try:
                result['recent'] = self.get_recently_played_formatted()
            except Exception:
                result['recent'] = []

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
