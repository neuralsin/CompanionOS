#!/usr/bin/env python3
"""
═══════════════════════════════════════════════════════════
  COMPANION OS v7.0 — BLUETOOTH BRIDGE
  
  Classic Bluetooth SPP bridge for PC↔ESP text commands.
  Text-only — no binary stream (image data goes via WiFi).
  
  Uses pyserial to connect to BT COM port on Windows.
  
  Usage:
    python bluetooth_bridge.py
    
  On first run, pair your ESP32 in Windows Bluetooth settings.
  The ESP32 advertises as "CompanionOS".
═══════════════════════════════════════════════════════════
"""

import serial
import serial.tools.list_ports
import time
import threading
import json
import sys
import socket


# ═══════════════════════════════════════════════════════════
# CONFIG
# ═══════════════════════════════════════════════════════════

BT_BAUD = 115200
BT_DEVICE_NAME = "CompanionOS"
UDP_PORT_TX = 8888  # Same port as WiFi UDP, for local relay
UDP_PORT_RX = 8889

# ═══════════════════════════════════════════════════════════
# AUTO-DETECT BT COM PORT
# ═══════════════════════════════════════════════════════════

def find_bt_port():
    """Scan COM ports for the ESP32 Bluetooth Serial device."""
    ports = serial.tools.list_ports.comports()
    
    # Look for BT port by description
    for p in ports:
        desc = (p.description or "").lower()
        hwid = (p.hwid or "").lower()
        if "bluetooth" in desc or "bthenum" in hwid:
            print(f"[BT] Found Bluetooth port: {p.device} ({p.description})")
            return p.device
    
    # Fallback: list all and let user pick
    if ports:
        print("\n[BT] Available COM ports:")
        for i, p in enumerate(ports):
            print(f"  [{i}] {p.device} — {p.description}")
        
        try:
            choice = int(input("\nSelect port number: "))
            return ports[choice].device
        except (ValueError, IndexError):
            pass
    
    print("[BT] No COM ports found. Is the ESP32 paired?")
    return None


# ═══════════════════════════════════════════════════════════
# BLUETOOTH SERIAL BRIDGE
# ═══════════════════════════════════════════════════════════

class BluetoothBridge:
    def __init__(self, port):
        self.port = port
        self.ser = None
        self.running = False
        self.connected = False
        self.udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.udp_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        
    def connect(self):
        """Open BT serial connection."""
        try:
            self.ser = serial.Serial(
                port=self.port,
                baudrate=BT_BAUD,
                timeout=1.0,
                write_timeout=2.0
            )
            self.connected = True
            print(f"[BT] Connected to {self.port} @ {BT_BAUD}")
            return True
        except serial.SerialException as e:
            print(f"[BT] Connection failed: {e}")
            return False
    
    def send_command(self, cmd: str):
        """Send a text command to ESP via BT.
        
        IMPORTANT: Text-only. No binary (0xFE) protocol.
        Image streams must use WiFi UDP exclusively.
        """
        if not self.connected or not self.ser:
            return False
        try:
            # Add newline terminator (ESP reads until \n)
            self.ser.write((cmd.strip() + "\n").encode())
            self.ser.flush()
            return True
        except serial.SerialException as e:
            print(f"[BT] Send error: {e}")
            self.connected = False
            return False
    
    def read_loop(self):
        """Background thread: Read ESP→PC responses and relay to UDP."""
        while self.running:
            try:
                if self.ser and self.ser.in_waiting:
                    line = self.ser.readline().decode("utf-8", errors="ignore").strip()
                    if line:
                        print(f"[ESP→PC] {line}")
                        # Relay to local UDP (companion_controller.py listens here)
                        self.udp_sock.sendto(
                            line.encode(),
                            ("127.0.0.1", UDP_PORT_RX)
                        )
                else:
                    time.sleep(0.05)
            except serial.SerialException:
                print("[BT] Connection lost, attempting reconnect...")
                self.connected = False
                time.sleep(2)
                self.connect()
            except Exception as e:
                print(f"[BT] Read error: {e}")
                time.sleep(0.5)
    
    def udp_relay_loop(self):
        """Background thread: Relay UDP commands to BT.
        
        Listens on UDP_PORT_TX for commands from companion_controller.py
        and forwards them to ESP via Bluetooth.
        
        FILTER: Only forwards text commands (rejects binary 0xFE packets).
        """
        relay_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        relay_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        relay_sock.bind(("127.0.0.1", UDP_PORT_TX))
        relay_sock.settimeout(1.0)
        
        print(f"[BT] UDP relay listening on 127.0.0.1:{UDP_PORT_TX}")
        
        while self.running:
            try:
                data, addr = relay_sock.recvfrom(2048)
                
                # CRITICAL: Reject binary packets (image stream uses 0xFE header)
                if len(data) > 0 and data[0] == 0xFE:
                    continue  # Skip — binary must go via WiFi
                
                msg = data.decode("utf-8", errors="ignore").strip()
                if msg:
                    self.send_command(msg)
                    
            except socket.timeout:
                pass
            except Exception as e:
                print(f"[BT] UDP relay error: {e}")
                time.sleep(1)
    
    def start(self):
        """Start the bridge (blocking)."""
        if not self.connect():
            return
        
        self.running = True
        
        # Start background threads
        read_thread = threading.Thread(target=self.read_loop, daemon=True)
        read_thread.start()
        
        udp_thread = threading.Thread(target=self.udp_relay_loop, daemon=True)
        udp_thread.start()
        
        print("\n[BT] Bridge active. Type commands or Ctrl+C to quit.")
        print("[BT] Commands: EMOTION:HAPPY, THOUGHT:text, PAGE:0, BTN:LEFT\n")
        
        # Interactive console
        try:
            while self.running:
                cmd = input("BT> ").strip()
                if cmd.lower() in ("quit", "exit", "q"):
                    break
                if cmd:
                    if self.send_command(cmd):
                        print(f"  → sent: {cmd}")
                    else:
                        print("  ✗ send failed")
        except KeyboardInterrupt:
            pass
        
        self.running = False
        if self.ser:
            self.ser.close()
        print("\n[BT] Bridge closed.")


# ═══════════════════════════════════════════════════════════
# MAIN
# ═══════════════════════════════════════════════════════════

if __name__ == "__main__":
    print("\n╔════════════════════════════════════════╗")
    print("║  CompanionOS Bluetooth Bridge v7.0    ║")
    print("╚════════════════════════════════════════╝\n")
    
    port = find_bt_port()
    if not port:
        sys.exit(1)
    
    bridge = BluetoothBridge(port)
    bridge.start()
