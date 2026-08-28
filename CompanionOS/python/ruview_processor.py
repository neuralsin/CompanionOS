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
    Presence / motion classifier that drives the RuView UI.

    Two operating modes — chosen automatically:

    1. ML mode (preferred):
       The loaded tiny_conv.onnx is a pre-trained CNN.  It does NOT need
       any ambient calibration — its decision boundary was learned during
       training on real CSI data.  The classifier skips the calibration
       phase entirely and starts producing predictions from the 3rd frame.
       Each 8×8 CSI frame is z-score normalised before inference so raw
       ESP32 amplitude scale doesn't matter.  Class probabilities are
       smoothed with an EMA(α=0.25) to suppress per-frame noise.

    2. Variance fallback:
       Used when onnxruntime or the model file is not available.  Runs a
       60-second ambient calibration on first boot, then persists the
       learned thresholds to ruview_calib.json so subsequent restarts
       skip the wait.
    """

    WINDOW_SIZE   = 100   # rolling window depth (~5 s at 20 Hz)
    CALIB_FRAMES  = 1200  # frames to collect for variance calibration (~60 s)
    SIGMA_MULT    = 3.0   # presence threshold = ambient_mean + 3σ
    MOTION_MULT   = 6.0   # motion threshold   = ambient_mean + 6σ
    EMA_ALPHA     = 0.25  # EMA smoothing for ML class probabilities
    CALIB_CACHE   = os.path.join(os.path.dirname(__file__), "ruview_calib.json")

    def __init__(self):
        # ── Variance-fallback calibration state ────────────────────────────
        self.calib_count  = 0
        self.calib_stats  = []           # WelfordOnline accumulators

        # Ambient baseline (set after calibration or loaded from cache)
        self.ambient_mean       = 0.0
        self.ambient_sigma      = 0.0
        self.presence_threshold = 5.0    # safe default until calibrated
        self.motion_threshold   = 10.0

        # Rolling window for variance-fallback runtime detection
        self.variance_window = deque(maxlen=self.WINDOW_SIZE)
        # Temporal buffer for ONNX model input [1, 3, 8, 8]
        self.csi_frame_buffer = deque(maxlen=3)

        # ── Output state ───────────────────────────────────────────────────
        self.occupied         = False
        self.motion           = False
        self.confidence       = 0.0
        self.current_variance = 0.0
        self.rssi             = 0
        self.n_subcarriers    = 0
        self.frames_processed = 0
        self.pps              = 0.0

        # EMA-smoothed class probabilities [Empty, Occupied, Motion, Fall]
        self._ema_probs = [0.5, 0.0, 0.0, 0.0]

        # PPS tracking
        self._pps_count = 0
        self._pps_last  = time.time()
        self.last_frame_time = 0.0

        # ── ML support ─────────────────────────────────────────────────────
        self.ml_model   = None
        self.ml_enabled = False
        self._load_pretrained_model()

        # ── Calibration gating ─────────────────────────────────────────────
        # If a pre-trained ML model loaded successfully, skip calibration
        # completely — the model's weights already encode the decision boundary.
        # Only use calibration for the statistical variance fallback.
        if self.ml_enabled:
            self.calibrating = False
            print("✅ RuView: ML model active — calibration skipped (pretrained weights used)")
        else:
            # Try loading persisted thresholds from a previous calibration run
            self.calibrating = not self._load_cached_calibration()
            if not self.calibrating:
                print("✅ RuView: Loaded cached calibration — no recalibration needed")
            else:
                print("📡 RuView: No cache — starting 60-second ambient calibration")

    # ──────────────────────────────────────────────────────────────────────
    # Model loading
    # ──────────────────────────────────────────────────────────────────────

    def _load_pretrained_model(self):
        """Try each model in priority order; stop at first successful load."""
        model_priority = [
            "tiny_conv.onnx",   # smallest, self-contained, highest priority
            "count_v1.onnx",    # needs .data sidecar
            "pose_v1.onnx",     # needs .data sidecar
        ]
        if not HAS_ML:
            print("⚠️ RuView ML: onnxruntime not installed. Using variance fallback.")
            return

        models_dir = os.path.join(os.path.dirname(__file__), "models")
        for fname in model_priority:
            path = os.path.join(models_dir, fname)
            if not os.path.exists(path):
                continue
            try:
                self.ml_model   = ort.InferenceSession(path)
                self.ml_enabled = True
                print(f"🚀 RuView ML: Loaded {fname} — instant detection, no calibration needed")
                return
            except Exception as e:
                print(f"⚠️ RuView ML: Could not load {fname}: {e}")

        print("⚠️ RuView ML: No model loaded. Using variance fallback.")

    # ──────────────────────────────────────────────────────────────────────
    # Calibration persistence (variance fallback only)
    # ──────────────────────────────────────────────────────────────────────

    def _load_cached_calibration(self) -> bool:
        """
        Load previously computed variance thresholds from disk.
        Returns True if a valid cache was found and applied.
        """
        try:
            if not os.path.exists(self.CALIB_CACHE):
                return False
            with open(self.CALIB_CACHE, 'r') as f:
                d = json.load(f)
            # Validate required keys
            if not all(k in d for k in ('ambient_mean', 'ambient_sigma',
                                         'presence_threshold', 'motion_threshold')):
                return False
            self.ambient_mean       = float(d['ambient_mean'])
            self.ambient_sigma      = float(d['ambient_sigma'])
            self.presence_threshold = float(d['presence_threshold'])
            self.motion_threshold   = float(d['motion_threshold'])
            return True
        except Exception:
            return False

    def _save_cached_calibration(self):
        """Persist current variance thresholds to disk for future restarts."""
        try:
            d = {
                'ambient_mean':       self.ambient_mean,
                'ambient_sigma':      self.ambient_sigma,
                'presence_threshold': self.presence_threshold,
                'motion_threshold':   self.motion_threshold,
            }
            with open(self.CALIB_CACHE, 'w') as f:
                json.dump(d, f, indent=2)
            print(f"💾 RuView: Calibration cached to {self.CALIB_CACHE}")
        except Exception as e:
            print(f"⚠️ RuView: Could not save calibration cache: {e}")

    def reset_calibration(self):
        """
        Manually restart ambient calibration.
        Has no effect when ML model is active — the model is always
        'calibrated' via its training weights.
        """
        if self.ml_enabled:
            print("ℹ️  RuView: Recalibration not needed — pre-trained ML model is active.")
            return
        self.calibrating  = True
        self.calib_count  = 0
        self.calib_stats  = []
        self.variance_window.clear()
        self.csi_frame_buffer.clear()
        self._ema_probs   = [0.5, 0.0, 0.0, 0.0]
        self.occupied     = False
        self.motion       = False
        self.confidence   = 0.0
        print("📡 RuView: Calibration RESET — leave room empty for 60 seconds")

    def process_frame(self, frame: ADR018Frame):
        """Process a single CSI frame and update presence / motion state."""
        if not frame.amplitudes:
            return

        self.frames_processed += 1
        self.rssi          = frame.rssi
        self.n_subcarriers = frame.n_subcarriers

        # ── PPS tracking ───────────────────────────────────────────────────
        self._pps_count += 1
        now = time.time()
        self.last_frame_time = now
        elapsed = now - self._pps_last
        if elapsed >= 1.0:
            self.pps        = self._pps_count / elapsed
            self._pps_count = 0
            self._pps_last  = now

        # ── Per-frame amplitude variance (used by variance fallback) ───────
        n = len(frame.amplitudes)
        if n < 2:
            return
        mean_amp = sum(frame.amplitudes) / n
        var_amp  = sum((a - mean_amp) ** 2 for a in frame.amplitudes) / (n - 1)

        # ── Variance-fallback calibration phase ────────────────────────────
        # Only runs when no ML model is loaded AND no cached thresholds exist.
        if self.calibrating:
            if not self.calib_stats:
                self.calib_stats = [WelfordOnline()]
            self.calib_stats[0].update(var_amp)
            self.calib_count += 1

            if self.calib_count % 200 == 0:
                remaining = max(0, (self.CALIB_FRAMES - self.calib_count) / 20)
                print(f"📡 RuView: Calibrating... {remaining:.0f}s remaining "
                      f"(frame {self.calib_count}/{self.CALIB_FRAMES})")

            if self.calib_count >= self.CALIB_FRAMES:
                self.ambient_mean   = self.calib_stats[0].mean
                self.ambient_sigma  = (math.sqrt(self.calib_stats[0].variance)
                                       if self.calib_stats[0].variance > 0 else 1.0)
                self.presence_threshold = max(
                    self.ambient_mean + self.SIGMA_MULT  * self.ambient_sigma, 2.0)
                self.motion_threshold   = max(
                    self.ambient_mean + self.MOTION_MULT * self.ambient_sigma, 5.0)
                self.calibrating = False
                # Persist thresholds so future restarts skip this step
                self._save_cached_calibration()
                print(f"✅ RuView: Calibration COMPLETE — "
                      f"ambient_mean={self.ambient_mean:.2f}, "
                      f"sigma={self.ambient_sigma:.2f}, "
                      f"pres_thresh={self.presence_threshold:.2f}, "
                      f"mot_thresh={self.motion_threshold:.2f}")
            return  # don't run detection during calibration

        # ── Runtime detection ──────────────────────────────────────────────
        self.variance_window.append(var_amp)
        self.current_variance = var_amp

        # Build 8×8 matrix from up to 64 subcarrier amplitudes
        amps = list(frame.amplitudes[:64])
        if len(amps) < 64:
            amps += [0.0] * (64 - len(amps))

        # Z-score normalise within this frame so raw ESP32 ADC scale is
        # irrelevant — matches the normalisation applied to training data.
        amp_arr   = np.array(amps, dtype=np.float32)
        amp_mean  = amp_arr.mean()
        amp_std   = amp_arr.std() + 1e-6   # prevent div-by-zero on silent channel
        amp_norm  = (amp_arr - amp_mean) / amp_std
        frame_8x8 = amp_norm.reshape(8, 8)
        self.csi_frame_buffer.append(frame_8x8)

        # ── ML path (pre-trained ONNX — no calibration required) ──────────
        if self.ml_enabled and HAS_ML and len(self.csi_frame_buffer) == 3 and self.ml_model is not None:
            try:
                # Stack last 3 frames → [1, 3, 8, 8]
                features   = np.stack(self.csi_frame_buffer, axis=0)
                features   = np.expand_dims(features, axis=0)

                input_name = self.ml_model.get_inputs()[0].name
                raw_out    = self.ml_model.run(None, {input_name: features})[0]

                # Output [1, 4, 8, 8] → spatial mean → [4] class scores
                # Classes: 0=Empty, 1=Occupied, 2=Motion, 3=Fall/Anomaly
                spatial_avg = np.mean(raw_out, axis=(2, 3))[0]

                # Softmax so probabilities sum to 1 and are comparable
                e_x  = np.exp(spatial_avg - spatial_avg.max())
                probs = (e_x / e_x.sum()).tolist()   # [p_empty, p_occ, p_mot, p_fall]

                # EMA smoothing (α=0.25) to reduce per-frame flicker
                a = self.EMA_ALPHA
                self._ema_probs = [
                    a * probs[i] + (1 - a) * self._ema_probs[i]
                    for i in range(4)
                ]
                p_empty, p_occ, p_mot, _ = self._ema_probs

                self.occupied   = (p_occ > p_empty or p_mot > p_empty)
                self.motion     = (p_mot > p_occ   and p_mot > p_empty)
                max_conf        = max(p_occ, p_mot)
                self.confidence = min(100.0, max(0.0, max_conf * 100.0))
                return
            except Exception as e:
                print(f"⚠️ RuView ML: Prediction error: {e}. Switching to variance fallback.")
                self.ml_enabled = False  # permanent fallback for this session

        # ── Variance-threshold fallback ────────────────────────────────────
        if len(self.variance_window) >= 10:
            rolling_mean    = sum(self.variance_window) / len(self.variance_window)
            self.occupied   = rolling_mean > self.presence_threshold
            self.motion     = rolling_mean > self.motion_threshold
            if self.presence_threshold > 0:
                ratio           = rolling_mean / self.presence_threshold
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
        now = time.time()
        if now - self.last_frame_time > 3.0:
            self.pps = 0.0

        if self.pps == 0.0:
            if self.last_frame_time == 0.0:
                status = "Waiting for CSI..."
            else:
                status = f"Offline ({int(now - self.last_frame_time)}s ago)"
        elif self.calibrating:
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
ruview_enabled = False

# Global stats for diagnostics
ruview_stats = {
    'packets_rx': 0,
    'invalid_rx': 0,
    'last_rx_time': 0.0
}


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
            ruview_stats['packets_rx'] += 1
            ruview_stats['last_rx_time'] = time.time()
        except socket.timeout:
            continue
        except Exception as e:
            print(f"RuView socket error: {e}")
            time.sleep(1)
            continue

        if len(data) < 4:
            ruview_stats['invalid_rx'] += 1
            continue

        magic = struct.unpack_from('<I', data, 0)[0]

        if magic == CSI_MAGIC:
            frame = parse_adr018(data)
            if frame:
                zone_manager.process_frame(frame)
            else:
                ruview_stats['invalid_rx'] += 1
        elif magic == VITALS_MAGIC:
            vitals = parse_vitals(data)
            if vitals:
                zone_manager.process_vitals(vitals)
            else:
                ruview_stats['invalid_rx'] += 1
        else:
            ruview_stats['invalid_rx'] += 1


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
                time_since_rx = time.time() - ruview_stats['last_rx_time']
                msg = "Waiting for CSI..."
                if ruview_stats['packets_rx'] == 0:
                    msg = "No CSI data (ESP32 → PC:8890)"
                elif time_since_rx > 5.0:
                    msg = f"CSI Offline ({int(time_since_rx)}s ago)"
                elif ruview_stats['invalid_rx'] > 0:
                    msg = f"Bad packets: {ruview_stats['invalid_rx']}"

                state = {
                    'occupied': False,
                    'motion': False,
                    'confidence': 0.0,
                    'status': msg,
                    'variance': 0.0,
                    'rssi': 0,
                    'calibrating': False,
                    'subcarriers': 0,
                    'pps': 0,
                    'frames': ruview_stats['packets_rx'],
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
