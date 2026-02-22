#!/usr/bin/env bash
# setup.sh — install ESP-IDF and system dependencies for ESP32-C6 development
# Tested on Ubuntu 22.04 and 24.04.
set -euo pipefail

IDF_DIR="${IDF_DIR:-$HOME/esp/esp-idf}"
IDF_VERSION="${IDF_VERSION:-v5.4}"

info()  { printf '\033[1;34m[setup]\033[0m %s\n' "$*"; }
ok()    { printf '\033[1;32m[ok]\033[0m %s\n' "$*"; }
warn()  { printf '\033[1;33m[warn]\033[0m %s\n' "$*"; }
die()   { printf '\033[1;31m[error]\033[0m %s\n' "$*" >&2; exit 1; }

# ---------- 1. System packages ----------

info "Installing system packages..."
sudo apt-get update -qq
sudo apt-get install -y \
    git wget flex bison gperf \
    python3 python3-pip python3-venv \
    cmake ninja-build ccache \
    libffi-dev libssl-dev libusb-1.0-0 dfu-util
ok "System packages installed."

# ---------- 2. Serial port access ----------

if groups "$USER" | grep -qw dialout; then
    ok "User '$USER' is already in the dialout group."
else
    info "Adding '$USER' to the dialout group..."
    sudo usermod -aG dialout "$USER"
    warn "Log out and back in (or reboot) for the group change to take effect."
fi

# ---------- 3. Clone ESP-IDF ----------

if [[ -d "$IDF_DIR/.git" ]]; then
    ok "ESP-IDF already cloned at $IDF_DIR."
    info "Updating to $IDF_VERSION..."
    git -C "$IDF_DIR" fetch --tags --quiet
    git -C "$IDF_DIR" checkout "$IDF_VERSION" --quiet
    git -C "$IDF_DIR" submodule update --init --recursive --quiet
else
    info "Cloning ESP-IDF $IDF_VERSION into $IDF_DIR..."
    mkdir -p "$(dirname "$IDF_DIR")"
    git clone --branch "$IDF_VERSION" --depth 1 --recurse-submodules \
        https://github.com/espressif/esp-idf.git "$IDF_DIR"
fi
ok "ESP-IDF $IDF_VERSION ready."

# ---------- 4. Install toolchains ----------

info "Installing ESP32-C6 toolchain (this may take a few minutes)..."
"$IDF_DIR/install.sh" esp32c6
ok "Toolchain installed."

# ---------- 5. Verify ----------

# Source the environment just long enough to check idf.py
set +u
# shellcheck disable=SC1090
source "$IDF_DIR/export.sh" > /dev/null 2>&1
set -u

if command -v idf.py > /dev/null 2>&1; then
    ok "idf.py $(idf.py --version 2>&1 | head -1) is available."
else
    die "idf.py not found after sourcing export.sh — check the install log above."
fi

# ---------- Done ----------

cat <<EOF

$(printf '\033[1;32m')Setup complete.$(printf '\033[0m')

Activate the ESP-IDF environment in each new shell:

    . $IDF_DIR/export.sh

To make it permanent:

    echo '. $IDF_DIR/export.sh' >> ~/.bashrc

Then build and flash the keyboard example:

    cd examples/ble_hid_keyboard
    ./flash.sh
EOF
