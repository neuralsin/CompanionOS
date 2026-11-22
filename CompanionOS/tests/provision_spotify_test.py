"""Account-free provisioning regressions using fixture files and fake UDP only."""

import contextlib
import errno
import importlib.util
import io
import json
import logging
from pathlib import Path
import socket
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

from spotipy import cache_handler


TOOL = Path(__file__).resolve().parents[1] / "tools" / "provision_spotify.py"
SPEC = importlib.util.spec_from_file_location("provision_spotify", TOOL)
tool = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(tool)
PEER = ("192.168.40.23", 8888)
TOKEN = {"access_token": "fixture-access-never-log", "refresh_token": "fixture-refresh-never-log",
         "expires_at": 4102444800}
CLIENT_ID = "fixture-client-never-log"
SECRET = "fixture-secret-never-log"


def config():
    return {"network": {"esp_ip": PEER[0], "esp_port_rx": PEER[1], "pc_port_rx": 8889},
            "spotify": {"client_id": CLIENT_ID, "client_secret": SECRET,
                        "redirect_uri": "http://127.0.0.1:8765/callback", "scope": "fixture-scope"}}


class FakeClock:
    def __init__(self):
        self.now = 0.0

    def __call__(self):
        return self.now


class FakeUDP:
    def __init__(self, clock, events=(), on_send=None):
        self.clock = clock
        self.events = list(events)
        self.sent = []
        self.closed = False
        self.on_send = on_send

    def __enter__(self):
        return self

    def __exit__(self, *_args):
        self.closed = True

    def settimeout(self, value):
        self.timeout = value

    def sendto(self, data, peer):
        self.sent.append((data, peer))
        if self.on_send:
            self.on_send(self, data, peer)
        return len(data)

    def recvfrom(self, size):
        assert size == 2048
        self.events.sort(key=lambda event: event[0])
        if self.events and self.events[0][0] <= self.clock.now + self.timeout:
            when, data, peer = self.events.pop(0)
            self.clock.now = max(self.clock.now, when)
            return data, peer
        self.clock.now += self.timeout
        raise socket.timeout


class ProvisionTests(unittest.TestCase):
    def setUp(self):
        # A test accidentally using the real socket or account path must fail.
        self.socket_guard = patch.object(tool.socket, "socket", side_effect=AssertionError("Real UDP forbidden"))
        self.socket_guard.start()
        self.addCleanup(self.socket_guard.stop)
        self.clock = FakeClock()
        self.loader = Mock(return_value=(dict(TOKEN), CLIENT_ID, SECRET))
        self.status = []

    def run_tool(self, udp, settings=None, timeout=8):
        return tool.provision(config() if settings is None else settings, timeout,
                              token_loader=self.loader, socket_factory=lambda: udp,
                              clock=self.clock, report=self.status.append)

    def assert_no_secrets(self, output):
        for value in (TOKEN["access_token"], TOKEN["refresh_token"], CLIENT_ID, SECRET,
                      "SPOTIFY_AUTH:{", "https://", "http://"):
            self.assertNotIn(value, output)

    def test_success_exact_peer_compact_packet_and_no_persistence_claim(self):
        udp = FakeUDP(self.clock, [(0.1, b"HELLO_COMPANION", PEER), (0.2, b"SPOTIFY_AUTH_OK", PEER)])
        self.assertEqual(self.run_tool(udp), 0)
        self.assertEqual(udp.sent[0], (b"COMPANION_PROBE:PC", PEER))
        packet, peer = udp.sent[1]
        self.assertEqual(peer, PEER)
        self.assertLessEqual(len(packet), 1800)
        self.assertNotIn(b'": ', packet)
        self.assertEqual(json.loads(packet.removeprefix(b"SPOTIFY_AUTH:")), {
            "access": TOKEN["access_token"], "refresh": TOKEN["refresh_token"],
            "client_id": CLIENT_ID, "client_secret": SECRET})
        self.assertTrue(udp.closed)
        self.assertIn("not tested", self.status[-1])
        self.assert_no_secrets("\n".join(self.status))

    def test_no_hello_means_no_credentials(self):
        udp = FakeUDP(self.clock)
        with self.assertRaisesRegex(tool.ProvisionError, "no credentials were transmitted"):
            self.run_tool(udp)
        self.assertEqual(udp.sent, [(b"COMPANION_PROBE:PC", PEER)])
        self.assertTrue(udp.closed)
        self.assertLessEqual(self.clock.now, 8)

    def test_wrong_ip_port_or_message_cannot_authorize_send(self):
        udp = FakeUDP(self.clock, [
            (0.1, b"HELLO_COMPANION", ("192.168.40.24", 8888)),
            (0.2, b"HELLO_COMPANION", (PEER[0], 7777)),
            (0.3, b"HELLO_COMPANION\n", PEER),
            (0.4, b"HELLO_COMPANION:anything", PEER),
            (0.5, b"SPOTIFY_AUTH_OK", PEER),
        ])
        with self.assertRaises(tool.ProvisionError):
            self.run_tool(udp)
        self.assertEqual(len(udp.sent), 1)

    def test_only_exact_ack_succeeds_and_retries_at_most_three_identical_packets(self):
        udp = FakeUDP(self.clock, [
            (0.1, b"HELLO_COMPANION", PEER),
            (0.2, b"SPOTIFY_AUTH_OK", ("192.168.40.24", 8888)),
            (0.3, b"SPOTIFY_AUTH_OK", (PEER[0], 7777)),
            (0.4, b"SPOTIFY_AUTH_OK\n", PEER),
        ])
        with self.assertRaisesRegex(tool.ProvisionError, "older firmware"):
            self.run_tool(udp)
        self.assertEqual(len(udp.sent), 4)
        self.assertEqual(udp.sent[1], udp.sent[2])
        self.assertEqual(udp.sent[2], udp.sent[3])
        self.assertTrue(all(peer == PEER for _, peer in udp.sent))
        self.assertLessEqual(self.clock.now, 8)
        self.assertFalse(any("acknowledged" in status for status in self.status))

    def test_third_idempotent_attempt_can_succeed(self):
        def respond(udp, data, peer):
            if data.startswith(b"SPOTIFY_AUTH:") and len(udp.sent) == 4:
                udp.events.append((self.clock.now + 0.01, b"SPOTIFY_AUTH_OK", peer))
        udp = FakeUDP(self.clock, [(0.1, b"HELLO_COMPANION", PEER)], respond)
        self.assertEqual(self.run_tool(udp), 0)
        self.assertEqual(len(udp.sent), 4)
        self.loader.assert_called_once()

    def test_invalid_targets_rejected_before_socket_or_cache(self):
        invalid = ["0.0.0.0", "255.255.255.255", "192.168.40.0", "192.168.40.255",
                   "127.0.0.1", "169.254.1.2", "224.0.0.1", "8.8.8.8", "100.64.0.1",
                   "192.0.0.1", "198.18.0.1", "::1", "::ffff:192.168.40.23", "deskbot.local",
                   "192.168.040.23", "", None, 3232245783, True]
        for address in invalid:
            with self.subTest(address=address):
                settings = config()
                settings["network"]["esp_ip"] = address
                with self.assertRaises(tool.ProvisionError):
                    self.run_tool(FakeUDP(self.clock), settings)
        self.loader.assert_not_called()

    def test_rfc1918_only(self):
        for address in ("10.0.0.1", "172.16.1.1", "172.31.254.254", "192.168.1.1"):
            settings = config()
            settings["network"]["esp_ip"] = address
            self.assertEqual(tool.target_from_config(settings), (address, 8888))

    def test_invalid_ports_and_configuration(self):
        for key, value in (("esp_port_rx", 0), ("esp_port_rx", 65536), ("esp_port_rx", True),
                           ("esp_port_rx", "8888"), ("pc_port_rx", 8888), ("pc_port_rx", "8889")):
            settings = config()
            settings["network"][key] = value
            with self.assertRaises(tool.ProvisionError):
                tool.target_from_config(settings)
        for settings in ({}, {"network": None}, {"network": {}}):
            with self.assertRaises(tool.ProvisionError):
                tool.target_from_config(settings)

    def test_invalid_timeout_no_network_or_cache(self):
        for timeout in (0, -1, float("nan"), float("inf"), "8"):
            with self.assertRaises(tool.ProvisionError):
                self.run_tool(FakeUDP(self.clock), timeout=timeout)
        self.loader.assert_not_called()

    def test_incomplete_and_oversized_credentials_never_transmitted(self):
        for token in ({}, {**TOKEN, "access_token": ""}, {**TOKEN, "refresh_token": None},
                      {**TOKEN, "access_token": "x" * 1800}):
            self.loader.return_value = (token, CLIENT_ID, SECRET)
            udp = FakeUDP(self.clock)
            with self.assertRaises(tool.ProvisionError):
                self.run_tool(udp)
            self.assertEqual(udp.sent, [])
            self.assertTrue(udp.closed)

    def test_wire_limit_counts_escaped_bytes(self):
        packet = tool.auth_packet(TOKEN, CLIENT_ID, SECRET)
        size = 1800 - len(packet) + len(TOKEN["access_token"])
        self.assertEqual(len(tool.auth_packet({**TOKEN, "access_token": "x" * size}, CLIENT_ID, SECRET)), 1800)
        with self.assertRaises(tool.ProvisionError):
            tool.auth_packet({**TOKEN, "access_token": "x" * (size + 1)}, CLIENT_ID, SECRET)
        with self.assertRaisesRegex(tool.ProvisionError, "packet limit"):
            tool.auth_packet({**TOKEN, "access_token": '"' * 900}, CLIENT_ID, SECRET)

    def test_empty_client_secret_pkce_is_transmitted_after_exact_hello(self):
        self.loader.return_value = (dict(TOKEN), CLIENT_ID, "")
        udp = FakeUDP(self.clock, [(0.1, b"HELLO_COMPANION", PEER), (0.2, b"SPOTIFY_AUTH_OK", PEER)])
        self.assertEqual(self.run_tool(udp), 0)
        self.assertEqual(udp.sent[0], (b"COMPANION_PROBE:PC", PEER))
        fields = json.loads(udp.sent[1][0].removeprefix(b"SPOTIFY_AUTH:"))
        self.assertEqual(fields, {"access": TOKEN["access_token"], "refresh": TOKEN["refresh_token"],
                                  "client_id": CLIENT_ID, "client_secret": ""})
        self.assertEqual(udp.sent[1][1], PEER)
        self.assert_no_secrets("\n".join(self.status))

    def test_non_ascii_whitespace_controls_and_non_strings_rejected_in_each_field(self):
        invalid = (" ", "\t", "\n", "\r", "\0", "\x1f", "\x7f", "\x80", "\u2603", "\U0001f600",
                   None, 123, True)
        for name in ("access_token", "refresh_token", "client_id", "client_secret"):
            for value in invalid:
                with self.subTest(field=name, kind=type(value).__name__):
                    token, client_id, secret = dict(TOKEN), CLIENT_ID, SECRET
                    if name == "client_id":
                        client_id = value
                    elif name == "client_secret":
                        secret = value
                    else:
                        token[name] = value
                    self.loader.return_value = (token, client_id, secret)
                    udp = FakeUDP(self.clock)
                    with self.assertRaisesRegex(tool.ProvisionError, "incomplete"):
                        self.run_tool(udp)
                    self.assertEqual(udp.sent, [])

    def test_per_field_length_limits_reserve_terminator(self):
        limits = {"access_token": 2048, "refresh_token": 1024, "client_id": 128, "client_secret": 256}
        # Isolate per-field bounds: the access field's maximum exceeds the wire cap.
        # The independent wire-limit test retains the real 1800-byte cap.
        with patch.object(tool, "MAX_PACKET", 10000):
            for name, limit in limits.items():
                for length in (limit - 1, limit, limit + 1):
                    with self.subTest(field=name, length=length):
                        token, client_id, secret = dict(TOKEN), CLIENT_ID, SECRET
                        value = "x" * length
                        if name == "client_id":
                            client_id = value
                        elif name == "client_secret":
                            secret = value
                        else:
                            token[name] = value
                        if length < limit:
                            packet = tool.auth_packet(token, client_id, secret)
                            fields = json.loads(packet.removeprefix(b"SPOTIFY_AUTH:"))
                            wire_name = {"access_token": "access", "refresh_token": "refresh"}.get(name, name)
                            self.assertEqual(fields[wire_name], value)
                        else:
                            with self.assertRaisesRegex(tool.ProvisionError, "incomplete"):
                                tool.auth_packet(token, client_id, secret)

    def test_only_client_secret_may_be_empty(self):
        for name in ("access_token", "refresh_token", "client_id"):
            with self.subTest(field=name):
                token, client_id = dict(TOKEN), CLIENT_ID
                if name == "client_id":
                    client_id = ""
                else:
                    token[name] = ""
                with self.assertRaisesRegex(tool.ProvisionError, "incomplete"):
                    tool.auth_packet(token, client_id, "")

    def test_dependency_output_and_exception_are_redacted(self):
        def noisy(_settings):
            print(TOKEN["access_token"])
            print(SECRET, file=sys.stderr)
            logging.error(TOKEN["refresh_token"])
            raise RuntimeError("https://secret.invalid/" + CLIENT_ID)
        self.loader.side_effect = noisy
        stdout, stderr = io.StringIO(), io.StringIO()
        previous = logging.root.manager.disable
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            with self.assertRaises(tool.ProvisionError) as error:
                self.run_tool(FakeUDP(self.clock))
        self.assertEqual(stdout.getvalue() + stderr.getvalue(), "")
        self.assert_no_secrets(str(error.exception))
        self.assertEqual(logging.root.manager.disable, previous)

    def test_bind_collision_does_not_load_credentials_or_kill_controller(self):
        sock = Mock()
        sock.bind.side_effect = OSError(errno.EADDRINUSE, SECRET)
        with patch.object(tool.socket, "socket", return_value=sock):
            with self.assertRaisesRegex(tool.ProvisionError, "controller.*will provision") as error:
                tool.provision(config(), 8, token_loader=self.loader)
        self.loader.assert_not_called()
        sock.sendto.assert_not_called()
        sock.close.assert_called_once()
        self.assert_no_secrets(str(error.exception))

    def test_exclusive_windows_bind_and_never_enable_broadcast_or_reuse(self):
        sock = Mock()
        with patch.object(tool.socket, "socket", return_value=sock) as factory, \
                patch.object(tool.socket, "SO_EXCLUSIVEADDRUSE", 999, create=True):
            self.assertIs(tool.open_receiver(), sock)
        factory.assert_called_once_with(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt.assert_called_once_with(socket.SOL_SOCKET, 999, 1)
        sock.bind.assert_called_once_with(("0.0.0.0", 8889))

    def test_deadline_exhausted_during_cache_load_sends_nothing(self):
        def slow(_settings):
            self.clock.now = 9
            return dict(TOKEN), CLIENT_ID, SECRET
        self.loader.side_effect = slow
        udp = FakeUDP(self.clock)
        with self.assertRaisesRegex(tool.ProvisionError, "timed out"):
            self.run_tool(udp)
        self.assertEqual(udp.sent, [])

    def test_cli_requires_no_broadcast_and_defaults_to_eight(self):
        with patch.object(tool, "provision") as provision, contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as error:
                tool.main([])
            self.assertEqual(error.exception.code, 2)
            provision.assert_not_called()
        with tempfile.TemporaryDirectory() as directory:
            fixture = Path(directory)
            (fixture / "python").mkdir()
            (fixture / "python" / "config.json").write_text(json.dumps(config()), encoding="utf-8")
            with patch.object(tool, "ROOT", fixture), patch.object(tool, "provision", return_value=0) as provision:
                self.assertEqual(tool.main(["--no-broadcast"]), 0)
                provision.assert_called_once_with(config(), 8.0)

    def test_main_reports_errors_without_secret_exception_details(self):
        for error in (RuntimeError(SECRET), OSError(TOKEN["access_token"])):
            output = io.StringIO()
            with patch.object(Path, "read_text", side_effect=error), contextlib.redirect_stdout(output):
                self.assertEqual(tool.main(["--no-broadcast"]), 1)
            self.assert_no_secrets(output.getvalue())
            self.assertIn("not confirmed", output.getvalue())

    def test_real_cache_file_handler_reads_fixture_without_oauth_or_accounts(self):
        with tempfile.TemporaryDirectory() as directory:
            fixture = Path(directory)
            (fixture / "python").mkdir()
            cache = fixture / "python" / ".cache"
            cache.write_text(json.dumps(TOKEN), encoding="utf-8")
            before = list(sys.path)
            with patch.object(tool, "ROOT", fixture), \
                    patch.object(cache_handler, "CacheFileHandler", wraps=cache_handler.CacheFileHandler) as factory, \
                    patch.dict(sys.modules, {"spotify_integration": None, "companion_controller": None,
                                             "spotipy.oauth2": None}), \
                    patch.dict(tool.os.environ, {"SPOTIFY_CLIENT_ID": CLIENT_ID, "SPOTIFY_CLIENT_SECRET": SECRET}), \
                    patch.object(tool.time, "time", return_value=1700000000):
                self.assertEqual(tool.cached_token(config()), (TOKEN, CLIENT_ID, SECRET))
                self.assertEqual(json.loads(cache.read_text(encoding="utf-8")), TOKEN)
                invalid = (None, {}, {**TOKEN, "expires_at": 0}, {**TOKEN, "expires_at": True},
                           {**TOKEN, "expires_at": float("nan")}, {**TOKEN, "expires_at": float("inf")})
                for token in invalid:
                    cache.write_text(json.dumps(token), encoding="utf-8")
                    contents = cache.read_text(encoding="utf-8")
                    with self.assertRaises(ValueError):
                        tool.cached_token(config())
                    self.assertEqual(cache.read_text(encoding="utf-8"), contents)
                self.assertEqual(factory.call_count, 1 + len(invalid))
                factory.assert_called_with(cache_path=str(cache))
            self.assertEqual(sys.path, before)


if __name__ == "__main__":
    unittest.main()
