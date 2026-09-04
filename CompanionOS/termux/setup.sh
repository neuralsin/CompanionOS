#!/data/data/com.termux/files/usr/bin/bash
# ═══════════════════════════════════════════════════════════
# CompanionOS — Termux Environment Setup Script
# ═══════════════════════════════════════════════════════════

echo "================================================="
echo "   Setting up CompanionOS Termux Environment     "
echo "================================================="

# 1. Update package manager
echo "[1/4] Updating Termux packages..."
pkg update -y && pkg upgrade -y

# 2. Install pre-compiled native binaries (avoids C++ build errors)
echo "[2/4] Installing C/C++ libraries and pre-compiled Python modules..."
pkg install -y python python-pillow python-lxml libjpeg-turbo libxml2 libxslt tmux git clang

# 3. Install Python dependencies
echo "[3/4] Installing Python requirements..."
pip install --upgrade pip
pip install spotipy requests python-dotenv flask yfinance websockets

# 4. Acquire Wake Lock so Termux doesn't sleep
echo "[4/4] Enabling background wake lock..."
termux-wake-lock

echo "================================================="
echo "  SUCCESS! Environment Ready.                    "
echo "  Run: ./start.sh or python companion_controller.py"
echo "================================================="
