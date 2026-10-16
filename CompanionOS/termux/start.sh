#!/data/data/com.termux/files/usr/bin/bash
# ═══════════════════════════════════════════════════════════
# CompanionOS — Termux Start Script
# ═══════════════════════════════════════════════════════════

# Change to script directory
cd "$(dirname "$0")"

# Keep CPU running in background
termux-wake-lock

echo "Starting CompanionOS Python Engine..."
python companion_controller.py
