"""
ESP8266 Video Streamer — WiFi UDP Sender
=========================================
Streams a video file to an ESP8266 over WiFi UDP with JPEG compression.
Targets 30-40 fps depending on video content complexity.

Requirements:
    pip install opencv-python

Usage:
    python pc_sender.py --video myvideo.mp4 --ip 192.168.x.x
    python pc_sender.py --video myvideo.mp4 --ip 192.168.x.x --fps 30
    python pc_sender.py --video myvideo.mp4 --ip 192.168.x.x --quality 45 --fps 30

    The IP address is shown on the TFT display after the ESP connects to WiFi.

UDP packet format:
    [0-1]  uint16 BE  — frame_id  (wraps at 65535)
    [2]    uint8      — chunk_id  (0-indexed)
    [3]    uint8      — num_chunks
    [4-7]  uint32 BE  — total JPEG length for this frame
    [8]    uint8      — XOR checksum of bytes 0-7
    [9+]   JPEG payload chunk (up to 1024 bytes)
"""

import cv2
import socket
import struct
import time
import argparse
import sys

# ─── Defaults ──────────────────────────────────────────────────────────────────
DEFAULT_PORT    = 5005
DEFAULT_FPS     = 45
DEFAULT_QUALITY = 35
DEFAULT_WIDTH   = 240
DEFAULT_HEIGHT  = 320
CHUNK_SIZE      = 1024
HEADER_SIZE     = 9
# ───────────────────────────────────────────────────────────────────────────────


def build_header(frame_id: int, chunk_id: int, num_chunks: int, frame_len: int) -> bytes:
    raw = struct.pack(">HBBL", frame_id & 0xFFFF, chunk_id, num_chunks, frame_len)
    checksum = 0
    for b in raw:
        checksum ^= b
    return raw + bytes([checksum])


def send_frame(sock, jpeg_bytes: bytes, frame_id: int, esp_addr: tuple, stats: dict):
    total     = len(jpeg_bytes)
    n_chunks  = (total + CHUNK_SIZE - 1) // CHUNK_SIZE

    if n_chunks > 255:
        print(f"[WARN] Frame {frame_id} too large ({total}B), skipping.")
        return

    for i in range(n_chunks):
        chunk  = jpeg_bytes[i * CHUNK_SIZE : (i + 1) * CHUNK_SIZE]
        header = build_header(frame_id, i, n_chunks, total)
        sock.sendto(header + chunk, esp_addr)
        # Small inter-chunk delay prevents UDP buffer overflow on the ESP.
        # 0.2ms gives ~5MB/s max — far above what we need.
        time.sleep(0.0002)

    stats["bytes"]  += total + n_chunks * HEADER_SIZE
    stats["frames"] += 1


def run(args):
    if not args.ip:
        sys.exit("[ERROR] --ip is required. Check the IP shown on the TFT display.")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 131072)
    esp_addr = (args.ip, args.port)

    cap = cv2.VideoCapture(args.video)
    if not cap.isOpened():
        sys.exit(f"[ERROR] Cannot open video: {args.video}")

    encode_params  = [cv2.IMWRITE_JPEG_QUALITY, args.quality]
    frame_interval = 1.0 / args.fps

    stats = {
        "frames"     : 0,
        "bytes"      : 0,
        "t_start"    : time.time(),
        "fps_window" : [],
    }

    print(f"[INFO] Streaming '{args.video}' → {args.ip}:{args.port}")
    print(f"[INFO] Resolution: {args.width}×{args.height}  "
          f"Quality: {args.quality}  Target FPS: {args.fps}")
    print("[INFO] Press Ctrl+C to stop.\n")

    frame_id      = 0
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

            frame = cv2.resize(frame, (args.width, args.height),
                               interpolation=cv2.INTER_LINEAR)

            ok, jpeg_buf = cv2.imencode(".jpg", frame, encode_params)
            if not ok:
                continue
            jpeg_bytes = jpeg_buf.tobytes()

            send_frame(sock, jpeg_bytes, frame_id, esp_addr, stats)
            frame_id = (frame_id + 1) & 0xFFFF

            # FPS report
            now = time.time()
            stats["fps_window"].append(now)
            stats["fps_window"] = [t for t in stats["fps_window"] if now - t <= 1.0]

            if now - t_last_report >= 1.0:
                actual_fps = len(stats["fps_window"])
                kbps       = stats["bytes"] / max(1, now - stats["t_start"]) / 1024
                jpeg_kb    = len(jpeg_bytes) / 1024
                print(f"  FPS: {actual_fps:>3}  |  "
                      f"JPEG: {jpeg_kb:>5.1f} KB  |  "
                      f"Throughput: {kbps:>7.1f} KB/s")
                t_last_report = now

            elapsed   = time.time() - t0
            sleep_for = frame_interval - elapsed
            if sleep_for > 0:
                time.sleep(sleep_for)

    except KeyboardInterrupt:
        print("\n[INFO] Stopped.")
    finally:
        cap.release()
        sock.close()
        elapsed = time.time() - stats["t_start"]
        print(f"\n[INFO] Sent {stats['frames']} frames in {elapsed:.1f}s "
              f"({stats['bytes'] / 1024:.0f} KB total)")


def main():
    parser = argparse.ArgumentParser(
        description="Stream video to ESP8266 TFT over WiFi UDP"
    )
    parser.add_argument("--video",   default="video.mp4",
                        help="Input video file (default: video.mp4)")
    parser.add_argument("--ip",      required=True,
                        help="ESP8266 IP address shown on TFT after boot")
    parser.add_argument("--port",    default=DEFAULT_PORT,    type=int)
    parser.add_argument("--fps",     default=DEFAULT_FPS,     type=int,
                        help=f"Target FPS (default: {DEFAULT_FPS})")
    parser.add_argument("--quality", default=DEFAULT_QUALITY, type=int,
                        help=f"JPEG quality 10-80 (default: {DEFAULT_QUALITY})")
    parser.add_argument("--width",   default=DEFAULT_WIDTH,   type=int)
    parser.add_argument("--height",  default=DEFAULT_HEIGHT,  type=int)
    args = parser.parse_args()
    run(args)


if __name__ == "__main__":
    main()
