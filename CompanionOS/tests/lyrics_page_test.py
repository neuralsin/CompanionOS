"""Cross-client tile contract and timestamp metadata; no running controller needed."""
import ast
from pathlib import Path
import re
from types import SimpleNamespace
import unittest

ROOT = Path(__file__).resolve().parents[1]


class LyricsPageTests(unittest.TestCase):
    def test_all_remote_maps_match_firmware(self):
        source = (ROOT / 'arduino/CompanionOS_Main/globals.h').read_text(encoding='utf-8')
        enum = source.split('enum AppState {', 1)[1].split('};', 1)[0]
        states = re.findall(r'^\s*(STATE_\w+)', enum, re.M)[:-1]
        self.assertEqual(states[2], 'STATE_LYRICS')
        self.assertEqual(len(states), 20)
        for file in ('python/templates/web_remote.html',):
            source = (ROOT / file).read_text(encoding='utf-8')
            block = source.split('const PAGES = [', 1)[1].split('];', 1)[0]
            entries = re.findall(r"id: (\d+), name: '([^']+)'", block)
            self.assertEqual([int(n) for n, _ in entries], list(range(20)))
            self.assertEqual(entries[2][1], 'Lyrics')
            self.assertIn('PC Dashboard', entries[13][1])
            self.assertIn('RuView', entries[14][1])
            self.assertIn('Dr. Hack', entries[17][1])
        android = (ROOT / 'android/CompanionApp/app/src/main/assets/www/app.js').read_text(encoding='utf-8')
        android_pages = ast.literal_eval(android.split('const pages = ', 1)[1].split(';', 1)[0])
        self.assertEqual(len(android_pages), len(states))
        self.assertEqual([p[0] for p in android_pages[-2:]], ['Calendar', 'Timetable'])
        tree = ast.parse((ROOT / 'python/web_remote.py').read_text(encoding='utf-8'))
        pages = next(ast.literal_eval(n.value) for n in tree.body if isinstance(n, ast.Assign)
                     and any(isinstance(t, ast.Name) and t.id == 'PAGE_NAMES' for t in n.targets))
        self.assertEqual(len(pages), len(states))
        self.assertEqual(pages[2], 'Lyrics')

    def test_controller_sends_future_line_timestamp(self):
        tree = ast.parse((ROOT / 'python/companion_controller.py').read_text(encoding='utf-8'))
        function = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == 'lyrics_frame')
        ns = {}
        exec(compile(ast.Module(body=[function], type_ignores=[]), 'lyrics', 'exec'), ns)
        lines = [{'time': 1000, 'words': 'First'}, {'time': 2500, 'words': 'Next'}]
        self.assertEqual(ns['lyrics_frame'](lines, 0)['next_at'], 1000)
        frame = ns['lyrics_frame'](lines, 1000)
        self.assertEqual((frame['line_at'], frame['next_at']), (1000, 2500))
        self.assertEqual(ns['lyrics_frame'](lines, 2500)['next_at'], -1)

    def test_no_active_player_is_not_a_network_error(self):
        tree = ast.parse((ROOT / 'python/spotify_integration.py').read_text(encoding='utf-8'))
        cls = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == 'SpotifyIntegration')
        function = next(n for n in cls.body if isinstance(n, ast.FunctionDef) and n.name == 'get_current_track')
        ns = {'print': lambda *args: None}
        exec(compile(ast.Module(body=[function], type_ignores=[]), 'spotify', 'exec'), ns)
        service = SimpleNamespace(enabled=True, client=SimpleNamespace(current_playback=lambda: None))
        self.assertIsNone(ns['get_current_track'](service))
        self.assertTrue(service.playback_sample_valid)
        def failure():
            raise TimeoutError('mock')
        service.client.current_playback = failure
        self.assertIsNone(ns['get_current_track'](service))
        self.assertFalse(service.playback_sample_valid)


if __name__ == '__main__':
    unittest.main()
