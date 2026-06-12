#!/usr/bin/env python3
"""
═══════════════════════════════════════════════════════════
  RUVIEW CSI PROCESSOR — CompanionOS Presence/Motion Module

  Receives ADR-018 binary CSI frames from a dedicated ESP32
  CSI node over UDP, runs a lightweight presence/motion
  classifier based on subcarrier amplitude variance, and
  exposes results for the PC bridge to push to the display.

  Architecture:
    [ESP32 CSI Node] --UDP:8890--> [This Module] --> send_udp("RUVIEW:{json}")

  ADR-018 Frame Layout (20-byte header + I/Q payload):
    [0..3]   Magic: 0xC5110001 (LE)
    [4]      Node ID
    [5]      Number of antennas
    [6..7]   Number of subcarriers (LE u16)
    [8..11]  Frequency MHz (LE u32)
    [12..15] Sequence number (LE u32)
    [16]     RSSI (i8)
    [17]     Noise floor (i8)
    [18..19] Reserved
    [20..]   I/Q data (signed int8 pairs: I0, Q0, I1, Q1, ...)

  Vitals Packet (32 bytes, magic 0xC5110002):
    [0..3]   Magic: 0xC5110002 (LE)
    [4]      Node ID
    [5]      Flags: bit0=presence, bit1=fall, bit2=motion
    [6..7]   Breathing rate (BPM * 100)
    [8..11]  Heart rate (BPM * 10000)
    [12]     RSSI (i8)
    [13]     n_persons
    [14..15] Reserved
    [16..19] Motion energy (float32)
    [20..23] Presence score (float32)
    [24..27] Timestamp ms (u32)
    [28..31] Reserved
═══════════════════════════════════════════════════════════
"""

import socket
import struct
import math
import time
import threading
import json
import os
from collections import deque

try:
    import onnxruntime as ort
    import numpy as np
    HAS_ML = True
except ImportError:
    HAS_ML = False

# ═══════════════════════════════════════════════════════════
# ADR-018 FRAME PARSER
# ═══════════════════════════════════════════════════════════

CSI_MAGIC = 0xC5110001
VITALS_MAGIC = 0xC5110002
CSI_HEADER_SIZE = 20


class ADR018Frame:
    """Parsed ADR-018 CSI frame."""
    __slots__ = ('node_id', 'n_antennas', 'n_subcarriers', 'freq_mhz',
                 'sequence', 'rssi', 'noise_floor', 'amplitudes', 'phases',
                 'timestamp')

    def __init__(self):
        self.node_id = 0
        self.n_antennas = 1
        self.n_subcarriers = 0
        self.freq_mhz = 0
        self.sequence = 0
        self.rssi = 0
        self.noise_floor = 0
        self.amplitudes = []  # list of float amplitudes per subcarrier
        self.phases = []      # list of float phases per subcarrier (radians)
        self.timestamp = 0.0


class VitalsFrame:
    """Parsed edge vitals packet (magic 0xC5110002)."""
    __slots__ = ('node_id', 'presence', 'motion', 'fall', 'breathing_bpm',
                 'heartrate_bpm', 'rssi', 'n_persons', 'motion_energy',
                 'presence_score', 'timestamp_ms')

    def __init__(self):
        self.node_id = 0
        self.presence = False
        self.motion = False
        self.fall = False
        self.breathing_bpm = 0.0
        self.heartrate_bpm = 0.0
        self.rssi = 0
        self.n_persons = 0
        self.motion_energy = 0.0
        self.presence_score = 0.0
        self.timestamp_ms = 0


def parse_adr018(data: bytes) -> ADR018Frame | None:
    """Parse an ADR-018 binary CSI frame."""
    if len(data) < CSI_HEADER_SIZE:
        return None

    magic = struct.unpack_from('<I', data, 0)[0]
    if magic != CSI_MAGIC:
        return None

    frame = ADR018Frame()
    frame.node_id = data[4]
    frame.n_antennas = data[5]
    frame.n_subcarriers = struct.unpack_from('<H', data, 6)[0]
    frame.freq_mhz = struct.unpack_from('<I', data, 8)[0]
    frame.sequence = struct.unpack_from('<I', data, 12)[0]
    frame.rssi = struct.unpack_from('b', data, 16)[0]
    frame.noise_floor = struct.unpack_from('b', data, 17)[0]
    frame.timestamp = time.time()

    # Parse I/Q pairs → amplitude + phase
    iq_data = data[CSI_HEADER_SIZE:]
    n_pairs = min(frame.n_subcarriers, len(iq_data) // 2)

    amplitudes = []
    phases = []
    for i in range(n_pairs):
        # I/Q are signed int8
        i_val = struct.unpack_from('b', iq_data, i * 2)[0]
        q_val = struct.unpack_from('b', iq_data, i * 2 + 1)[0]
        amp = math.sqrt(i_val * i_val + q_val * q_val)
        phase = math.atan2(q_val, i_val)
        amplitudes.append(amp)
        phases.append(phase)

    frame.amplitudes = amplitudes
    frame.phases = phases
    frame.n_subcarriers = n_pairs
    return frame


def parse_vitals(data: bytes) -> VitalsFrame | None:
    """Parse a 32-byte vitals packet (magic 0xC5110002)."""
    if len(data) < 32:
        return None

    magic = struct.unpack_from('<I', data, 0)[0]
    if magic != VITALS_MAGIC:
        return None

    v = VitalsFrame()
    v.node_id = data[4]
    flags = data[5]
    v.presence = bool(flags & 0x01)
    v.fall = bool(flags & 0x02)
    v.motion = bool(flags & 0x04)
    v.breathing_bpm = struct.unpack_from('<H', data, 6)[0] / 100.0
    v.heartrate_bpm = struct.unpack_from('<I', data, 8)[0] / 10000.0
    v.rssi = struct.unpack_from('b', data, 12)[0]
    v.n_persons = data[13]
    v.motion_energy = struct.unpack_from('<f', data, 16)[0]
    v.presence_score = struct.unpack_from('<f', data, 20)[0]
    v.timestamp_ms = struct.unpack_from('<I', data, 24)[0]
    return v


# ═══════════════════════════════════════════════════════════
# PRESENCE / MOTION CLASSIFIER
# ═══════════════════════════════════════════════════════════

class WelfordOnline:
    """Welford's online algorithm for running mean/variance."""
    __slots__ = ('count', 'mean', 'm2')

    def __init__(self):
        self.count = 0
        self.mean = 0.0
        self.m2 = 0.0

    def update(self, x: float):
        self.count += 1
        delta = x - self.mean
        self.mean += delta / self.count
        delta2 = x - self.mean
        self.m2 += delta * delta2

    @property
    def variance(self) -> float:
        if self.count < 2:
            return 0.0
        return self.m2 / (self.count - 1)

    def reset(self):
        self.count = 0
        self.mean = 0.0
        self.m2 = 0.0


class PresenceClassifier:
    """
    Variance-threshold presence/motion classifier.

    Uses rolling variance of subcarrier amplitude over a sliding window.
    Implements a 60-second ambient calibration window on startup
    (RuView's documented approach — calibrate in empty room).
    """

    WINDOW_SIZE = 100        # ~5 seconds at 20 Hz
    CALIB_FRAMES = 1200      # ~60 seconds at 20 Hz
    SIGMA_MULT = 3.0         # threshold = mean + 3*sigma of ambient
    MOTION_MULT = 6.0        # motion threshold = mean + 6*sigma
    TOP_K = 8                # Track top-K most variant subcarriers

    def __init__(self):
        self.calibrating = True
        self.calib_count = 0
        self.calib_stats = []  # WelfordOnline per subcarrier (for calibration)

        # Ambient baseline (set after calibration)
        self.ambient_mean = 0.0
        self.ambient_sigma = 0.0
        self.presence_threshold = 5.0   # default until calibrated
        self.motion_threshold = 10.0    # default until calibrated

        # Rolling window for runtime detection
        self.variance_window = deque(maxlen=self.WINDOW_SIZE)

        # Output state
        self.occupied = False
        self.motion = False
        self.confidence = 0.0
        self.current_variance = 0.0
        self.rssi = 0
        self.n_subcarriers = 0
        self.frames_processed = 0
        self.pps = 0.0  # packets per second

        # PPS tracking
        self._pps_count = 0
        self._pps_last = time.time()

        # ML Support (Original RuView)
        self.ml_model = None
        self.ml_enabled = False
        self._load_pretrained_model()

    def _load_pretrained_model(self):
        # Check standard model paths
        model_paths = [
            os.path.join(os.path.dirname(__file__), "models", "tiny_conv.onnx"),
            os.path.join(os.path.dirname(__file__), "models", "count_v1.onnx"),
            os.path.join(os.path.dirname(__file__), "models", "pose_v1.onnx")
        ]
        if HAS_ML:
            for p in model_paths:
                if os.path.exists(p):
                    try:
                        self.ml_model = ort.InferenceSession(p)
                        self.ml_enabled = True
                        print(f"🚀 RuView ML: Loaded pretrained ONNX model from {p}")
                        break
                    except Exception as e:
                        print(f"⚠️ RuView ML: Failed to load {p} - {e}")
        
        if not self.ml_enabled:
            print("⚠️ RuView ML: No pretrained ONNX model found. Using statistical variance fallback.")

    def reset_calibration(self):
        """Restart the 60-second ambient calibration."""
        self.calibrating = True
        self.calib_count = 0
        self.calib_stats = []
        self.variance_window.clear()
        self.occupied = False
        self.motion = False
        self.confidence = 0.0
        print("📡 RuView: Calibration RESET — leave room empty for 60 seconds")

    def process_frame(self, frame: ADR018Frame):
        """Process a single CSI frame and update presence/motion state."""
        if not frame.amplitudes:
            return

        self.frames_processed += 1
        self.rssi = frame.rssi
        self.n_subcarriers = frame.n_subcarriers

        # PPS calculation
        self._pps_count += 1
        now = time.time()
        elapsed = now - self._pps_last
        if elapsed >= 1.0:
            self.pps = self._pps_count / elapsed
            self._pps_count = 0
            self._pps_last = now

        # Compute per-frame variance across all subcarrier amplitudes
        n = len(frame.amplitudes)
        if n < 2:
            return

        mean_amp = sum(frame.amplitudes) / n
        var_amp = sum((a - mean_amp) ** 2 for a in frame.amplitudes) / (n - 1)

        if self.calibrating:
            # ── Calibration phase: collect ambient statistics ──
            if not self.calib_stats:
                self.calib_stats = [WelfordOnline() for _ in range(1)]

            self.calib_stats[0].update(var_amp)
            self.calib_count += 1

            if self.calib_count % 200 == 0:
                remaining = max(0, (self.CALIB_FRAMES - self.calib_count) / 20)
                print(f"📡 RuView: Calibrating... {remaining:.0f}s remaining "
                      f"(frame {self.calib_count}/{self.CALIB_FRAMES})")

            if self.calib_count >= self.CALIB_FRAMES:
                # Calibration complete — set thresholds
                self.ambient_mean = self.calib_stats[0].mean
                self.ambient_sigma = math.sqrt(self.calib_stats[0].variance) if self.calib_stats[0].variance > 0 else 1.0
                self.presence_threshold = self.ambient_mean + self.SIGMA_MULT * self.ambient_sigma
                self.motion_threshold = self.ambient_mean + self.MOTION_MULT * self.ambient_sigma

                # Ensure minimum thresholds
                self.presence_threshold = max(self.presence_threshold, 2.0)
                self.motion_threshold = max(self.motion_threshold, 5.0)

                self.calibrating = False
                print(f"✅ RuView: Calibration COMPLETE — "
                      f"ambient_mean={self.ambient_mean:.2f}, "
                      f"ambient_sigma={self.ambient_sigma:.2f}, "
                      f"presence_thresh={self.presence_threshold:.2f}, "
                      f"motion_thresh={self.motion_threshold:.2f}")
            return

        # ── Runtime detection ──
        self.variance_window.append(var_amp)
        self.current_variance = var_amp

        if self.ml_enabled and HAS_ML:
            try:
                # Basic feature vector prep (ensure correct input size)
                # Note: Exact shape depends on the ONNX model input
                input_name = self.ml_model.get_inputs()[0].name
                expected_shape = self.ml_model.get_inputs()[0].shape
                
                features = np.array(frame.amplitudes, dtype=np.float32).flatten()
                
                # Try to reshape into expected shape if it's dynamic or known
                try:
                    if len(expected_shape) >= 2:
                        features = features[:np.prod(expected_shape[1:])].reshape(1, *expected_shape[1:])
                    else:
                        features = features.reshape(1, -1)
                except:
                    features = features.reshape(1, -1)
                
                prediction = self.ml_model.run(None, {input_name: features})[0]
                
                # Assume prediction is a score or classification array
                # e.g. prediction[0][0] > 0.5 means occupied
                val = prediction.flatten()[0]
                self.occupied = bool(val > 0.5)
                self.motion = var_amp > self.motion_threshold
                self.confidence = float(min(100.0, max(0.0, val * 100.0)))
                return
            except Exception as e:
                self.ml_enabled = False # fallback to variance if predict fails

        # Rolling mean of variance window
        if len(self.variance_window) >= 10:
            rolling_mean = sum(self.variance_window) / len(self.variance_window)

            self.occupied = rolling_mean > self.presence_threshold
            self.motion = rolling_mean > self.motion_threshold

            # Confidence: 0-100 based on how far above threshold
            if self.presence_threshold > 0:
                ratio = rolling_mean / self.presence_threshold
                self.confidence = min(100.0, max(0.0, (ratio - 0.5) * 200))
            else:
                self.confidence = 0.0

    def process_vitals(self, vitals: VitalsFrame):
        """Process a vitals packet from the edge processor on the CSI node."""
        self.occupied = vitals.presence
        self.motion = vitals.motion
        self.rssi = vitals.rssi
        self.current_variance = vitals.motion_energy
        if vitals.presence_score > 0:
            self.confidence = min(100.0, vitals.presence_score * 10.0)
        self.calibrating = False  # edge processor handles its own calibration

    def get_state(self) -> dict:
        """Return current state as a dict for JSON serialization."""
        if self.calibrating:
            remaining = max(0, (self.CALIB_FRAMES - self.calib_count) / 20)
            status = f"Calibrating ({remaining:.0f}s)"
        elif self.motion:
            status = "Motion Detected"
        elif self.occupied:
            status = "Occupied"
        else:
            status = "Empty"

        return {
            'occupied': self.occupied,
            'motion': self.motion,
            'confidence': round(self.confidence, 1),
            'status': status,
            'variance': round(self.current_variance, 3),
            'rssi': self.rssi,
            'calibrating': self.calibrating,
            'subcarriers': self.n_subcarriers,
            'pps': round(self.pps, 1),
            'frames': self.frames_processed,
            'thresh_presence': round(self.presence_threshold, 2),
            'thresh_motion': round(self.motion_threshold, 2),
        }


# ═══════════════════════════════════════════════════════════
# ZONE MANAGER — Multi-node support (v1: single node)
# ═══════════════════════════════════════════════════════════

class ZoneManager:
    """
    Maps CSI node IDs to zone labels.
    v1: single node. The data model supports multiple nodes
    for future expansion without code changes.
    """

    def __init__(self):
        self.zones = {}  # node_id -> {'label': str, 'classifier': PresenceClassifier}
        self.default_label = "Room"
        self._lock = threading.Lock()

    def get_or_create_zone(self, node_id: int) -> tuple:
        """Get or create a zone for a given node_id."""
        with self._lock:
            if node_id not in self.zones:
                label = f"Zone {node_id}" if len(self.zones) > 0 else self.default_label
                self.zones[node_id] = {
                    'label': label,
                    'classifier': PresenceClassifier(),
                }
                print(f"📡 RuView: New CSI node {node_id} registered as '{label}'")
            return self.zones[node_id]['label'], self.zones[node_id]['classifier']

    def process_frame(self, frame: ADR018Frame):
        """Route a CSI frame to the correct zone classifier."""
        label, classifier = self.get_or_create_zone(frame.node_id)
        classifier.process_frame(frame)

    def process_vitals(self, vitals: VitalsFrame):
        """Route a vitals packet to the correct zone classifier."""
        label, classifier = self.get_or_create_zone(vitals.node_id)
        classifier.process_vitals(vitals)

    def get_all_states(self) -> list:
        """Return states for all zones."""
        with self._lock:
            result = []
            for node_id, zone in self.zones.items():
                state = zone['classifier'].get_state()
                state['node_id'] = node_id
                state['label'] = zone['label']
                result.append(state)
            return result

    def recalibrate_all(self):
        """Trigger recalibration for all zones."""
        with self._lock:
            for zone in self.zones.values():
                zone['classifier'].reset_calibration()

    def set_zone_label(self, node_id: int, label: str):
        """Rename a zone."""
        with self._lock:
            if node_id in self.zones:
                self.zones[node_id]['label'] = label


# ═══════════════════════════════════════════════════════════
# UDP LISTENER THREAD
# ═══════════════════════════════════════════════════════════

# Global zone manager instance
zone_manager = ZoneManager()

# Toggle: enable/disable CSI processing
ruview_enabled = True


def ruview_listener_loop(port: int = 8890):
    """
    Background thread: listens for ADR-018 CSI frames on UDP.
    Parses both CSI frames (0xC5110001) and vitals packets (0xC5110002).
    """
    global ruview_enabled

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(('0.0.0.0', port))
    sock.settimeout(2.0)  # Non-blocking with timeout for clean shutdown

    print(f"📡 RuView CSI listener started on UDP port {port}")

    while True:
        if not ruview_enabled:
            time.sleep(1)
            continue

        try:
            data, addr = sock.recvfrom(2048)
        except socket.timeout:
            continue
        except Exception as e:
            print(f"RuView socket error: {e}")
            time.sleep(1)
            continue

        if len(data) < 4:
            continue

        magic = struct.unpack_from('<I', data, 0)[0]

        if magic == CSI_MAGIC:
            frame = parse_adr018(data)
            if frame:
                zone_manager.process_frame(frame)
        elif magic == VITALS_MAGIC:
            vitals = parse_vitals(data)
            if vitals:
                zone_manager.process_vitals(vitals)
        # else: unknown packet type, silently ignore


def ruview_push_loop(send_udp_fn, interval: float = 2.0):
    """
    Background thread: periodically pushes zone states to the ESP32
    display unit via the existing UDP send function.
    """
    global ruview_enabled
    last_state_str = ''
    last_push_at = 0.0
    heartbeat_interval = 8.0

    time.sleep(2)  # Wait briefly for UDP discovery without leaving the page offline
    print("📡 RuView push loop started")

    while True:
        if not ruview_enabled:
            time.sleep(interval)
            continue

        try:
            states = zone_manager.get_all_states()
            if not states:
                # Push a default offline/waiting state
                state = {
                    'occupied': False,
                    'motion': False,
                    'confidence': 0.0,
                    'status': "Waiting for CSI Node",
                    'variance': 0.0,
                    'rssi': 0,
                    'calibrating': False,
                    'subcarriers': 0,
                    'pps': 0,
                    'frames': 0,
                    'thresh_presence': 0,
                    'thresh_motion': 0
                }
                states = [state]
            
            if states:
                # For v1 single-node, send just the first zone
                state = states[0]
                state_str = json.dumps(state)
                now = time.time()

                # Push on change and heartbeat the last state so the ESP recovers
                # after missed packets, page changes, or late boot.
                if state_str != last_state_str or (now - last_push_at) >= heartbeat_interval:
                    send_udp_fn(f"RUVIEW:{state_str}")
                    last_state_str = state_str
                    last_push_at = now

                    if state.get('occupied') or state.get('motion'):
                        status_emoji = "🔴" if state.get('motion') else "🟡"
                        print(f"{status_emoji} RuView: {state.get('status', '?')} "
                              f"(conf={state.get('confidence', 0):.0f}%, "
                              f"var={state.get('variance', 0):.3f}, "
                              f"pps={state.get('pps', 0):.0f})")
        except Exception as e:
            print(f"RuView push error: {e}")

        time.sleep(interval)


# ═══════════════════════════════════════════════════════════
# STANDALONE TEST
# ═══════════════════════════════════════════════════════════

if __name__ == '__main__':
    print("RuView CSI Processor — Standalone Test Mode")
    print("=" * 50)
    print(f"Listening on UDP port 8890 for ADR-018 frames...")
    print(f"Expected magic: 0x{CSI_MAGIC:08X}")
    print()

    # Run listener in main thread for testing
    ruview_listener_loop(port=8890)
