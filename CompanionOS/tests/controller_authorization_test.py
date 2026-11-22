"""Exercise actual controller credential-routing functions without starting its services."""

import ast
import json
from pathlib import Path
from unittest import TestCase, main
from unittest.mock import Mock

SOURCE = Path(__file__).resolve().parents[1] / "python/companion_controller.py"


class ControllerAuthorizationTests(TestCase):
    def setUp(self):
        tree = ast.parse(SOURCE.read_text(encoding="utf-8"))
        names = {"sync_spotify_credentials", "recover_spotify_authorization"}
        functions = [node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name in names]
        self.token = {"access_token": "FAKE_ACCESS", "refresh_token": "FAKE_REFRESH"}
        self.service = Mock()
        self.service.get_token_info.return_value = self.token
        self.sock = Mock()
        self.env = {"json": json, "sock": self.sock, "spotify_service": self.service,
                    "active_esp_ip": "192.168.1.123", "ESP_PORT_RX": 8888,
                    "SPOTIFY_CLIENT_ID": "FAKE_CLIENT", "SPOTIFY_CLIENT_SECRET": "FAKE_SECRET",
                    "print": Mock()}
        exec(compile(ast.Module(body=functions, type_ignores=[]), str(SOURCE), "exec"), self.env)

    def test_known_unicast_only(self):
        self.assertTrue(self.env["sync_spotify_credentials"]())
        packet, destination = self.sock.sendto.call_args.args
        self.assertEqual(destination, ("192.168.1.123", 8888))
        self.assertTrue(packet.startswith(b"SPOTIFY_AUTH:"))
        self.assertEqual(json.loads(packet[13:])["refresh"], "FAKE_REFRESH")
        self.env["print"].assert_not_called()

    def test_invalid_or_broadcast_targets_never_receive_secrets(self):
        for target in (None, "", "255.255.255.255", "192.168.1.255", "192.168.1.0", "127.0.0.1",
                       "0.0.0.0", "224.0.0.1", "8.8.8.8", "::1", "invalid"):
            self.env["active_esp_ip"] = target
            self.assertFalse(self.env["sync_spotify_credentials"](), target)
        self.sock.sendto.assert_not_called()

    def test_missing_and_oversized_tokens_rejected(self):
        for token in ({}, {"access_token": "x"}, {"access_token": "x" * 2000, "refresh_token": "y"}):
            self.service.get_token_info.return_value = token
            self.assertFalse(self.env["sync_spotify_credentials"]())
        self.sock.sendto.assert_not_called()

    def test_valid_cache_does_not_open_browser(self):
        self.env["recover_spotify_authorization"]("192.168.1.123")
        self.sock.sendto.assert_called_once()
        self.service.request_authorization.assert_not_called()

    def test_invalid_cache_starts_flow_and_callback_provisions(self):
        self.service.get_token_info.return_value = None
        self.env["recover_spotify_authorization"]("192.168.1.123")
        callback = self.service.request_authorization.call_args.args[0]
        callback(self.token)
        self.sock.sendto.assert_called_once()


if __name__ == "__main__":
    main()
