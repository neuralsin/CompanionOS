"""Exercise the real optional-feed demand functions without starting services."""
import ast
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]


class StopLoop(BaseException):
    pass


class OptionalDemandTests(unittest.TestCase):
    def setUp(self):
        self.now = 100.0
        tree = ast.parse((ROOT / 'python/companion_controller.py').read_text(encoding='utf-8'))
        names = {'optional_view_active', 'accept_view_lease', 'optional_feed_ticks', 'get_steam_service'}
        functions = [n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name in names]
        self.ns = {'_view_lease': (-1, 0.0, None), 'active_esp_ip': '192.0.2.2',
                   'time': SimpleNamespace(monotonic=lambda: self.now, sleep=self.sleep)}
        exec(compile(ast.Module(body=functions, type_ignores=[]), 'controller', 'exec'), self.ns)

    def sleep(self, seconds):
        self.now += seconds

    def test_quiet_default_expiry_and_untrusted_input(self):
        active, accept = self.ns['optional_view_active'], self.ns['accept_view_lease']
        self.assertFalse(active(7))
        for bad in ('VIEW:-1', 'VIEW:20', 'VIEW:x', 'VIEW:7:1', 'VIEW:\u0667', 'VIEW:'):
            self.assertFalse(accept(bad, '192.0.2.2'))
        self.assertFalse(accept('VIEW:7', '192.0.2.3'))
        self.assertTrue(accept('VIEW:7', '192.0.2.2'))
        for page in range(20):
            self.assertEqual(active(page), page in (6, 7, 8))
        self.assertFalse(active(6, adjacent=False))
        self.now += 12
        self.assertFalse(active(7))

    def test_wraparound(self):
        self.ns['accept_view_lease']('VIEW:0', '192.0.2.2')
        self.assertTrue(self.ns['optional_view_active'](19))

    def test_lyrics_focus_stops_all_optional_feeds(self):
        self.ns['accept_view_lease']('VIEW:2', '192.0.2.2')
        for page in range(20):
            self.assertFalse(self.ns['optional_view_active'](page))

    def test_inactive_does_no_collection_then_wakes_promptly(self):
        def sleep(seconds):
            self.now += seconds
            if self.now == 102:
                self.ns['accept_view_lease']('VIEW:7', '192.0.2.2')
            if self.now > 103:
                raise StopLoop
        self.ns['time'].sleep = sleep
        ticks = self.ns['optional_feed_ticks'](7, 30)
        next(ticks)
        self.assertEqual(self.now, 102)
        with self.assertRaises(StopLoop):
            next(ticks)

    def test_steam_tracker_not_constructed_at_import(self):
        tree = ast.parse((ROOT / 'python/companion_controller.py').read_text(encoding='utf-8'))
        for node in tree.body:
            self.assertFalse(isinstance(node, ast.ImportFrom) and node.module == 'steam_tracker')
            if isinstance(node, ast.Assign):
                self.assertFalse(isinstance(node.value, ast.Call) and isinstance(node.value.func, ast.Name)
                                 and node.value.func.id == 'SteamTracker')

    def test_steam_constructed_once_on_first_demand(self):
        calls = []
        instance = object()

        def construct(**kwargs):
            calls.append(kwargs)
            return instance

        self.ns.update(steam_service=None, gaming_cfg={'steam_id': 'test-account'})
        with patch.dict('sys.modules', {'steam_tracker': SimpleNamespace(SteamTracker=construct)}):
            self.assertEqual(calls, [])
            self.assertIs(self.ns['get_steam_service'](), instance)
            self.assertIs(self.ns['get_steam_service'](), instance)
        self.assertEqual(calls, [{'steam_api_key': '', 'steam_id': 'test-account'}])


if __name__ == '__main__':
    unittest.main()
