"""Local-only BrowserAuthorization tests.

Run: python -B -m unittest discover -s CompanionOS/tests -p spotify_authorization_test.py -v
No credentials, Spotify client, external requests, or real browser are used.
"""

import contextlib
import http.client
import importlib.util
import io
import logging
from pathlib import Path
import socket
import threading
import types
import unittest
from unittest import mock
from urllib.parse import parse_qs, urlencode, urlsplit


SOURCE = Path(__file__).resolve().parents[1] / "python" / "spotify_authorization.py"
SPEC = importlib.util.spec_from_file_location("spotify_authorization_under_test", SOURCE)
authorization = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(authorization)

FAKE_CODE = "FAKE_PRIVATE_AUTHORIZATION_CODE"
FAKE_ACCESS = "FAKE_PRIVATE_ACCESS_TOKEN"
FAKE_REFRESH = "FAKE_PRIVATE_REFRESH_TOKEN"


class FakeAuthManager:
    def __init__(self, port, host="127.0.0.1"):
        netloc = f"[{host}]" if ":" in host else host
        self.redirect_uri = f"http://{netloc}:{port}/callback"
        self.authorize_states = []
        self.exchanges = []
        self.token = {"access_token": FAKE_ACCESS, "refresh_token": FAKE_REFRESH}
        self.exception = None

    def get_authorize_url(self, *, state):
        self.authorize_states.append(state)
        # This value is inspected as a Location header, never followed.
        return "https://spotify.example.invalid/authorize?" + urlencode({"state": state})

    def get_access_token(self, *, code, check_cache):
        self.exchanges.append((code, check_cache))
        if self.exception is not None:
            raise self.exception
        return self.token


class BrowserAuthorizationTests(unittest.TestCase):
    def setUp(self):
        self.flows = []
        self.threads = []
        self.allowed_ports = set()
        self.allowed_hosts = {"127.0.0.1"}
        self.stdout = io.StringIO()
        self.stderr = io.StringIO()
        self.log_output = io.StringIO()
        self.log_handler = logging.StreamHandler(self.log_output)
        self.stack = contextlib.ExitStack()
        self.addCleanup(self.stack.close)
        self.stack.enter_context(contextlib.redirect_stdout(self.stdout))
        self.stack.enter_context(contextlib.redirect_stderr(self.stderr))
        self.browser = self.stack.enter_context(
            mock.patch.object(authorization.webbrowser, "open", return_value=True)
        )
        logging.getLogger().addHandler(self.log_handler)
        self.addCleanup(logging.getLogger().removeHandler, self.log_handler)

        # Track only this process's callback threads. Never enumerate or touch
        # the parent's live OAuth process, browser, cache, or callback listener.
        real_thread = threading.Thread

        def callback_thread(*args, **kwargs):
            thread = real_thread(*args, **kwargs)
            self.threads.append(thread)
            return thread

        self.stack.enter_context(
            mock.patch.object(authorization.threading, "Thread", side_effect=callback_thread)
        )
        real_connection = socket.create_connection

        def loopback_connection(address, *args, **kwargs):
            host, port = address
            if host not in self.allowed_hosts or port not in self.allowed_ports:
                raise AssertionError("Test attempted a connection outside its private loopback listener")
            return real_connection(address, *args, **kwargs)

        self.stack.enter_context(
            mock.patch.object(socket, "create_connection", side_effect=loopback_connection)
        )
        self.addCleanup(self.close_flows)

    def close_flows(self):
        for flow in self.flows:
            flow.close()
        for thread in self.threads:
            if thread.ident is not None:
                thread.join(timeout=2)
                self.assertFalse(thread.is_alive(), "Callback worker leaked after test cleanup")
        self.browser.assert_not_called()

    def start_flow(self, *, timeout=5, host="127.0.0.1"):
        # Production intentionally rejects port zero in redirect URIs. Reserve
        # an ephemeral candidate briefly; retry only if another process wins
        # the tiny gap before the real production server binds it.
        for _ in range(5):
            family = socket.AF_INET6 if host == "::1" else socket.AF_INET
            with socket.socket(family, socket.SOCK_STREAM) as reservation:
                reservation.bind((host, 0))
                port = reservation.getsockname()[1]
            manager = FakeAuthManager(port, host)
            success = mock.Mock()
            flow = authorization.BrowserAuthorization(
                manager, success, timeout=timeout, open_browser=False
            )
            self.flows.append(flow)
            if flow.start():
                self.allowed_ports.add(port)
                self.allowed_hosts.add(host)
                return flow, manager, success, port
            self.assertEqual(flow.status, "Spotify callback port is already in use")
        self.fail("Could not acquire a private ephemeral callback port")

    def get(self, port, path):
        self.assertIn(port, self.allowed_ports)
        connection = http.client.HTTPConnection("127.0.0.1", port, timeout=2)
        try:
            connection.request("GET", path)
            response = connection.getresponse()
            return response.status, dict(response.getheaders()), response.read().decode("utf-8")
        finally:
            connection.close()

    def callback(self, flow, port, **query):
        return self.get(port, "/callback?" + urlencode(query, doseq=True))

    def wait_closed(self, flow):
        self.assertTrue(flow.done.wait(timeout=2), "Flow did not reach a terminal state")
        # done can be set inside the handler before the worker closes its
        # listener. Join the real worker before testing replay or port reuse.
        for thread in self.threads:
            if thread.ident is not None:
                thread.join(timeout=2)
                self.assertFalse(thread.is_alive())

    def assert_no_private_logs(self, *private_values):
        captured = self.stdout.getvalue() + self.stderr.getvalue() + self.log_output.getvalue()
        for value in (FAKE_CODE, FAKE_ACCESS, FAKE_REFRESH, "code=", "state=", *private_values):
            self.assertNotIn(value, captured)
        self.assertEqual(captured, "", "The callback server must not emit request or exception logs")

    def test_login_redirect_carries_state_without_following_external_location(self):
        flow, manager, success, port = self.start_flow()
        self.assertEqual(flow.status, "waiting for browser")
        self.assertEqual(flow.login_url, f"http://127.0.0.1:{port}/login")
        status, headers, body = self.get(port, "/login")
        self.assertEqual(status, 302)
        self.assertEqual(headers["Cache-Control"], "no-store")
        self.assertEqual(body, "")
        location = urlsplit(headers["Location"])
        self.assertEqual(location.hostname, "spotify.example.invalid")
        self.assertEqual(parse_qs(location.query)["state"], [flow.state])
        self.assertEqual(manager.authorize_states, [flow.state])
        self.assertEqual(manager.exchanges, [])
        success.assert_not_called()
        self.assertFalse(flow.done.is_set())
        self.assert_no_private_logs(flow.state, headers["Location"])

    def test_missing_blank_mismatched_and_duplicate_state_are_nonterminal(self):
        flow, manager, success, port = self.start_flow()
        for state in (None, "", "WRONG_PRIVATE_STATE", [flow.state, flow.state]):
            with self.subTest(state_kind=type(state).__name__):
                query = {"code": FAKE_CODE}
                if state is not None:
                    query["state"] = state
                status, _, _ = self.callback(flow, port, **query)
                self.assertEqual(status, 400)
                self.assertFalse(flow.done.is_set())
                self.assertEqual(flow.status, "waiting for browser")
        self.assertEqual(manager.exchanges, [])
        success.assert_not_called()
        self.assert_no_private_logs(flow.state, "WRONG_PRIVATE_STATE")

    def test_duplicate_code_parameters_are_rejected_without_exchange(self):
        flow, manager, success, port = self.start_flow()
        for codes in ([FAKE_CODE, FAKE_CODE], [FAKE_CODE, "OTHER_PRIVATE_CODE"]):
            status, _, _ = self.callback(flow, port, state=flow.state, code=codes)
            self.assertEqual(status, 400)
            self.assertFalse(flow.done.is_set())
        self.assertEqual(manager.exchanges, [])
        success.assert_not_called()
        self.assert_no_private_logs(flow.state, "OTHER_PRIVATE_CODE")

    def test_non_ascii_states_are_cleanly_rejected_and_valid_retry_still_works(self):
        flow, manager, success, port = self.start_flow()
        for state in ("non_ascii_\u00e9", "\u2603", "\U0001f600"):
            with self.subTest(codepoints=len(state)):
                status, _, body = self.callback(flow, port, state=state, code=FAKE_CODE)
                self.assertEqual(status, 400)
                self.assertIn("state did not match", body)
                self.assertFalse(flow.done.is_set())
        self.assertEqual(manager.exchanges, [])
        success.assert_not_called()
        self.assert_no_private_logs(flow.state)
        self.assertEqual(self.callback(flow, port, state=flow.state, code=FAKE_CODE)[0], 200)
        self.wait_closed(flow)
        success.assert_called_once_with(manager.token)
        self.assertEqual(manager.exchanges, [(FAKE_CODE, False)])
        self.assert_no_private_logs(flow.state)

    def test_missing_blank_and_oversized_code_are_nonterminal(self):
        flow, manager, success, port = self.start_flow()
        for code in (None, "", "X" * 2049):
            query = {"state": flow.state}
            if code is not None:
                query["code"] = code
            status, _, _ = self.callback(flow, port, **query)
            self.assertEqual(status, 400)
            self.assertFalse(flow.done.is_set())
        self.assertEqual(manager.exchanges, [])
        success.assert_not_called()

    def test_declined_authorization_is_terminal_and_never_exchanges_code(self):
        flow, manager, success, port = self.start_flow()
        status, headers, body = self.callback(
            flow, port, state=flow.state, error="access_denied", code=FAKE_CODE
        )
        self.assertEqual(status, 403)
        self.assertIn("declined", body)
        self.assertEqual(headers["Cache-Control"], "no-store")
        self.assertEqual(headers["Referrer-Policy"], "no-referrer")
        self.wait_closed(flow)
        self.assertEqual(flow.status, "Authorization declined")
        self.assertEqual(manager.exchanges, [])
        success.assert_not_called()
        self.assert_no_private_logs(flow.state)

    def test_decline_with_bad_state_cannot_terminate_original_flow(self):
        flow, manager, success, port = self.start_flow()
        status, _, _ = self.callback(flow, port, state="wrong", error="access_denied")
        self.assertEqual(status, 400)
        self.assertFalse(flow.done.is_set())
        self.assertEqual(manager.exchanges, [])
        success.assert_not_called()

    def test_valid_callback_exchanges_and_completes_exactly_once_then_refuses_replay(self):
        flow, manager, success, port = self.start_flow()
        status, headers, body = self.callback(flow, port, state=flow.state, code=FAKE_CODE)
        self.assertEqual(status, 200)
        self.assertEqual(headers["Cache-Control"], "no-store")
        self.assertEqual(headers["Referrer-Policy"], "no-referrer")
        self.assertEqual(headers["Content-Type"], "text/html; charset=utf-8")
        for value in (FAKE_CODE, FAKE_ACCESS, FAKE_REFRESH, flow.state):
            self.assertNotIn(value, body)
        self.wait_closed(flow)
        self.assertEqual(flow.status, "authorized")
        self.assertEqual(manager.exchanges, [(FAKE_CODE, False)])
        success.assert_called_once_with(manager.token)
        with self.assertRaises(OSError):
            self.callback(flow, port, state=flow.state, code=FAKE_CODE)
        self.assertEqual(manager.exchanges, [(FAKE_CODE, False)])
        success.assert_called_once_with(manager.token)
        self.assert_no_private_logs(flow.state)

    def test_rejected_request_does_not_prevent_later_valid_callback(self):
        flow, manager, success, port = self.start_flow()
        self.assertEqual(self.callback(flow, port, state="wrong", code=FAKE_CODE)[0], 400)
        self.assertEqual(self.callback(flow, port, state=flow.state, code=FAKE_CODE)[0], 200)
        self.wait_closed(flow)
        self.assertEqual(manager.exchanges, [(FAKE_CODE, False)])
        success.assert_called_once_with(manager.token)

    def test_queued_duplicate_callback_during_exchange_cannot_exchange_twice(self):
        flow, manager, success, port = self.start_flow()
        entered = threading.Event()
        release = threading.Event()
        outcome = []
        errors = []

        def exchange(*, code, check_cache):
            manager.exchanges.append((code, check_cache))
            entered.set()
            if not release.wait(timeout=2):
                raise RuntimeError("Test exchange was not released")
            return manager.token

        manager.get_access_token = exchange
        # Release a blocked fake exchange even if an earlier assertion fails.
        self.addCleanup(release.set)

        def first_request():
            try:
                outcome.append(self.callback(flow, port, state=flow.state, code=FAKE_CODE))
            except Exception as exc:
                errors.append(exc)

        first = threading.Thread(target=first_request, daemon=True, name="fake-oauth-client")
        first.start()
        self.assertTrue(entered.wait(timeout=1))
        queued = socket.create_connection(("127.0.0.1", port), timeout=1)
        try:
            path = "/callback?" + urlencode({"state": flow.state, "code": FAKE_CODE})
            queued.sendall(
                ("GET " + path + " HTTP/1.0\r\nHost: 127.0.0.1\r\n\r\n").encode("ascii")
            )
            release.set()
            first.join(timeout=2)
            self.assertFalse(first.is_alive())
            self.assertEqual(errors, [])
            self.assertEqual(outcome[0][0], 200)
            self.wait_closed(flow)
            queued.settimeout(1)
            try:
                replay_response = queued.recv(4096)
            except (ConnectionResetError, ConnectionAbortedError):
                replay_response = b""
            self.assertEqual(replay_response, b"", "A queued replay must not get another success response")
        finally:
            release.set()
            queued.close()
            first.join(timeout=2)
        self.assertEqual(manager.exchanges, [(FAKE_CODE, False)])
        success.assert_called_once_with(manager.token)
        self.assert_no_private_logs(flow.state)

    def test_success_callback_exception_is_terminal_and_not_logged(self):
        flow, manager, success, port = self.start_flow()
        success.side_effect = RuntimeError(FAKE_ACCESS + " " + FAKE_REFRESH)
        status, _, body = self.callback(flow, port, state=flow.state, code=FAKE_CODE)
        self.assertEqual(status, 502)
        self.wait_closed(flow)
        self.assertEqual(flow.status, "Authorization exchange failed")
        self.assertEqual(manager.exchanges, [(FAKE_CODE, False)])
        success.assert_called_once_with(manager.token)
        self.assertNotIn(FAKE_ACCESS, body)
        self.assertNotIn(FAKE_REFRESH, body)
        self.assert_no_private_logs(flow.state)

    def test_success_releases_private_callback_port_for_a_new_listener(self):
        flow, manager, success, port = self.start_flow()
        self.assertEqual(self.callback(flow, port, state=flow.state, code=FAKE_CODE)[0], 200)
        self.wait_closed(flow)
        # Use the same server's reuse policy, not platform-specific raw-socket
        # TIME_WAIT behavior, to verify the original listener is really gone.
        replacement = authorization.HTTPServer(("127.0.0.1", port), authorization.BaseHTTPRequestHandler)
        replacement.server_close()
        success.assert_called_once_with(manager.token)
        self.assertEqual(manager.exchanges, [(FAKE_CODE, False)])

    def test_exchange_exception_is_generic_terminal_and_logs_no_query_or_tokens(self):
        flow, manager, success, port = self.start_flow()
        private_url = f"http://127.0.0.1:{port}/callback?code={FAKE_CODE}&state={flow.state}"
        manager.exception = RuntimeError(private_url + " " + FAKE_ACCESS + " " + FAKE_REFRESH)
        status, _, body = self.callback(flow, port, state=flow.state, code=FAKE_CODE)
        self.assertEqual(status, 502)
        self.wait_closed(flow)
        self.assertEqual(flow.status, "Authorization exchange failed")
        self.assertEqual(manager.exchanges, [(FAKE_CODE, False)])
        success.assert_not_called()
        for value in (private_url, FAKE_CODE, FAKE_ACCESS, FAKE_REFRESH):
            self.assertNotIn(value, body)
        self.assert_no_private_logs(flow.state, private_url)

    def test_incomplete_token_never_reaches_success_callback(self):
        flow, manager, success, port = self.start_flow()
        manager.token = {"access_token": FAKE_ACCESS}
        status, _, body = self.callback(flow, port, state=flow.state, code=FAKE_CODE)
        self.assertEqual(status, 502)
        self.wait_closed(flow)
        self.assertEqual(flow.status, "Authorization exchange failed")
        success.assert_not_called()
        self.assertNotIn(FAKE_ACCESS, body)
        self.assert_no_private_logs(flow.state)

    def test_busy_private_port_fails_without_browser_or_worker(self):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
            listener.bind(("127.0.0.1", 0))
            listener.listen(1)
            manager = FakeAuthManager(listener.getsockname()[1])
            success = mock.Mock()
            flow = authorization.BrowserAuthorization(manager, success, open_browser=False)
            self.flows.append(flow)
            self.assertFalse(flow.start())
            self.assertTrue(flow.done.is_set())
            self.assertEqual(flow.status, "Spotify callback port is already in use")
            self.assertEqual(self.threads, [])
            self.assertEqual(manager.exchanges, [])
            success.assert_not_called()

    def test_expiry_closes_listener_without_exchange_or_success(self):
        flow, manager, success, port = self.start_flow(timeout=0.05)
        self.wait_closed(flow)
        self.assertEqual(flow.status, "Authorization timed out")
        with self.assertRaises(OSError):
            self.get(port, "/login")
        self.assertEqual(manager.exchanges, [])
        success.assert_not_called()
        self.assert_no_private_logs(flow.state)

    def test_explicit_close_ends_wait_without_authorizing(self):
        flow, manager, success, _ = self.start_flow()
        flow.close()
        self.wait_closed(flow)
        self.assertNotEqual(flow.status, "authorized")
        self.assertEqual(manager.exchanges, [])
        success.assert_not_called()

    def check_incomplete_request_timeout(self, *, host="127.0.0.1", explicit_close=False):
        flow, manager, success, port = self.start_flow(timeout=0.1, host=host)
        accepted = threading.Event()
        timeouts = []
        handler_type = flow._server.RequestHandlerClass
        original_setup = handler_type.setup

        def announced_setup(handler):
            original_setup(handler)
            timeouts.append(handler.connection.gettimeout())
            accepted.set()

        with mock.patch.object(handler_type, "setup", announced_setup):
            client = socket.create_connection((host, port), timeout=1)
            try:
                # Hold the HTTP headers open. Do not finish or close the client
                # to rescue the worker; production's read timeout must do that.
                client.sendall(b"GET /callback HTTP/1.1\r\nHost: localhost\r\n")
                self.assertTrue(accepted.wait(timeout=1))
                self.assertEqual(timeouts, [1.0])
                if explicit_close:
                    flow.close()
                self.wait_closed(flow)
                self.assertEqual(client.recv(4096), b"")
            finally:
                client.close()
        if not explicit_close:
            self.assertEqual(flow.status, "Authorization timed out")
        self.assertEqual(manager.exchanges, [])
        success.assert_not_called()
        self.assert_no_private_logs(flow.state)

    def test_incomplete_request_cannot_block_expiry_indefinitely(self):
        self.check_incomplete_request_timeout()

    def test_incomplete_request_cannot_block_explicit_close_indefinitely(self):
        self.check_incomplete_request_timeout(explicit_close=True)

    def test_ipv6_inherits_incomplete_request_read_timeout(self):
        if not socket.has_ipv6:
            self.skipTest("IPv6 is unavailable on this host")
        # Skip only when the OS cannot bind ::1, not for a failed production
        # timeout assertion or a callback/server implementation error.
        try:
            with socket.socket(socket.AF_INET6, socket.SOCK_STREAM) as probe:
                probe.bind(("::1", 0))
        except OSError:
            self.skipTest("IPv6 loopback is unavailable on this host")
        self.check_incomplete_request_timeout(host="::1")

    def test_wrong_path_and_large_request_are_rejected_without_logging_query(self):
        flow, manager, success, port = self.start_flow()
        status, _, _ = self.get(port, "/wrong?code=" + FAKE_CODE)
        self.assertEqual(status, 404)
        status, _, _ = self.get(port, "/callback?code=" + FAKE_CODE + "X" * 4100)
        self.assertEqual(status, 414)
        self.assertFalse(flow.done.is_set())
        self.assertEqual(manager.exchanges, [])
        success.assert_not_called()
        self.assert_no_private_logs(flow.state)

    def test_non_loopback_or_decorated_redirect_is_rejected_before_binding(self):
        for redirect in (
            "https://127.0.0.1:12345/callback",
            "http://spotify.example.invalid:12345/callback",
            "http://localhost:12345/callback",
            "http://127.0.0.1/callback",
            "http://user:pass@127.0.0.1:12345/callback",
            "http://127.0.0.1:12345/callback?query=private",
            "http://127.0.0.1:12345/callback#fragment",
        ):
            with self.subTest(redirect_kind=urlsplit(redirect).hostname):
                manager = FakeAuthManager(12345)
                manager.redirect_uri = redirect
                success = mock.Mock()
                flow = authorization.BrowserAuthorization(manager, success, open_browser=False)
                self.flows.append(flow)
                with mock.patch.object(authorization, "HTTPServer") as server:
                    self.assertFalse(flow.start())
                    server.assert_not_called()
                self.assertTrue(flow.done.is_set())
                self.assertEqual(flow.status, "A registered HTTP loopback redirect URI is required")
                self.assertEqual(manager.exchanges, [])
                success.assert_not_called()


if __name__ == "__main__":
    unittest.main()
