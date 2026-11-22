#!/usr/bin/env python3
"""
═══════════════════════════════════════════════════════════
  PC DASHBOARD TELEMETRY SERVICE (CompanionOS Module)
  Collects Live Windows PC Statistics:
  - CPU %, RAM %, Disk Read/Write MB/s, Disk Free %
  - Download/Upload MB/s, Total RX/TX GB, Network Latency ms
  - CPU Frequency GHz, Process Count
  - Top CPU & RAM Processes
  - GPU Usage %, VRAM Usage % & MB, GPU Temp °C, Laptop Temp °C
═══════════════════════════════════════════════════════════
"""

import time
import socket
import ctypes
import subprocess
import os

try:
    import psutil
except ImportError:
    psutil = None


class PCDashboardTelemetry:
    def __init__(self):
        self.last_net_recv = 0
        self.last_net_sent = 0
        self.last_disk_read = 0
        self.last_disk_write = 0
        self.last_counter_time = time.time()
        self.last_slow_update = 0.0

        # Slow cached values
        self.latency_ms = -1.0
        self.top_cpu_name = "N/A"
        self.top_cpu_pct = 0.0
        self.top_ram_name = "N/A"
        self.top_ram_mb = 0.0
        self.disk_free_pct = 0.0

        # GPU State
        self.nvml_available = False
        self.nvml_handle = None
        self._init_nvml()

        # Prime psutil counters if psutil is available
        if psutil:
            try:
                psutil.cpu_percent(interval=None)
                net = psutil.net_io_counters()
                if net:
                    self.last_net_recv = net.bytes_recv
                    self.last_net_sent = net.bytes_sent
                disk = psutil.disk_io_counters()
                if disk:
                    self.last_disk_read = disk.read_bytes
                    self.last_disk_write = disk.write_bytes
            except Exception:
                pass

    def _init_nvml(self):
        """Initialize NVML via ctypes for ultra-fast, zero-overhead NVIDIA GPU polling."""
        try:
            self.nvml = ctypes.CDLL("nvml.dll")
            if self.nvml.nvmlInit_v2() == 0:
                count = ctypes.c_uint()
                self.nvml.nvmlDeviceGetCount_v2(ctypes.byref(count))
                if count.value > 0:
                    self.nvml_handle = ctypes.c_void_p()
                    if self.nvml.nvmlDeviceGetHandleByIndex_v2(0, ctypes.byref(self.nvml_handle)) == 0:
                        self.nvml_available = True
        except Exception:
            self.nvml_available = False

    def get_gpu_metrics(self):
        """Returns (gpu_pct, vram_pct, vram_used_mb, vram_total_mb, gpu_temp_c)."""
        if self.nvml_available and self.nvml_handle:
            try:
                class nvmlUtilization_t(ctypes.Structure):
                    _fields_ = [("gpu", ctypes.c_uint), ("memory", ctypes.c_uint)]

                class nvmlMemory_t(ctypes.Structure):
                    _fields_ = [("total", ctypes.c_ulonglong), ("free", ctypes.c_ulonglong), ("used", ctypes.c_ulonglong)]

                util = nvmlUtilization_t()
                self.nvml.nvmlDeviceGetUtilizationRates(self.nvml_handle, ctypes.byref(util))
                gpu_pct = float(util.gpu)

                mem = nvmlMemory_t()
                self.nvml.nvmlDeviceGetMemoryInfo(self.nvml_handle, ctypes.byref(mem))
                vram_total_mb = float(mem.total) / (1024.0 * 1024.0)
                vram_used_mb = float(mem.used) / (1024.0 * 1024.0)
                vram_pct = (vram_used_mb / vram_total_mb * 100.0) if vram_total_mb > 0 else 0.0

                temp = ctypes.c_uint()
                self.nvml.nvmlDeviceGetTemperature(self.nvml_handle, 0, ctypes.byref(temp))
                gpu_temp = float(temp.value)

                return gpu_pct, vram_pct, vram_used_mb, vram_total_mb, gpu_temp
            except Exception:
                pass

        # Fallback to nvidia-smi if NVML direct call failed
        try:
            res = subprocess.run(
                ['nvidia-smi', '--query-gpu=utilization.gpu,utilization.memory,memory.used,memory.total,temperature.gpu', '--format=csv,noheader,nounits'],
                capture_output=True, text=True, timeout=1.0
            )
            if res.returncode == 0:
                parts = [p.strip() for p in res.stdout.strip().split(',')]
                if len(parts) >= 5:
                    gpu_pct = float(parts[0])
                    vram_pct = float(parts[1])
                    vram_used_mb = float(parts[2])
                    vram_total_mb = float(parts[3])
                    gpu_temp = float(parts[4])
                    return gpu_pct, vram_pct, vram_used_mb, vram_total_mb, gpu_temp
        except Exception:
            pass

        return 0.0, 0.0, 0.0, 0.0, 0.0

    def sanitize_name(self, name):
        if not name:
            return "N/A"
        name = str(name).replace(",", " ").replace("\n", " ").replace("\r", " ").strip()
        return name[:24]

    def get_tcp_latency_ms(self, host="1.1.1.1", port=443, timeout=0.8):
        start = time.perf_counter()
        try:
            with socket.create_connection((host, port), timeout=timeout):
                return (time.perf_counter() - start) * 1000.0
        except OSError:
            return -1.0

    def get_top_processes(self):
        if not psutil:
            return ("N/A", 0.0, "N/A", 0.0)
        top_cpu_name = "N/A"
        top_cpu_pct = 0.0
        top_ram_name = "N/A"
        top_ram_mb = 0.0
        cpu_count = psutil.cpu_count(logical=True) or 1

        for proc in psutil.process_iter(["pid", "name", "memory_info"]):
            try:
                raw_cpu = proc.cpu_percent(interval=None)
                cpu = raw_cpu / cpu_count
                mem_info = proc.info.get("memory_info")
                ram_mb = (mem_info.rss / 1024 / 1024) if mem_info else 0.0
                name = self.sanitize_name(proc.info.get("name") or f"PID {proc.info.get('pid')}")

                if cpu > top_cpu_pct:
                    top_cpu_pct = cpu
                    top_cpu_name = name
                if ram_mb > top_ram_mb:
                    top_ram_mb = ram_mb
                    top_ram_name = name
            except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess):
                continue

        return (top_cpu_name, top_cpu_pct, top_ram_name, top_ram_mb)

    def collect(self):
        """Collect and return full dictionary of metrics."""
        now = time.time()
        dt = now - self.last_counter_time
        if dt <= 0:
            dt = 0.5

        if not psutil:
            return {}

        cpu = psutil.cpu_percent(interval=None)
        ram = psutil.virtual_memory().percent

        # Network rates
        net = psutil.net_io_counters()
        download_bytes = max(0, net.bytes_recv - self.last_net_recv)
        upload_bytes = max(0, net.bytes_sent - self.last_net_sent)
        download_mb = (download_bytes / dt / 1024.0 / 1024.0)
        upload_mb = (upload_bytes / dt / 1024.0 / 1024.0)
        total_rx_gb = (net.bytes_recv / 1024.0 / 1024.0 / 1024.0)
        total_tx_gb = (net.bytes_sent / 1024.0 / 1024.0 / 1024.0)
        self.last_net_recv = net.bytes_recv
        self.last_net_sent = net.bytes_sent

        # Uptime
        uptime_seconds = int(now - psutil.boot_time())

        # CPU Frequency
        freq = psutil.cpu_freq()
        cpu_freq = freq.current if freq else 0.0

        # Disk IO rates
        disk = psutil.disk_io_counters()
        if disk:
            read_delta = max(0, disk.read_bytes - self.last_disk_read)
            write_delta = max(0, disk.write_bytes - self.last_disk_write)
            disk_read_mb = (read_delta / dt / 1024.0 / 1024.0)
            disk_write_mb = (write_delta / dt / 1024.0 / 1024.0)
            self.last_disk_read = disk.read_bytes
            self.last_disk_write = disk.write_bytes
        else:
            disk_read_mb = 0.0
            disk_write_mb = 0.0

        process_count = len(psutil.pids())

        # Slower updates (every 2.0s)
        if now - self.last_slow_update >= 2.0 or self.last_slow_update == 0:
            try:
                disk_usage = psutil.disk_usage("C:\\")
            except Exception:
                disk_usage = psutil.disk_usage("/")
            self.disk_free_pct = 100.0 - disk_usage.percent

            self.latency_ms = self.get_tcp_latency_ms()
            (
                self.top_cpu_name,
                self.top_cpu_pct,
                self.top_ram_name,
                self.top_ram_mb
            ) = self.get_top_processes()
            self.last_slow_update = now

        # GPU and Temperatures
        gpu_pct, vram_pct, vram_used_mb, vram_total_mb, gpu_temp = self.get_gpu_metrics()
        laptop_temp = gpu_temp  # Primary laptop sensor

        self.last_counter_time = now

        return {
            "cpu": cpu,
            "ram": ram,
            "down": download_mb,
            "up": upload_mb,
            "uptime": uptime_seconds,
            "freq": cpu_freq,
            "dread": disk_read_mb,
            "dwrite": disk_write_mb,
            "proc": process_count,
            "rx": total_rx_gb,
            "tx": total_tx_gb,
            "latency": self.latency_ms,
            "diskfree": self.disk_free_pct,
            "top_cpu_pct": self.top_cpu_pct,
            "top_cpu_name": self.sanitize_name(self.top_cpu_name),
            "top_ram_mb": self.top_ram_mb,
            "top_ram_name": self.sanitize_name(self.top_ram_name),
            "gpu": gpu_pct,
            "vram": vram_pct,
            "vram_used": vram_used_mb,
            "vram_total": vram_total_mb,
            "gpu_temp": gpu_temp,
            "laptop_temp": laptop_temp
        }

    def format_packet(self, data):
        """Format 23-parameter CSV packet with PCDASH: prefix."""
        if not data:
            return ""
        return (
            f"PCDASH:{data['cpu']:.1f},"
            f"{data['ram']:.1f},"
            f"{data['down']:.3f},"
            f"{data['up']:.3f},"
            f"{data['uptime']},"
            f"{data['freq']:.1f},"
            f"{data['dread']:.3f},"
            f"{data['dwrite']:.3f},"
            f"{data['proc']},"
            f"{data['rx']:.3f},"
            f"{data['tx']:.3f},"
            f"{data['latency']:.1f},"
            f"{data['diskfree']:.1f},"
            f"{data['top_cpu_pct']:.1f},"
            f"{data['top_cpu_name']},"
            f"{data['top_ram_mb']:.1f},"
            f"{data['top_ram_name']},"
            f"{data['gpu']:.1f},"
            f"{data['vram']:.1f},"
            f"{data['vram_used']:.1f},"
            f"{data['vram_total']:.1f},"
            f"{data['gpu_temp']:.1f},"
            f"{data['laptop_temp']:.1f}"
        )
