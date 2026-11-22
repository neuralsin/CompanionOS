"""Run real PC lyric methods without starting OAuth, UDP, or background services."""
import ast
from collections import deque
import json
from pathlib import Path
from types import SimpleNamespace
import unittest

ROOT = Path(__file__).resolve().parents[1]


class LyricsTests(unittest.TestCase):
    def setUp(self):
        tree = ast.parse((ROOT / 'python/spotify_integration.py').read_text(encoding='utf-8'))
        cls = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == 'SpotifyIntegration')
        method = next(n for n in cls.body if isinstance(n, ast.FunctionDef) and n.name == 'get_lyrics')
        self.responses, self.calls = deque(), []

        def get(url, **kwargs):
            self.calls.append((url, kwargs))
            status, data = self.responses.popleft()
            return SimpleNamespace(status_code=status, json=lambda: data)

        ns = {'requests': SimpleNamespace(get=get)}
        exec(compile(ast.Module(body=[method], type_ignores=[]), 'spotify_integration', 'exec'), ns)
        self.fetch = lambda invoke=ns['get_lyrics']: invoke(None, 'Track', 'Artist')
        tree = ast.parse((ROOT / 'python/companion_controller.py').read_text(encoding='utf-8'))
        function = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == 'lyrics_frame')
        ns = {}
        exec(compile(ast.Module(body=[function], type_ignores=[]), 'controller', 'exec'), ns)
        self.frame = ns['lyrics_frame']

    def test_repeated_timestamps_offset_and_real_breaks(self):
        self.responses.append((200, {'syncedLyrics': '[offset:+500]\n[00:02.00][00:08.00]Verse\n[00:04.00]'}))
        lines = self.fetch()
        self.assertEqual(lines, [{'time': 1500, 'words': 'Verse'}, {'time': 3500, 'words': ''},
                                 {'time': 7500, 'words': 'Verse'}])
        self.assertEqual(self.frame(lines, 4000)['line1'], '')

    def test_plain_is_real_text_and_explicitly_unsynced(self):
        self.responses.append((200, {'syncedLyrics': None, 'plainLyrics': 'First line\n\nSecond line'}))
        lines = self.fetch()
        self.assertEqual([n['words'] for n in lines], ['First line', 'Second line'])
        self.assertFalse(self.frame(lines, 0)['synced'])
        self.assertEqual(self.frame(lines, 6000)['line1'], 'Second line')

    def test_not_found_is_not_instrumental(self):
        self.responses.extend([(404, {}), (200, [])])
        frame = self.frame(self.fetch(), 0)
        self.assertEqual(frame['status'], 'Lyrics unavailable')
        self.assertEqual(frame['line1'], '')

    def test_enhanced_timing_tags_are_not_printed(self):
        self.responses.append((200, {'syncedLyrics': '[00:01]<00:01>First <00:02>word'}))
        self.assertEqual(self.fetch()[0]['words'], 'First word')

    def test_only_confirmed_instrumental_is_labeled(self):
        self.responses.append((200, {'instrumental': True}))
        self.assertEqual(self.frame(self.fetch(), 0)['status'], 'Instrumental')

    def test_search_never_uses_an_unrelated_song(self):
        self.responses.extend([(404, {}), (200, [
            {'artistName': 'Other artist', 'trackName': 'Track', 'syncedLyrics': '[00:00]Wrong'},
            {'artistName': 'Artist', 'trackName': 'Other song', 'syncedLyrics': '[00:00]Wrong'},
            {'artistName': 'Artist', 'trackName': 'Track', 'syncedLyrics': '[00:00]Correct'},
        ])])
        self.assertEqual(self.fetch()[0]['words'], 'Correct')
        self.assertTrue(all('Authorization' not in kwargs.get('headers', {}) for _, kwargs in self.calls))

    def test_intro_does_not_show_first_verse_early(self):
        lines = [{'time': 5000, 'words': 'First'}, {'time': 9000, 'words': 'Second'}]
        self.assertEqual(self.frame(lines, 4999)['line1'], '')
        self.assertEqual(self.frame(lines, 4999)['line2'], 'First')
        self.assertEqual(self.frame(lines, 5000)['line1'], 'First')
        self.assertEqual(self.frame(lines, 10000)['line1'], 'Second')
        self.assertEqual(self.frame(lines, 5000)['line1'], 'First')
        self.assertEqual(self.frame(lines, 5000), self.frame(lines, 5000))

    def test_empty_frame_clears_old_lines(self):
        self.assertEqual(self.frame([], 0)['line1'], '')
        self.assertEqual(self.frame([], 0)['line2'], '')

    def test_packet_and_utf8_bounds(self):
        words = '\U0001f3b5' * 500
        frame = self.frame([{'time': 0, 'words': words}, {'time': 1, 'words': words},
                            {'time': 2, 'words': words}], 1)
        for key in ('prev', 'line1', 'line2', 'status'):
            self.assertLessEqual(len(frame[key].encode('utf-8')), 192)
        self.assertLess(len(('LYRICS:' + json.dumps(frame, ensure_ascii=False)).encode('utf-8')), 2048)


if __name__ == '__main__':
    unittest.main()
