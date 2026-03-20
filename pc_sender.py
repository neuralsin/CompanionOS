"""
ESP8266 Video Streamer — PC Sender
====================================
Streams a video file to an ESP8266 over WiFi UDP with JPEG compression.

Requirements:
    pip install opencv-python

Usage:
    python pc_sender.py                        # streams default video
    python pc_sender.py --video myvideo.mp4   # custom video
    python pc_sender.py --ip 192.168.1.42      # custom ESP IP
    python pc_sender.py --quality 30 --fps 25  # tune quality/fps

Protocol (UDP packet layout):
    Byte 0-1 : frame_id  (uint16, wraps at 65535)
    Byte 2   : chunk_id  (uint8,  0-indexed)
    Byte 3   : num_chunks(uint8,  total chunks for this frame)
    Byte 4-7 : frame_len (uint32, total JPEG bytes in this frame)
    Byte 8   : checksum  (uint8,  XOR of bytes 0-7)
    Byte 9+  : JPEG payload chunk
"""

import cv2
import socket
import struct
import time
import argparse
import sys

# ─── Default Configuration ─────────────────────────────────────────────────────
DEFAULT_VIDEO   = "video.mp4"   # Path to your video file
DEFAULT_ESP_IP  = "192.168.4.1" # ESP8266 IP (set after it connects to your WiFi)
DEFAULT_PORT    = 5005           # Must match ESP8266 sketch
DEFAULT_FPS     = 20             # Target frames per second
DEFAULT_QUALITY = 35             # JPEG quality (20–50 recommended; lower = faster)
DEFAULT_WIDTH   = 240            # TFT width  (match your display)
DEFAULT_HEIGHT  = 320            # TFT height (match your display)
CHUNK_SIZE      = 1024           # UDP payload per packet (keep ≤ 1400 for safety)
HEADER_SIZE     = 9              # Bytes of our custom header
# ───────────────────────────────────────────────────────────────────────────────


def build_header(frame_id: int, chunk_id: int, num_chunks: int, frame_len: int) -> bytes:
    """Pack the 9-byte packet header."""
    raw = struct.pack(">HBBL", frame_id & 0xFFFF, chunk_id, num_chunks, frame_len)
    checksum = 0
    for b in raw:
        checksum ^= b
    return raw + bytes([checksum])


def send_frame(sock: socket.socket, jpeg_bytes: bytes, frame_id: int,
               esp_addr: tuple, stats: dict) -> None:
    """Fragment a JPEG frame into chunks and send via UDP."""
    total_len   = len(jpeg_bytes)
    num_chunks  = (total_len + CHUNK_SIZE - 1) // CHUNK_SIZE

    if num_chunks > 255:
        print(f"[WARN] Frame {frame_id} too large ({total_len}B), skipping.")
        return

    for i in range(num_chunks):
        chunk   = jpeg_bytes[i * CHUNK_SIZE : (i + 1) * CHUNK_SIZE]
        header  = build_header(frame_id, i, num_chunks, total_len)
        packet  = header + chunk
        sock.sendto(packet, esp_addr)

        # Micro-delay between chunks prevents ESP8266 UDP buffer overflow
        # 0.3ms gives ~3MB/s max throughput — well above our needs
        time.sleep(0.0003)

    stats["bytes_sent"] += total_len + num_chunks * HEADER_SIZE
    stats["frames_sent"] += 1


def run(args):
    cap = cv2.VideoCapture(args.video)
    if not cap.isOpened():
        sys.exit(f"[ERROR] Cannot open video: {args.video}")

    sock     = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 65536)
    esp_addr = (args.ip, args.port)

    encode_params = [cv2.IMWRITE_JPEG_QUALITY, args.quality]
    frame_interval = 1.0 / args.fps

    stats = {
        "frames_sent": 0,
        "bytes_sent": 0,
        "t_start": time.time(),
        "fps_window": [],
    }

    frame_id   = 0
    t_last_fps = time.time()

    print(f"[INFO] Streaming '{args.video}' → {args.ip}:{args.port}")
    print(f"[INFO] Resolution: {args.width}×{args.height}  Quality: {args.quality}  Target FPS: {args.fps}")
    print("[INFO] Press Ctrl+C to stop.\n")

    try:
        while True:
            t_frame_start = time.time()

            ret, frame = cap.read()
            if not ret:
                # Loop video
                cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
                ret, frame = cap.read()
                if not ret:
                    break

            # Resize to TFT resolution
            frame = cv2.resize(frame, (args.width, args.height),
                               interpolation=cv2.INTER_LINEAR)

            # Encode to JPEG
            ok, jpeg_buf = cv2.imencode(".jpg", frame, encode_params)
            if not ok:
                continue
            jpeg_bytes = jpeg_buf.tobytes()

            # Send
            send_frame(sock, jpeg_bytes, frame_id, esp_addr, stats)
            frame_id = (frame_id + 1) & 0xFFFF

            # Console FPS readout every second
            now = time.time()
            stats["fps_window"].append(now)
            stats["fps_window"] = [t for t in stats["fps_window"] if now - t <= 1.0]
            if now - t_last_fps >= 1.0:
                actual_fps  = len(stats["fps_window"])
                kbps        = stats["bytes_sent"] / max(1, now - stats["t_start"]) / 1024
                jpeg_kb     = len(jpeg_bytes) / 1024
                print(f"  FPS: {actual_fps:>3}  |  JPEG size: {jpeg_kb:>5.1f} KB  |  Throughput: {kbps:>6.1f} KB/s")
                t_last_fps = now

            # Sleep to hit target FPS (account for encoding + send time)
            elapsed   = time.time() - t_frame_start
            sleep_for = frame_interval - elapsed
            if sleep_for > 0:
                time.sleep(sleep_for)

    except KeyboardInterrupt:
        print("\n[INFO] Stopped by user.")
    finally:
        cap.release()
        sock.close()
        elapsed = time.time() - stats["t_start"]
        print(f"\n[INFO] Sent {stats['frames_sent']} frames in {elapsed:.1f}s "
              f"({stats['bytes_sent'] / 1024:.0f} KB total)")


def main():
    parser = argparse.ArgumentParser(description="Stream video to ESP8266 TFT display")
    parser.add_argument("--video",   default=DEFAULT_VIDEO,   help="Input video file")
    parser.add_argument("--ip",      default=DEFAULT_ESP_IP,  help="ESP8266 IP address")
    parser.add_argument("--port",    default=DEFAULT_PORT,    type=int)
    parser.add_argument("--fps",     default=DEFAULT_FPS,     type=int,   help="Target FPS")
    parser.add_argument("--quality", default=DEFAULT_QUALITY, type=int,   help="JPEG quality 10-80")
    parser.add_argument("--width",   default=DEFAULT_WIDTH,   type=int,   help="Frame width")
    parser.add_argument("--height",  default=DEFAULT_HEIGHT,  type=int,   help="Frame height")
    args = parser.parse_args()
    run(args)


if __name__ == "__main__":
    main()
