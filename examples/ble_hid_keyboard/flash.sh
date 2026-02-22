#!/usr/bin/env bash
# flash.sh — build, flash, and monitor the BLE HID keyboard example.
# Run from the examples/ble_hid_keyboard/ directory.
set -euo pipefail

TARGET="esp32c6"

info() { printf '\033[1;34m[flash]\033[0m %s\n' "$*"; }
die()  { printf '\033[1;31m[error]\033[0m %s\n' "$*" >&2; exit 1; }

# ---------- Sanity checks ----------

command -v idf.py > /dev/null 2>&1 || \
    die "idf.py not found. Run: . ~/esp/esp-idf/export.sh"

[[ -f CMakeLists.txt ]] || \
    die "Run this script from the examples/ble_hid_keyboard/ directory."

# ---------- Detect serial port ----------

detect_port() {
    # Prefer USB-JTAG (ACM) over FTDI/CP210x (USB)
    for pattern in /dev/ttyACM0 /dev/ttyACM1 /dev/ttyUSB0 /dev/ttyUSB1; do
        [[ -e "$pattern" ]] && { echo "$pattern"; return; }
    done
}

PORT="${PORT:-$(detect_port)}"

if [[ -z "$PORT" ]]; then
    die "No serial port found. Connect the board and retry, or set PORT=/dev/ttyXXX."
fi

if [[ ! -w "$PORT" ]]; then
    die "No write access to $PORT. Add yourself to dialout: sudo usermod -aG dialout \$USER"
fi

info "Using port: $PORT"

# ---------- Set target (idempotent) ----------

# Read current target from sdkconfig if it exists
CURRENT_TARGET=""
if [[ -f sdkconfig ]]; then
    CURRENT_TARGET=$(grep -oP '(?<=CONFIG_IDF_TARGET=").*(?=")' sdkconfig 2>/dev/null || true)
fi

if [[ "$CURRENT_TARGET" != "$TARGET" ]]; then
    info "Setting target to $TARGET..."
    idf.py set-target "$TARGET"
fi

# ---------- Build ----------

info "Building..."
idf.py build

# ---------- Flash + monitor ----------

info "Flashing $PORT and opening monitor (Ctrl+] to exit)..."
idf.py -p "$PORT" flash monitor
