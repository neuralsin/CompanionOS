#!/usr/bin/env python3
"""
═══════════════════════════════════════════════════════════
  PC DASHBOARD WIRELESS SENDER (One-Way Telemetry)
  CompanionOS & ESP32-Wireless-PC-Dashboard Compatible
  Pushes CPU, RAM, GPU, VRAM, Temperatures, Net, Disk to ESP
═══════════════════════════════════════════════════════════
"""

import os
import sys
import time
import socket
import json

# Ensure local modules importable
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)

from pc_dashboard_service import PCDashboardTelemetry

# Load config if present
CONFIG_FILE = os.path.join(SCRIPT_DIR, "config.json")
ESP_IP = "192.168.1.187"
ESP_PORT = 8888
SECONDARY_PORT = 4210

if os.path.exists(CONFIG_FILE):
    try:
        with open(CONFIG_FILE, "r") as f:
            cfg = json.load(f)
            ESP_IP = cfg.get("network", {}).get("esp_ip", ESP_IP)
            ESP_PORT = cfg.get("network", {}).get("esp_port_rx", ESP_PORT)
    except Exception:
        pass

if len(sys.argv) > 1:
    ESP_IP = sys.argv[1]
if len(sys.argv) > 2:
    ESP_PORT = int(sys.argv[2])

UPDATE_INTERVAL = 0.5

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
try:
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
except Exception:
    pass

telemetry = PCDashboardTelemetry()

print("═══════════════════════════════════════════════════════")
print("  CompanionOS — Wireless PC Dashboard Telemetry Sender")
print("  Mode: ONE-WAY DATA RECEIVING (No control overhead)")
print(f"  Target: {ESP_IP} (Ports: {ESP_PORT}, {SECONDARY_PORT})")
print("  Telemetry: CPU, RAM, GPU, VRAM, Temps, Net, Disk")
print("  Press Ctrl+C to stop.")
print("═══════════════════════════════════════════════════════\n")

try:
    while True:
        data = telemetry.collect()
        packet = telemetry.format_packet(data)

        if packet:
            # Send to main CompanionOS port
            try:
                sock.sendto(packet.encode("utf-8"), (ESP_IP, ESP_PORT))
            except Exception:
                pass

            # Also send to port 4210 (InnoCircuitsLab default) for dual-port reception
            try:
                sock.sendto(packet.encode("utf-8"), (ESP_IP, SECONDARY_PORT))
            except Exception:
                pass

            gpu_txt = f"{data['gpu']:4.1f}%" if data['gpu'] > 0 else "N/A"
            temp_txt = f"{data['gpu_temp']:.0f}°C" if data['gpu_temp'] > 0 else "N/A"
            lat_txt = f"{data['latency']:.0f}ms" if data['latency'] >= 0 else "OFF"

            print(
                f"\rCPU: {data['cpu']:4.1f}% | "
                f"RAM: {data['ram']:4.1f}% | "
                f"GPU: {gpu_txt} ({temp_txt}) | "
                f"VRAM: {data['vram']:4.1f}% | "
                f"NET: {lat_txt:>5} | "
                f"TOP: {data['top_ram_name'][:12]:12}",
                end="",
                flush=True
            )

        time.sleep(UPDATE_INTERVAL)

except KeyboardInterrupt:
    print("\n\nStopped.")
finally:
    sock.close()
