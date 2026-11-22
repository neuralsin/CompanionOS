"""Run real controller timer/feed functions without starting hardware or services."""

import ast
import contextlib
import io
import json
import os
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import mock_open, patch

SOURCE = Path(__file__).resolve().parents[1] / "python/companion_controller.py"


def load_function(name, namespace):
    tree = ast.parse(SOURCE.read_text(encoding="utf-8"))
    function = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == name)
    exec(compile(ast.Module(body=[function], type_ignores=[]), str(SOURCE), "exec"), namespace)
    return namespace[name]


class TimerTests(unittest.TestCase):
    def setUp(self):
        self.state = dict(pomodoro_active=False, pomodoro_remaining=700.0, pomodoro_is_break=False,
                          pomodoro_duration=1500, pomodoro_break_duration=300, pomodoro_last_tick=90.0,
                          pomodoro_sessions=4, time=SimpleNamespace(time=lambda: 100.0))
        self.command = load_function('handle_pomodoro_command', self.state)

    def test_resume_preserves_progress(self):
        self.assertTrue(self.command('POMO:START'))
        self.assertTrue(self.state['pomodoro_active'])
        self.assertEqual(self.state['pomodoro_remaining'], 700)
        self.assertEqual(self.state['pomodoro_last_tick'], 100)

    def test_pause_accounts_for_time_since_last_tick_exactly_once(self):
        self.state['pomodoro_active'] = True
        self.command('POMO:PAUSE')
        self.command('POMO:PAUSE')
        self.assertFalse(self.state['pomodoro_active'])
        self.assertEqual(self.state['pomodoro_remaining'], 690)

    def test_restart_completed_phase(self):
        self.state.update(pomodoro_remaining=0, pomodoro_is_break=True)
        self.command('POMO:START')
        self.assertEqual(self.state['pomodoro_remaining'], 300)

    def test_reset_is_handled_and_skip_does_not_count_as_completion(self):
        self.command('POMO:SKIP')
        self.assertTrue(self.state['pomodoro_is_break'])
        self.assertEqual(self.state['pomodoro_remaining'], 300)
        self.assertEqual(self.state['pomodoro_sessions'], 4)
        self.command('POMO:RESET')
        self.assertFalse(self.state['pomodoro_active'] or self.state['pomodoro_is_break'])
        self.assertEqual(self.state['pomodoro_remaining'], 1500)

    def test_other_commands_are_not_consumed(self):
        before = dict(self.state)
        self.assertFalse(self.command('PLAY_PAUSE'))
        self.assertEqual(self.state, before)


class SocialFeedTests(unittest.TestCase):
    def publish(self, contents):
        packets = []
        state = {'config': {}, 'optional_feed_ticks': lambda *args: iter(range(2)),
                 'SCRIPT_DIR': 'unused', 'os': SimpleNamespace(path=SimpleNamespace(
                     join=os.path.join, exists=lambda path: True)), 'json': json, 'send_udp': packets.append}
        feed = load_function('social_feed_loop', state)
        with patch('builtins.open', mock_open(read_data=json.dumps(contents))), contextlib.redirect_stdout(io.StringIO()):
            feed()
        return [json.loads(packet[7:]) for packet in packets]

    def test_identical_message_resends_for_reconnection(self):
        packets = self.publish([{'app': 'Messages', 'title': 'Sam: Hello', 'time': '10:00'}])
        self.assertEqual(len(packets), 2)
        self.assertEqual(packets[0], packets[1])
        self.assertEqual(packets[0]['body'], 'Hello')

    def test_empty_source_clears_old_card(self):
        for packet in self.publish([]):
            self.assertEqual(packet['body'], '')
            self.assertEqual(packet['user'], '')

    def test_malformed_rows_do_not_crash_feed(self):
        packets = self.publish([None, {'title': 'Real message', 'app': 'Mail'}, 'bad'])
        self.assertEqual(packets[0]['app'], 'Mail')


if __name__ == '__main__':
    unittest.main()
