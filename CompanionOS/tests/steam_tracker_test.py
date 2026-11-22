"""Deterministic local-detection regressions; no games, WinAPI, or network used.

Run: python -m unittest discover -s CompanionOS/tests -p steam_tracker_test.py -v
"""

import ctypes
import importlib.util
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch


SOURCE = Path(__file__).resolve().parents[1] / "python" / "steam_tracker.py"
SPEC = importlib.util.spec_from_file_location("tested_steam_tracker", SOURCE)
steam = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(steam)


def process(pid, name, exe="", cmdline=None):
    return SimpleNamespace(info={"pid": pid, "name": name, "exe": exe},
                           cmdline=Mock(return_value=cmdline or []))


class SteamTrackerTests(unittest.TestCase):
    def setUp(self):
        self.network = patch.object(steam.requests.sessions.Session, "request",
                                    side_effect=AssertionError("Network disabled"))
        self.network.start()
        self.addCleanup(self.network.stop)
        self.tracker = steam.SteamTracker()
        self.foreground = patch.object(self.tracker, "_foreground_process_id", return_value=0).start()
        self.gpu = patch.object(self.tracker, "_process_has_gpu_dlls", return_value=False).start()
        self.processes = patch.object(steam.psutil, "process_iter", return_value=[]).start()
        self.title = patch.object(self.tracker, "_foreground_window_title", return_value="Work document").start()
        self.addCleanup(patch.stopall)

    def detect(self, *processes, foreground=0):
        self.processes.return_value = list(processes)
        self.foreground.return_value = foreground
        return self.tracker.detect_local_game()

    def test_mixed_case_store_paths(self):
        paths = {
            r"D:\sTeAm\StEaMaPpS\CoMmOn\ELDEN_RING\Game\eldenring.exe": "Elden Ring",
            r"D:\ePiC gAmEs\Cool_Game\Binaries\Win64\game.exe": "Cool Game",
            r"D:\XBOXGAMES\Space_Game\Content\game.exe": "Space Game",
            r"D:\gOg GaLaXy\gAmEs\Old_Game\bin\game.exe": "Old Game",
            "D:/STEAM/SteamApps/Common/Forward_Game/bin/play.exe": "Forward Game",
            r"\\server\share\SteamApps\Common\Network_Game\play.exe": "Network Game",
        }
        for path, title in paths.items():
            with self.subTest(path=path):
                self.assertEqual(self.tracker._title_from_exe_path(path), title)
                self.assertEqual(self.detect(process(123, "play.exe", path)), title)
        self.gpu.assert_not_called()

    def test_store_markers_must_be_complete_directory_components(self):
        for path in (r"D:\NotSteamApps\Common\Foo\play.exe",
                     r"D:\Epic Games Backup\Foo\play.exe",
                     r"D:\XboxGames-old\Foo\play.exe",
                     r"D:\SteamApps\Common\play.exe"):
            with self.subTest(path=path):
                self.assertIsNone(self.tracker._game_install_title(path))
                self.assertEqual(self.detect(process(123, "play.exe", path)), "")

    def test_protected_store_game_does_not_require_module_access(self):
        self.gpu.side_effect = steam.psutil.AccessDenied(123)
        self.assertEqual(self.detect(process(123, "protected.exe",
                         r"D:\SteamApps\Common\Protected_Game\bin\protected.exe")), "Protected Game")
        self.gpu.assert_not_called()

    def test_program_files_alone_is_not_game_evidence(self):
        self.assertEqual(self.detect(process(123, "drawing.exe", r"C:\Program Files\Drawing\drawing.exe")), "")

    def test_arbitrary_foreground_title_is_not_a_game(self):
        self.title.return_value = "Quarterly budget spreadsheet"
        self.assertEqual(self.detect(process(123, "accounting.exe", r"C:\Tools\accounting.exe"), foreground=123), "")
        self.assertEqual(self.detect(), "")
        self.title.assert_not_called()

    def test_known_game_list_and_case_insensitivity_preserved(self):
        self.assertEqual(self.detect(process(123, "GENSHINIMPACT.EXE")), "Genshin Impact")
        self.assertEqual(self.detect(process(123, "cs2.exe")), "Counter-Strike 2")
        self.gpu.assert_not_called()

    def test_launchers_and_gpu_blocklist_stay_excluded(self):
        self.gpu.return_value = True
        for name in ("steam.exe", "EpicGamesLauncher.exe", "chrome.exe", "code.exe"):
            with self.subTest(name=name):
                self.assertEqual(self.detect(process(123, name, "D:\\SteamApps\\Common\\Tools\\" + name)), "")
        self.gpu.assert_not_called()

    def test_java_without_minecraft_evidence_is_not_minecraft(self):
        self.gpu.return_value = True
        app = process(123, "javaw.exe", r"C:\Java\bin\javaw.exe", ["javaw", "-jar", "business.jar"])
        self.assertEqual(self.detect(app, foreground=123), "")

    def test_minecraft_main_class_is_evidence_without_logging_tokens(self):
        app = process(123, "JAVAW.EXE", r"C:\Java\bin\javaw.exe",
                      ["javaw", "net.minecraft.client.main.Main", "--accessToken", "PRIVATE-TEST-TOKEN"])
        with patch("builtins.print") as output:
            self.assertEqual(self.detect(app), "Minecraft")
        output.assert_not_called()

    def test_minecraft_game_directory_is_evidence(self):
        app = process(123, "javaw.exe", r"C:\Java\bin\javaw.exe",
                      ["javaw", "--gameDir", r"C:\Users\Player\.minecraft"])
        self.assertEqual(self.detect(app), "Minecraft")

    def test_minecraft_runtime_path_works_with_denied_command_line(self):
        app = process(123, "javaw.exe", r"C:\Games\Minecraft\runtime\bin\javaw.exe")
        app.cmdline.side_effect = steam.psutil.AccessDenied(123)
        self.assertEqual(self.detect(app), "Minecraft")
        app.cmdline.assert_not_called()

    def test_denied_or_disappearing_java_is_not_guessed(self):
        for error in (steam.psutil.AccessDenied(123), steam.psutil.NoSuchProcess(123)):
            app = process(123, "javaw.exe", r"C:\Java\bin\javaw.exe")
            app.cmdline.side_effect = error
            self.assertEqual(self.detect(app), "")

    def test_foreground_game_beats_earlier_known_game(self):
        first = process(123, "cs2.exe")
        second = process(456, "unknown.exe", r"D:\XboxGames\Foreground_Game\Content\game.exe")
        self.assertEqual(self.detect(first, second, foreground=456), "Foreground Game")

    def test_foreground_non_game_does_not_beat_known_game(self):
        self.assertEqual(self.detect(process(123, "cs2.exe"), process(456, "excel.exe"), foreground=456), "Counter-Strike 2")

    def test_known_game_priority_preserved_without_foreground(self):
        self.assertEqual(self.detect(process(123, "play.exe", r"D:\XboxGames\Other\play.exe"),
                                     process(456, "cs2.exe")), "Counter-Strike 2")

    def test_gpu_fallback_uses_windows_basename_on_any_host(self):
        self.gpu.return_value = True
        self.assertEqual(self.detect(process(123, "foo.exe", r"D:\Custom\My_Game-Win64-Shipping.exe")), "My Game")

    def test_foreground_gpu_only_app_never_beats_strong_game_evidence(self):
        self.gpu.return_value = True
        weak = process(789, "gpu_utility.exe", r"C:\Tools\gpu_utility.exe")
        strong = [(process(123, "cs2.exe"), "Counter-Strike 2"),
                  (process(456, "play.exe", r"D:\XboxGames\Installed_Game\play.exe"), "Installed Game")]
        for candidate, expected in strong:
            with self.subTest(expected=expected):
                self.assertEqual(self.detect(weak, candidate, foreground=789), expected)

    @staticmethod
    def fake_module_api(success=True):
        calls = []
        enum_type = ctypes.CFUNCTYPE(
            ctypes.wintypes.BOOL, ctypes.wintypes.HANDLE,
            ctypes.POINTER(ctypes.c_void_p), ctypes.wintypes.DWORD,
            ctypes.POINTER(ctypes.wintypes.DWORD), ctypes.wintypes.DWORD)

        @enum_type
        def enumerate_modules(handle, modules, capacity, needed, flags):
            calls.append((handle, capacity, flags))
            if not success:
                return False
            modules[0] = 0xBEEF
            needed[0] = ctypes.sizeof(ctypes.c_void_p)
            return True

        def module_name(handle, module, buffer, capacity):
            buffer.value = b'D3D11.DLL'
            return len(buffer.value)

        kernel = SimpleNamespace(OpenProcess=Mock(return_value=0xABCD), CloseHandle=Mock())
        psapi = SimpleNamespace(EnumProcessModulesEx=enumerate_modules,
                                GetModuleBaseNameA=Mock(side_effect=module_name))
        return SimpleNamespace(kernel32=kernel, psapi=psapi), calls

    def test_typed_module_enum_requires_array_and_finds_gpu_dll_and_closes_handle(self):
        api, calls = self.fake_module_api()
        modules = (ctypes.c_void_p * 2)()
        needed = ctypes.wintypes.DWORD()
        # Reproduce the original conversion error at the actual ctypes boundary.
        with self.assertRaises((ctypes.ArgumentError, TypeError)):
            api.psapi.EnumProcessModulesEx(0xABCD, ctypes.byref(modules),
                                          ctypes.sizeof(modules), ctypes.byref(needed), 3)
        self.assertEqual(calls, [])
        with patch.object(steam.ctypes, "windll", api, create=True):
            # Bypass the per-instance GPU stub to exercise the real scanner.
            self.assertTrue(steam.SteamTracker._process_has_gpu_dlls(self.tracker, 123))
        self.assertEqual(calls, [(0xABCD, ctypes.sizeof(ctypes.c_void_p) * 1024, 3)])
        api.psapi.GetModuleBaseNameA.assert_called_once()
        self.assertEqual(api.psapi.GetModuleBaseNameA.call_args.args[1].value, 0xBEEF)
        api.kernel32.CloseHandle.assert_called_once_with(0xABCD)

    def test_failed_typed_module_enum_still_closes_handle(self):
        api, calls = self.fake_module_api(success=False)
        with patch.object(steam.ctypes, "windll", api, create=True):
            self.assertFalse(steam.SteamTracker._process_has_gpu_dlls(self.tracker, 123))
        self.assertEqual(len(calls), 1)
        api.psapi.GetModuleBaseNameA.assert_not_called()
        api.kernel32.CloseHandle.assert_called_once_with(0xABCD)

    def test_foreground_pid_winapi_is_mocked(self):
        user32 = SimpleNamespace(GetForegroundWindow=Mock(return_value=0x100000010),
                                 GetWindowThreadProcessId=Mock())
        def assign_pid(hwnd, address):
            self.assertEqual(hwnd, 0x100000010)
            ctypes.cast(address, ctypes.POINTER(ctypes.wintypes.DWORD))[0] = 456
            return 789
        user32.GetWindowThreadProcessId.side_effect = assign_pid
        with patch.object(steam.os, "name", "nt"), patch.object(steam.ctypes, "windll", SimpleNamespace(user32=user32), create=True):
            self.assertEqual(steam.SteamTracker._foreground_process_id(), 456)
            user32.GetForegroundWindow.return_value = 0
            self.assertEqual(steam.SteamTracker._foreground_process_id(), 0)
            user32.GetForegroundWindow.side_effect = OSError("unavailable")
            self.assertEqual(steam.SteamTracker._foreground_process_id(), 0)

    def test_session_start_continue_and_stop_without_network(self):
        game = process(123, "cs2.exe")
        with patch.object(steam.time, "time", return_value=1000):
            self.processes.return_value = [game]
            state = self.tracker.get_gaming_state()
        self.assertTrue(state["active"])
        self.assertEqual(state["status"], "Playing")
        with patch.object(steam.time, "time", return_value=1065):
            self.assertEqual(self.tracker.get_gaming_state()["session"], "01:05")
        self.processes.return_value = []
        self.assertFalse(self.tracker.get_gaming_state()["active"])
        self.assertEqual(self.tracker.last_game, "Counter-Strike 2")
        self.assertIsNone(self.tracker.session_start)


if __name__ == "__main__":
    unittest.main()
