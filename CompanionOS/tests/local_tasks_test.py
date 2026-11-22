"""Local productivity data must be real, bounded and chronologically ordered."""

from datetime import datetime
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

SOURCE = Path(__file__).resolve().parents[1] / 'python/google_cal.py'
SPEC = importlib.util.spec_from_file_location('tested_calendar', SOURCE)
calendar = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(calendar)


class LocalTaskTests(unittest.TestCase):
    def test_fresh_install_does_not_invent_a_schedule(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'tasks.json'
            task_list = calendar.LocalTaskList(path)
            self.assertEqual(json.loads(path.read_text()), [])
            self.assertFalse(task_list.get_productivity_state()['active'])

    def test_invalid_rows_do_not_hide_real_tasks_and_next_is_sorted(self):
        entries = [None, 42, {'title': None, 'time': '13:00'},
                   {'title': 'Invalid', 'time': '25:90 - 26:00'},
                   {'title': 'Reversed', 'time': '13:00 - 12:00'},
                   {'title': 'Late', 'time': '18:00'},
                   {'title': 'Done', 'time': '13:30', 'done': True},
                   {'title': 'Next', 'time': '13:00 - 14:00'},
                   {'title': 'Current', 'time': '11:00 - 12:00'}]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'tasks.json'
            path.write_text(json.dumps({'tasks': entries}), encoding='utf-8')
            before = path.read_bytes()
            task_list = calendar.LocalTaskList(path)
            with patch.object(calendar, 'datetime') as clock:
                clock.now.return_value = datetime(2026, 10, 25, 11, 30)
                state = task_list.get_productivity_state()
            self.assertEqual(state['current'], 'Current')
            self.assertEqual(state['progress'], 50)
            self.assertEqual((state['next1'], state['next2']), ('Next', 'Late'))
            self.assertEqual(path.read_bytes(), before)


if __name__ == '__main__':
    unittest.main()
