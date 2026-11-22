"""Bounded browser OAuth callback server. Never log codes, tokens, or URLs."""

import hmac
import secrets
import socket
import threading
import time
import webbrowser
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import parse_qs, urlsplit


class BrowserAuthorization:
    def __init__(self, auth_manager, on_success, *, timeout=300, open_browser=True):
        self.auth_manager = auth_manager
        self.on_success = on_success
        self.timeout = timeout
        self.open_browser = open_browser
        self.state = secrets.token_urlsafe(32)
        self.done = threading.Event()
        self.status = "idle"
        self.login_url = ""
        self._server = None

    def start(self):
        redirect = urlsplit(self.auth_manager.redirect_uri)
        if (redirect.scheme != "http" or redirect.hostname not in {"127.0.0.1", "::1"}
                or not redirect.port or redirect.query or redirect.fragment
                or redirect.username or redirect.password):
            self.status = "A registered HTTP loopback redirect URI is required"
            self.done.set()
            return False
        flow = self
        callback_path = redirect.path or "/"

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *_args):
                pass

            def page(self, code, message):
                body = ("<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width'>"
                        "<title>CompanionOS Spotify</title><style>body{background:#101414;color:#f0f4f2;"
                        "font:16px system-ui;max-width:560px;margin:15vh auto;padding:24px}"
                        "h1{font-size:26px;color:#1ed760}</style><h1>CompanionOS</h1><p>"
                        + message + "</p><script>history.replaceState(null,'','/spotify-result')</script>").encode("utf-8")
                self.send_response(code)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Cache-Control", "no-store")
                self.send_header("Referrer-Policy", "no-referrer")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            def do_GET(self):
                if len(self.path) > 4096:
                    self.page(414, "Authorization response is too large.")
                    return
                parsed = urlsplit(self.path)
                if parsed.path == "/login":
                    self.send_response(302)
                    self.send_header("Location", flow.auth_manager.get_authorize_url(state=flow.state))
                    self.send_header("Cache-Control", "no-store")
                    self.send_header("Content-Length", "0")
                    self.end_headers()
                    return
                if parsed.path != callback_path:
                    self.page(404, "Not found.")
                    return
                query = parse_qs(parsed.query, keep_blank_values=True)
                states = query.get("state", [])
                if len(states) != 1 or not states[0].isascii() or not hmac.compare_digest(states[0], flow.state):
                    self.page(400, "Authorization state did not match. Return to the original sign-in tab.")
                    return
                if "error" in query:
                    flow.status = "Authorization declined"
                    self.page(403, "Spotify authorization was declined. You can try again from CompanionOS.")
                    flow.done.set()
                    return
                codes = query.get("code", [])
                if len(codes) != 1 or not codes[0] or len(codes[0]) > 2048:
                    self.page(400, "Spotify did not return an authorization code.")
                    return
                try:
                    token = flow.auth_manager.get_access_token(code=codes[0], check_cache=False)
                    if not isinstance(token, dict) or not token.get("access_token") or not token.get("refresh_token"):
                        raise ValueError("Incomplete authorization")
                    flow.on_success(token)
                    flow.status = "authorized"
                    self.page(200, "Spotify connected. The controller will sync your deskbot automatically. You can close this tab.")
                except Exception:
                    flow.status = "Authorization exchange failed"
                    self.page(502, "Spotify authorization could not be completed. Check the registered app and try again.")
                finally:
                    flow.done.set()

        try:
            class LoopbackServer(HTTPServer):
                def get_request(self):
                    connection, address = super().get_request()
                    connection.settimeout(1.0)
                    return connection, address

            server_type = LoopbackServer
            if redirect.hostname == "::1":
                class IPv6Server(LoopbackServer):
                    address_family = socket.AF_INET6
                server_type = IPv6Server
            self._server = server_type((redirect.hostname, redirect.port), Handler)
            self._server.timeout = 0.25
        except OSError:
            self.status = "Spotify callback port is already in use"
            self.done.set()
            return False
        self.login_url = f"http://{redirect.netloc}/login"
        self.status = "waiting for browser"
        threading.Thread(target=self._serve, daemon=True, name="spotify-authorization").start()
        if self.open_browser:
            webbrowser.open(self.login_url, new=2)
        return True

    def _serve(self):
        deadline = time.monotonic() + self.timeout
        try:
            while not self.done.is_set() and time.monotonic() < deadline:
                self._server.handle_request()
            if not self.done.is_set():
                self.status = "Authorization timed out"
        finally:
            self._server.server_close()
            self.done.set()

    def close(self):
        self.done.set()
