"""One-time, acknowledged Spotify provisioning to the configured private deskbot."""

import argparse
import contextlib
import errno
import ipaddress
import json
import logging
import math
import os
import socket
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PC_PORT_RX = 8889
MAX_PACKET = 1800
MAX_ATTEMPTS = 3
PRIVATE_NETWORKS = tuple(ipaddress.IPv4Network(net) for net in
                         ("10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16"))


class ProvisionError(Exception):
    """Only fixed, credential-free messages may be exposed to the caller."""


def target_from_config(config):
    try:
        network = config["network"]
        if not isinstance(network["esp_ip"], str):
            raise ValueError
        address = ipaddress.IPv4Address(network["esp_ip"])
        port = network["esp_port_rx"]
        receive_port = network["pc_port_rx"]
        if (not any(address in net for net in PRIVATE_NETWORKS)
                or int(str(address).split(".")[-1]) in (0, 255)
                or type(port) is not int or not 1 <= port <= 65535
                or type(receive_port) is not int or receive_port != PC_PORT_RX):
            raise ValueError
        return str(address), port
    except (KeyError, TypeError, ValueError, ipaddress.AddressValueError):
        raise ProvisionError("Invalid network configuration: a private unicast IPv4 deskbot and PC port 8889 are required.") from None


def cached_token(config):
    # Reading the existing cache must not launch consent, refresh, or prompt stdin.
    from spotipy.cache_handler import CacheFileHandler

    settings = config["spotify"]
    client_id = os.getenv("SPOTIFY_CLIENT_ID", settings["client_id"])
    secret = os.getenv("SPOTIFY_CLIENT_SECRET", settings["client_secret"])
    token = CacheFileHandler(cache_path=str(ROOT / "python" / ".cache")).get_cached_token()
    if (not isinstance(token, dict)
            or type(token.get("expires_at")) not in (int, float)
            or not math.isfinite(token["expires_at"])
            or token["expires_at"] <= time.time()):
        raise ValueError
    return token, client_id, secret


def auth_packet(token, client_id, secret):
    fields = {"access": token.get("access_token"), "refresh": token.get("refresh_token"),
              "client_id": client_id, "client_secret": secret}
    limits = {"access": 2048, "refresh": 1024, "client_id": 128, "client_secret": 256}
    if any(not isinstance(value, str) or len(value) >= limits[name]
           or (not value and name != "client_secret")
           or any(ord(c) <= 32 or ord(c) >= 127 for c in value)
           for name, value in fields.items()):
        raise ProvisionError("Cached Spotify credentials are incomplete; complete authorization first.")
    packet = ("SPOTIFY_AUTH:" + json.dumps(fields, separators=(",", ":"),
                                          ensure_ascii=True)).encode("utf-8")
    if len(packet) > MAX_PACKET:
        raise ProvisionError("Spotify credentials exceed the firmware packet limit; nothing was transmitted.")
    return packet


def open_receiver():
    receiver = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        # Windows needs EXCLUSIVEADDRUSE; never opt into REUSEADDR/REUSEPORT.
        if hasattr(socket, "SO_EXCLUSIVEADDRUSE"):
            receiver.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        receiver.bind(("0.0.0.0", PC_PORT_RX))
        return receiver
    except OSError as error:
        receiver.close()
        if error.errno in (errno.EADDRINUSE, errno.EACCES) or getattr(error, "winerror", None) in (10048, 10013):
            raise ProvisionError("UDP 8889 is unavailable. If the controller is already running, it will provision the deskbot on connection; no process was stopped. This tool did not confirm delivery.") from None
        raise ProvisionError("Could not exclusively bind UDP 8889; nothing was transmitted.") from None


def wait_for(receiver, expected, message, deadline, clock):
    while True:
        remaining = deadline - clock()
        if remaining <= 0:
            return False
        receiver.settimeout(remaining)
        try:
            data, peer = receiver.recvfrom(2048)
        except socket.timeout:
            return False
        if peer == expected and data == message:
            return True


def provision(config, timeout, *, token_loader=cached_token, socket_factory=open_receiver,
              clock=time.monotonic, report=print):
    if not isinstance(timeout, (int, float)) or not math.isfinite(timeout) or timeout <= 0:
        raise ProvisionError("Timeout must be a finite positive number of seconds.")
    peer = target_from_config(config)
    deadline = clock() + timeout
    with socket_factory() as receiver:
        # Suppress dependency output, warnings, and logs even on cache failures.
        previous_logging = logging.root.manager.disable
        try:
            logging.disable(sys.maxsize)
            with open(os.devnull, "w") as sink, contextlib.redirect_stdout(sink), contextlib.redirect_stderr(sink):
                token, client_id, secret = token_loader(config)
                packet = auth_packet(token, client_id, secret)
        except ProvisionError:
            raise
        except Exception:
            raise ProvisionError("No valid cached Spotify authorization is available; complete authorization first. Nothing was transmitted.") from None
        finally:
            logging.disable(previous_logging)

        if clock() >= deadline:
            raise ProvisionError("Provisioning timed out before discovery; nothing was transmitted.")
        report("Probing the configured deskbot only; credential broadcast is disabled.")
        hello_deadline = clock() + (deadline - clock()) / 2
        receiver.sendto(b"COMPANION_PROBE:PC", peer)
        if not wait_for(receiver, peer, b"HELLO_COMPANION", hello_deadline, clock):
            raise ProvisionError("No greeting from the exact configured deskbot; no credentials were transmitted.")

        report("Expected deskbot confirmed; awaiting provisioning acknowledgement.")
        for attempt in range(MAX_ATTEMPTS):
            now = clock()
            if now >= deadline:
                break
            attempt_deadline = now + (deadline - now) / (MAX_ATTEMPTS - attempt)
            receiver.sendto(packet, peer)
            if wait_for(receiver, peer, b"SPOTIFY_AUTH_OK", attempt_deadline, clock):
                report("Deskbot acknowledged Spotify provisioning. Persistence across restart was not tested.")
                return 0
        raise ProvisionError("No SPOTIFY_AUTH_OK from the exact configured deskbot. Delivery/persistence is NOT confirmed; older firmware may not support acknowledgements.")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--timeout", type=float, default=8.0,
                        help="total provisioning deadline in seconds (default: 8)")
    parser.add_argument("--no-broadcast", action="store_true", required=True,
                        help="required safety acknowledgement; only configured ESP_IP is contacted")
    args = parser.parse_args(argv)
    try:
        config = json.loads((ROOT / "python" / "config.json").read_text(encoding="utf-8"))
        return provision(config, args.timeout)
    except ProvisionError as error:
        print(str(error))
    except KeyboardInterrupt:
        print("Provisioning cancelled; delivery/persistence is not confirmed.")
    except Exception:
        # Never format dependency/socket/config exceptions: they can hold secrets.
        print("Provisioning failed; delivery/persistence is not confirmed. Check local configuration, dependencies, and network access.")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
