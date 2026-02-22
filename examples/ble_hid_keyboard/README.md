# BLE HID Keyboard Example

Demonstrates the ESP32-C6 acting as a wireless Bluetooth keyboard using the
HID-over-GATT Profile (HOGP) with the NimBLE stack and ESP-IDF 5.x.

When connected and paired, the device types `hello world` into the active text
field every 30 seconds.

See [`../../docs/ble-hid-keyboard.md`](../../docs/ble-hid-keyboard.md) for a
full explanation of the HOGP profile, report descriptor, and pairing flow.

---

## Hardware

Any ESP32-C6 development board. The examples below use a board with a built-in
USB-JTAG/serial bridge (e.g. ESP32-C6-DevKitC-1), which appears as
`/dev/ttyACM0` on Linux. Boards with a separate FTDI/CP210x chip use
`/dev/ttyUSB0` instead.

---

## Development environment setup (Ubuntu 22.04 / 24.04)

### 1. System packages

```bash
sudo apt update
sudo apt install -y \
    git wget flex bison gperf \
    python3 python3-pip python3-venv \
    cmake ninja-build ccache \
    libffi-dev libssl-dev libusb-1.0-0 dfu-util
```

### 2. Serial port access

Add your user to the `dialout` group so you can access the USB serial port
without `sudo`. **Log out and back in (or reboot) for this to take effect.**

```bash
sudo usermod -aG dialout $USER
```

### 3. Install ESP-IDF

ESP-IDF v5.2 or later is required for full ESP32-C6 support.

```bash
mkdir -p ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf
cd ~/esp/esp-idf

# Install toolchains for ESP32-C6 only (faster than installing all targets)
./install.sh esp32c6
```

The install script downloads the RISC-V GCC toolchain, OpenOCD, and other
tools into `~/.espressif/`.

### 4. Activate the environment

Source the export script in every new shell session before using `idf.py`.
Add it to `~/.bashrc` to make it permanent.

```bash
. ~/esp/esp-idf/export.sh
```

To make it permanent:

```bash
echo '. ~/esp/esp-idf/export.sh' >> ~/.bashrc
```

Verify:

```bash
idf.py --version
# should print: ESP-IDF v5.x.x
```

---

## Build

From this directory:

```bash
# Set the target chip (required once per build directory)
idf.py set-target esp32c6

# Build
idf.py build
```

`sdkconfig.defaults` is picked up automatically and configures NimBLE, NVS
bonding persistence, and the GAP device name. To change settings interactively:

```bash
idf.py menuconfig
# navigate to: Component config → Bluetooth
```

---

## Flash and monitor

Connect the ESP32-C6 board via USB, then:

```bash
# Flash and open serial monitor in one step
idf.py -p /dev/ttyACM0 flash monitor
```

Replace `/dev/ttyACM0` with `/dev/ttyUSB0` if your board uses a FTDI/CP210x
bridge. To find the port:

```bash
ls /dev/tty{ACM,USB}* 2>/dev/null
# or:
dmesg | tail -20
```

Exit the monitor with **Ctrl+]**.

---

## Pairing

1. Open Bluetooth settings on any host (Linux, macOS, Windows, iOS, Android).
2. Scan for new devices and select **ESP32-C6 Keyboard**.
3. Accept the pairing dialog — no PIN is required (Just Works pairing).
4. Open a text editor and observe `hello world` being typed.

The bond is stored in NVS flash. On the next power cycle the host reconnects
and re-encrypts automatically without re-pairing.

To unpair (forget the device), remove it from the host's Bluetooth settings.
The next connection attempt will re-pair from scratch.

---

## Project structure

```
ble_hid_keyboard/
├── CMakeLists.txt          # project-level cmake
├── sdkconfig.defaults      # NimBLE and Bluetooth configuration
└── main/
    ├── CMakeLists.txt      # component registration
    ├── keycodes.h          # USB HID key usage IDs and modifier bitmasks
    ├── hid_dev.h           # hid_keyboard_report_t, API declarations
    ├── hid_dev.c           # GATT service table (DIS/BAS/HID) and callbacks
    └── main.c              # NimBLE init, GAP event handler, demo task
```

---

## Troubleshooting

**Device does not appear in Bluetooth scan**
- Confirm the board is powered and the monitor shows `advertising started`.
- Some hosts require the device to be in range and advertising for a few
  seconds before it appears.

**Pairing dialog does not appear / pairing fails**
- Remove any stale entry for "ESP32-C6 Keyboard" from the host's Bluetooth
  settings and retry.
- On Linux with BlueZ, use `bluetoothctl` to remove the device:
  ```bash
  bluetoothctl remove <MAC>
  ```

**Port permission denied**
- Confirm you are in the `dialout` group (`groups $USER`) and have logged out
  and back in since adding yourself.

**`idf.py` not found after opening a new terminal**
- Re-run `. ~/esp/esp-idf/export.sh`, or add it to `~/.bashrc` as shown above.
