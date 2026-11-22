"""Check real controller startup wiring and run the real publisher with fake CSI state."""

import ast
import contextlib
import io
import json
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import Mock

ROOT = Path(__file__).resolve().parents[1]


class StopLoop(BaseException):
    pass


class RuViewWiringTests(unittest.TestCase):
    def test_probe_echo_is_not_reported_as_a_sensor_measurement(self):
        tree = ast.parse((ROOT / "python/ruview_processor.py").read_text(encoding="utf-8"))
        function = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == "ruview_listener_loop")
        sock = Mock()
        address = ('192.0.2.2', 50000)
        sock.recvfrom.side_effect = [(b"COMPANION_CSI_PING", address), StopLoop()]
        stats = {"last_rx_time": 0, "packets_rx": 0, "invalid_rx": 0}
        namespace = {"socket": SimpleNamespace(socket=lambda *args: sock, AF_INET=2, SOCK_DGRAM=2,
                                              SOL_SOCKET=1, SO_REUSEADDR=2, timeout=TimeoutError),
                     "time": SimpleNamespace(time=lambda: 1000), "ruview_enabled": True,
                     "ruview_stats": stats}
        exec(compile(ast.Module(body=[function], type_ignores=[]), "ruview_processor.py", "exec"), namespace)
        with contextlib.redirect_stdout(io.StringIO()), self.assertRaises(StopLoop):
            namespace['ruview_listener_loop']()
        sock.sendto.assert_called_once_with(b"COMPANION_CSI_PONG", address)
        self.assertEqual(stats, {"last_rx_time": 0, "packets_rx": 0, "invalid_rx": 0})

    def test_expired_frames_never_publish_live_occupancy(self):
        tree = ast.parse((ROOT / "python/ruview_processor.py").read_text(encoding="utf-8"))
        cls = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == "PresenceClassifier")
        function = next(n for n in cls.body if isinstance(n, ast.FunctionDef) and n.name == "get_state")
        namespace = {"time": SimpleNamespace(time=lambda: 1000.0)}
        exec(compile(ast.Module(body=[function], type_ignores=[]), "ruview_processor.py", "exec"), namespace)
        sensor = SimpleNamespace(last_frame_time=990.0, pps=20.0, occupied=True, motion=True,
                                 confidence=95.0, current_variance=1.2, rssi=-60, calibrating=True,
                                 n_subcarriers=128, frames_processed=200, presence_threshold=2.0,
                                 motion_threshold=5.0)
        state = namespace['get_state'](sensor)
        self.assertEqual(state['pps'], 0)
        self.assertFalse(state['occupied'] or state['motion'] or state['calibrating'])
        self.assertEqual(state['confidence'], 0)
        self.assertIn("Offline", state['status'])

    def test_both_threads_started(self):
        tree = ast.parse((ROOT / "python/companion_controller.py").read_text(encoding="utf-8"))
        main = next(node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name == "main")
        started = set()
        gated = set()
        for node in ast.walk(main):
            if (isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)
                    and node.func.attr == "start" and isinstance(node.func.value, ast.Call)):
                for keyword in node.func.value.keywords:
                    if keyword.arg == "target" and isinstance(keyword.value, ast.Name):
                        started.add(keyword.value.id)
                        if any(k.arg == 'kwargs' and isinstance(k.value, ast.Dict)
                               and any(isinstance(key, ast.Constant) and key.value == 'active_fn'
                                       for key in k.value.keys) for k in node.func.value.keywords):
                            gated.add(keyword.value.id)
        self.assertTrue({"ruview_listener_loop", "ruview_push_loop"}.issubset(started))
        self.assertTrue({"ruview_listener_loop", "ruview_push_loop"}.issubset(gated))

    def test_listener_does_not_read_packets_without_a_view_lease(self):
        tree = ast.parse((ROOT / "python/ruview_processor.py").read_text(encoding="utf-8"))
        function = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == "ruview_listener_loop")
        sock = Mock()
        def sleep(seconds):
            self.assertEqual(seconds, 0.25)
            raise StopLoop
        namespace = {"socket": SimpleNamespace(socket=lambda *args: sock, AF_INET=2, SOCK_DGRAM=2,
                                              SOL_SOCKET=1, SO_REUSEADDR=2, timeout=TimeoutError),
                     "time": SimpleNamespace(sleep=sleep), "ruview_enabled": True}
        exec(compile(ast.Module(body=[function], type_ignores=[]), "ruview_processor.py", "exec"), namespace)
        with contextlib.redirect_stdout(io.StringIO()), self.assertRaises(StopLoop):
            namespace['ruview_listener_loop'](active_fn=lambda: False)
        sock.recvfrom.assert_not_called()

    def publish_once(self, states):
        tree = ast.parse((ROOT / "python/ruview_processor.py").read_text(encoding="utf-8"))
        function = next(node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name == "ruview_push_loop")
        sleeps = []

        def sleep(seconds):
            sleeps.append(seconds)
            if len(sleeps) == 2:
                raise StopLoop

        namespace = {
            "time": SimpleNamespace(time=lambda: 1000.0, sleep=sleep), "json": json,
            "ruview_enabled": True, "zone_manager": SimpleNamespace(get_all_states=lambda: states),
            "ruview_stats": {"last_rx_time": 0, "packets_rx": 0, "invalid_rx": 0},
        }
        exec(compile(ast.Module(body=[function], type_ignores=[]), "ruview_processor.py", "exec"), namespace)
        packets = []
        with contextlib.redirect_stdout(io.StringIO()), self.assertRaises(StopLoop):
            namespace["ruview_push_loop"](packets.append)
        self.assertEqual(len(packets), 1)
        self.assertTrue(packets[0].startswith("RUVIEW:"))
        return json.loads(packets[0][7:])

    def test_processed_state_reaches_display(self):
        state = {"status": "Still", "occupied": True, "motion": False, "confidence": 75, "variance": 1.2, "pps": 20}
        self.assertEqual(self.publish_once([state]), state)

    def test_missing_frames_not_faked_as_occupied(self):
        state = self.publish_once([])
        self.assertFalse(state["occupied"])
        self.assertEqual(state["pps"], 0)
        self.assertIn("No CSI data", state["status"])

    def test_inactive_publisher_does_not_collect_or_send(self):
        tree = ast.parse((ROOT / "python/ruview_processor.py").read_text(encoding="utf-8"))
        function = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == "ruview_push_loop")
        sleeps = []

        def sleep(seconds):
            sleeps.append(seconds)
            if len(sleeps) == 2:
                raise StopLoop

        def collect():
            self.fail("Inactive publisher collected CSI state")

        namespace = {"time": SimpleNamespace(sleep=sleep), "ruview_enabled": True,
                     "zone_manager": SimpleNamespace(get_all_states=collect)}
        exec(compile(ast.Module(body=[function], type_ignores=[]), "ruview_processor.py", "exec"), namespace)
        packets = []
        with contextlib.redirect_stdout(io.StringIO()), self.assertRaises(StopLoop):
            namespace['ruview_push_loop'](packets.append, active_fn=lambda: False)
        self.assertFalse(packets)


if __name__ == "__main__":
    unittest.main()
