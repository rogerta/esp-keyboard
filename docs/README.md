# ESP32 Bluetooth Documentation

Extracted from [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/).
Focus: **ESP32-C3** (BLE 5.0) and **ESP32-C6** (BLE 5.3). Both are BLE-only (no Classic Bluetooth).

## Files

| File | Description | Size |
|------|-------------|------|
| [esp32-c3-c6-bluetooth.md](esp32-c3-c6-bluetooth.md) | **START HERE** — C3 vs C6 side-by-side comparison, feature support matrix, coexistence | 16 KB |
| [bt-architecture.md](bt-architecture.md) | Stack architecture, Bluedroid vs NimBLE, HCI/VHCI, task priorities, memory | 18 KB |
| [ble-overview.md](ble-overview.md) | BLE protocol layers, chip support matrix, GAP/GATT/SMP feature status table | 11 KB |
| [ble-get-started.md](ble-get-started.md) | Device discovery, connection, GATT data exchange — with NimBLE code examples | 39 KB |
| [ble-security.md](ble-security.md) | SMP pairing, LE Secure Connections, key distribution, BluFi (deprecated) | 39 KB |
| [ble-advanced.md](ble-advanced.md) | Multi-connection (up to 9), low power modes, power consumption figures | 18 KB |
| [ble-mesh.md](ble-mesh.md) | BLE Mesh architecture, node types, provisioning, models, encryption | 27 KB |
| [nimble-and-controller.md](nimble-and-controller.md) | NimBLE API, Controller/VHCI, bt_common, WiFi coexistence | 39 KB |
| [classic-bt.md](classic-bt.md) | Classic BT profiles (A2DP, SPP, HFP, etc.) — **not available on C3/C6** | 57 KB |

## C3/C6 Quick Reference

### What's NOT available on C3/C6
- Classic Bluetooth (A2DP, SPP, HFP, HID, AVRCP) — original ESP32 only
- BLE Mesh: verify chip support before using on C2 variants

### C3 vs C6 BLE Feature Delta

| Feature | C3 | C6 |
|---------|----|----|
| BT Spec certified | 5.4 | 5.3 |
| 2M PHY | ✓ | ✓ |
| Coded PHY (125/500 kbps, long range) | ✓ | ✓ |
| Extended advertising (1650 byte payload) | ✓ | ✓ |
| Channel Selection Algorithm #2 | ✓ | ✓ |
| Periodic Advertising Sync Transfer | ✗ | ✓ |
| AdvDataInfo in Periodic Advertising | ✗ | ✓ |
| LE Channel Classification | ✗ | Experimental |
| Periodic Advertising with Responses | ✗ | Experimental |
| BLE 6.0 features | ✗ | Developing (est. 2026-06) |
| 802.15.4 (Thread/Zigbee) coexistence | ✗ | ✓ (with caveats) |

### Host Stack Recommendation
**NimBLE** over Bluedroid for C3/C6:
- Lower RAM footprint
- Better BLE 5.x feature coverage
- Apache 2.0 license
- `CONFIG_BT_NIMBLE_ENABLED=y` in sdkconfig

### Low Power BLE
- 32 kHz external crystal → best sleep current, 500 PPM accuracy
- 136 kHz internal RC → only for legacy advertising/scanning, exceeds 500 PPM
- C6 light sleep reaches ~24 µA with 32 kHz XTAL

### WiFi + BLE Coexistence (both chips)
- Single 2.4 GHz RF, time-division multiplexing
- Requires `CONFIG_ESP_COEX_SW_COEXIST_ENABLE=y`
- C6 additionally arbitrates with 802.15.4; **BLE connected + Thread scanning = unstable**

### Memory
- Releasing unused BT memory: `esp_bt_controller_mem_release()` recovers ~70 KB
- Default BLE max connections: 3 (configurable up to 9 via `BTDM_CTRL_BLE_MAX_CONN`)

### TX Power
- Range: -12 dBm to +9 dBm in 3 dBm steps (`N12` … `P9`)
- Per-connection control available via `esp_ble_tx_power_set()`

### Security
- Use **LE Secure Connections** (LESC): ECDH-based, LTK never transmitted over air
- Do NOT use BluFi for new projects — use `network_provisioning` component instead
- Security Mode 1 Level 4 = LESC + MITM protection + bonding (highest)
