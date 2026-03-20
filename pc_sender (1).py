"""
ESP8266 Video Streamer — USB Serial Sender
==========================================
Streams a video file to an ESP8266 over USB serial at 2,000,000 baud
using JPEG compression.

Requirements:
    pip install opencv-python pyserial

Usage:
    python pc_sender.py                              # auto-detect COM port
    python pc_sender.py --port COM3                  # Windows
    python pc_sender.py --port /dev/ttyUSB0          # Linux
    python pc_sender.py --port /dev/cu.usbserial-0001  # macOS
    python pc_sender.py --video myvideo.mp4 --quality 30 --fps 20

Frame wire format:
    [0-3]  0xFF 0xAA 0xFF 0xAA   — start marker
    [4-7]  uint32 little-endian  — JPEG byte count
    [8]    uint8                 — XOR checksum of bytes 4-7
    [9+]   JPEG payload
"""

import cv2
import serial
import serial.tools.list_ports
import struct
import time
import argparse
import sys

# ─── Default configuration ─────────────────────────────────────────────────────
DEFAULT_VIDEO   = "video.mp4"
DEFAULT_BAUD    = 2000000
DEFAULT_FPS     = 20
DEFAULT_QUALITY = 35
DEFAULT_WIDTH   = 320
DEFAULT_HEIGHT  = 240
# ───────────────────────────────────────────────────────────────────────────────

MARKER = bytes([0xFF, 0xAA, 0xFF, 0xAA])


def auto_detect_port() -> str:
    """Try to find the ESP8266 serial port automatically."""
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        return None

    # Prefer ports with ESP/CH340/CP210x in the description
    for p in ports:
        desc = (p.description or "").lower()
        if any(kw in desc for kw in ["ch340", "cp210", "esp", "uart", "usb serial"]):
            return p.device

    # Fall back to first available port
    return ports[0].device


def build_packet(jpeg_bytes: bytes) -> bytes:
    """Wrap a JPEG buffer in the frame protocol."""
    length = len(jpeg_bytes)
    # Little-endian uint32
    len_bytes = struct.pack("<I", length)
    # XOR checksum of the 4 length bytes
    checksum = len_bytes[0] ^ len_bytes[1] ^ len_bytes[2] ^ len_bytes[3]
    return MARKER + len_bytes + bytes([checksum]) + jpeg_bytes


def run(args):
    # ── Open serial port ──
    port = args.port or auto_detect_port()
    if not port:
        sys.exit("[ERROR] No serial port found. Use --port to specify manually.")

    print(f"[INFO] Opening {port} at {args.baud} baud...")
    try:
        ser = serial.Serial(
            port     = port,
            baudrate = args.baud,
            timeout  = 1,
            write_timeout = 2,
        )
    except serial.SerialException as e:
        sys.exit(f"[ERROR] Could not open port: {e}")

    # Give the ESP8266 time to reset after the serial port opens
    # (opening DTR/RTS lines triggers the reset circuit on NodeMCU/Wemos)
    print("[INFO] Waiting 2s for ESP8266 to boot after port open...")
    time.sleep(2)
    ser.reset_input_buffer()
    ser.reset_output_buffer()

    # ── Open video ──
    cap = cv2.VideoCapture(args.video)
    if not cap.isOpened():
        ser.close()
        sys.exit(f"[ERROR] Cannot open video: {args.video}")

    encode_params  = [cv2.IMWRITE_JPEG_QUALITY, args.quality]
    frame_interval = 1.0 / args.fps

    stats = {
        "frames"     : 0,
        "bytes"      : 0,
        "t_start"    : time.time(),
        "fps_window" : [],
    }

    print(f"[INFO] Streaming '{args.video}' → {port}")
    print(f"[INFO] Resolution: {args.width}×{args.height}  "
          f"Quality: {args.quality}  Target FPS: {args.fps}")
    print("[INFO] Press Ctrl+C to stop.\n")

    t_last_report = time.time()

    try:
        while True:
            t0 = time.time()

            ret, frame = cap.read()
            if not ret:
                cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
                ret, frame = cap.read()
                if not ret:
                    break

            # Resize
            frame = cv2.resize(frame, (args.width, args.height),
                               interpolation=cv2.INTER_LINEAR)

            # JPEG encode
            ok, jpeg_buf = cv2.imencode(".jpg", frame, encode_params)
            if not ok:
                continue
            jpeg_bytes = jpeg_buf.tobytes()

            # Build and send packet
            packet = build_packet(jpeg_bytes)
            try:
                ser.write(packet)
                ser.flush()
            except serial.SerialTimeoutException:
                print("[WARN] Serial write timeout — ESP may be overwhelmed, skipping frame.")
                continue

            stats["frames"] += 1
            stats["bytes"]  += len(packet)

            # Console report every second
            now = time.time()
            stats["fps_window"].append(now)
            stats["fps_window"] = [t for t in stats["fps_window"] if now - t <= 1.0]

            if now - t_last_report >= 1.0:
                actual_fps = len(stats["fps_window"])
                kbps       = stats["bytes"] / max(1, now - stats["t_start"]) / 1024
                jpeg_kb    = len(jpeg_bytes) / 1024
                print(f"  FPS: {actual_fps:>3}  |  "
                      f"JPEG: {jpeg_kb:>5.1f} KB  |  "
                      f"Packet: {len(packet):>6} B  |  "
                      f"Avg throughput: {kbps:>6.1f} KB/s")
                t_last_report = now

            # Pace to target FPS
            elapsed   = time.time() - t0
            sleep_for = frame_interval - elapsed
            if sleep_for > 0:
                time.sleep(sleep_for)

    except KeyboardInterrupt:
        print("\n[INFO] Stopped.")
    finally:
        cap.release()
        ser.close()
        elapsed = time.time() - stats["t_start"]
        print(f"\n[INFO] Sent {stats['frames']} frames in {elapsed:.1f}s  "
              f"({stats['bytes'] / 1024:.0f} KB total)")


def main():
    parser = argparse.ArgumentParser(
        description="Stream video to ESP8266 TFT display over USB serial"
    )
    parser.add_argument("--video",   default=DEFAULT_VIDEO,
                        help="Input video file (default: video.mp4)")
    parser.add_argument("--port",    default=None,
                        help="Serial port (e.g. COM3 or /dev/ttyUSB0). "
                             "Auto-detected if omitted.")
    parser.add_argument("--baud",    default=DEFAULT_BAUD,    type=int,
                        help=f"Baud rate (default: {DEFAULT_BAUD})")
    parser.add_argument("--fps",     default=DEFAULT_FPS,     type=int,
                        help=f"Target FPS (default: {DEFAULT_FPS})")
    parser.add_argument("--quality", default=DEFAULT_QUALITY, type=int,
                        help=f"JPEG quality 10-80 (default: {DEFAULT_QUALITY})")
    parser.add_argument("--width",   default=DEFAULT_WIDTH,   type=int,
                        help=f"Frame width  (default: {DEFAULT_WIDTH})")
    parser.add_argument("--height",  default=DEFAULT_HEIGHT,  type=int,
                        help=f"Frame height (default: {DEFAULT_HEIGHT})")
    args = parser.parse_args()
    run(args)


if __name__ == "__main__":
    main()
