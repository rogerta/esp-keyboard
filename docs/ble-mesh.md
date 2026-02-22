# ESP-BLE-MESH: Comprehensive Technical Reference

Source: [ESP-IDF BLE Mesh Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/esp-ble-mesh/ble-mesh-index.html)

---

## Table of Contents

1. [Overview](#overview)
2. [Use Cases](#use-cases)
3. [Architecture](#architecture)
4. [Protocol Stack Layers](#protocol-stack-layers)
5. [Bearer Layer](#bearer-layer)
6. [Node Types](#node-types)
7. [Provisioning Process](#provisioning-process)
8. [Models](#models)
9. [Message Format and Encryption](#message-format-and-encryption)
10. [Network Management](#network-management)
11. [Feature List](#feature-list)
12. [Terminology](#terminology)

---

## Overview

ESP-BLE-MESH is Espressif's implementation of the Bluetooth SIG Mesh Profile (v1.0.1), built on top of the Zephyr Bluetooth Mesh stack. It enables many-to-many (m:m) device communication over Bluetooth Low Energy and is optimized for creating large-scale device networks.

Key characteristics:

- Built on Bluetooth Low Energy (BLE)
- Implements Mesh Profile v1.0.1
- Supports m:m communication topology
- Devices relay data to other devices outside direct radio range, extending effective network coverage
- Supports tens to thousands of devices in a single network
- Supports simultaneous Wi-Fi and mesh operation on compatible ESP32 devices

---

## Use Cases

- **Smart home automation**: Lighting control, appliance management
- **Building automation**: HVAC, occupancy sensing, access control, door management
- **Industrial sensor networks**: Large-scale distributed sensing
- **Smart lighting**: On/off, dimming, color temperature, hue/saturation control
- **Energy management**: Power monitoring and control
- **Any large-scale IoT deployment** requiring reliable, secure multi-hop wireless communication

---

## Architecture

The ESP-BLE-MESH implementation is organized into five major components:

### 1. Mesh Protocol Stack

The central component implementing most functions of the Mesh Profile specification and all client models defined in the Mesh Model specification. It is organized using:

- A **layer-based approach** for networking and provisioning subsystems
- A **module-based approach** for models

### 2. Network Management

Handles ongoing network health and security procedures (see [Network Management](#network-management)).

### 3. Features

Optional node capabilities: Low Power, Friend, Relay, and Proxy roles.

### 4. Mesh Bearer Layer

Provides the two transmission mechanisms that underpin all mesh communication (see [Bearer Layer](#bearer-layer)).

### 5. Applications

User-facing implementations that use the stack via APIs and event handler callbacks. Espressif provides reference examples for Node, Provisioner, Fast Provisioning, and Wi-Fi coexistence scenarios.

---

## Protocol Stack Layers

The mesh networking stack is hierarchical. From bottom to top:

```
+-----------------------------+
|        Access Layer         |
+-----------------------------+
|    Upper Transport Layer    |
+-----------------------------+
|    Lower Transport Layer    |
+-----------------------------+
|        Network Layer        |
+-----------------------------+
|       Bearer Layer          |
+-----------------------------+
|  BLE (Advertising / GATT)   |
+-----------------------------+
```

### Network Layer

- Defines address types and the format of network messages
- Implements the relay function of the device
- Handles network-level routing and filtering

### Lower Transport Layer

- Handles **segmentation and reassembly (SAR)** of Protocol Data Units (PDUs)
- Enables transmission of larger messages that exceed the single-packet payload limit
- Reassembles received segments back into complete PDUs in correct order

### Upper Transport Layer

- Encrypts, decrypts, and authenticates application data
- Manages **transport control messages**
- Handles friendship-related messages (Friend Request, Friend Update, Friend Poll, etc.)
- Handles **heartbeat** message transmission and reception

### Access Layer

- Defines the format of application data
- Controls encryption and decryption conducted by the Upper Transport Layer
- Defines opcodes for model messages
- Routes messages to the appropriate model handlers

---

### Provisioning Stack Layers

Provisioning uses a parallel layered structure:

```
+-------------------------------+
|       Provisioning PDUs       |
+-------------------------------+
| Generic Provisioning PDU /    |
| Proxy PDU                     |
+-------------------------------+
|    PB-ADV / PB-GATT           |
+-------------------------------+
| Advertising / Provisioning    |
| Service (session setup)       |
+-------------------------------+
```

- **PB-ADV**: Uses Generic Provisioning PDUs transmitted over BLE advertising channels
- **PB-GATT**: Uses connection-oriented channels with Mesh Provisioning Service encapsulation
- **PB-Remote**: Uses the existing mesh network to provision devices outside the provisioner's direct radio range

---

## Bearer Layer

Two bearer types carry all mesh traffic:

### Advertising Bearer

- Transmits mesh packets in the Advertising Data of a BLE advertising PDU
- Uses the **Mesh Message AD Type**
- Connectionless; any node in range receives the advertisement

### GATT Bearer

- Uses the **Proxy protocol** to transmit and receive Proxy PDUs
- Operates over a GATT connection between two devices
- Enables non-mesh devices (e.g., mobile apps) to interact with the mesh network via a Proxy Node

---

## Node Types

### Unprovisioned Device

A device that has not yet joined a mesh network. It advertises its presence using the Unprovisioned Device beacon. Examples: a new light bulb, temperature sensor, or door controller fresh from the factory.

### Node

A provisioned device that is a member of the mesh network. It can send, receive, and optionally relay messages.

### Provisioner

- A node capable of adding unprovisioned devices to the network
- Typically implemented as a gateway device or a mobile application
- Manages device onboarding: scans for unprovisioned devices, runs the provisioning protocol, assigns unicast addresses, and distributes network/application keys
- Can provision up to 100 devices in approximately 60 seconds (fast provisioning mode)

### Relay Node

- A node with the **Relay feature** enabled
- Receives network PDUs and retransmits them over the advertising bearer
- Extends the effective range of the network beyond direct radio range
- Supports up to **126 relay hops**

### Proxy Node

- A node with the **Proxy feature** enabled
- Bridges GATT and advertising bearers
- Allows devices that only support GATT (e.g., smartphones) to communicate with the mesh network
- Implements both Proxy Server and Proxy Client roles

### Friend Node

- A node with the **Friend feature** enabled
- Stores messages and security updates destined for Low Power Nodes
- Maintains a message queue for each associated Low Power Node
- Delivers stored messages when polled by the Low Power Node

### Low Power Node (LPN)

- A node with the **Low Power feature** enabled
- Operates at significantly reduced receiver duty cycles
- Establishes a **friendship** with a Friend Node
- Periodically polls the Friend Node for any queued messages rather than continuously listening
- Achieves very low power consumption, suitable for battery-powered sensors

---

## Provisioning Process

Provisioning is the process of adding an unprovisioned device to the mesh network. It is managed by a Provisioner.

### Steps

1. **Discovery**: The Provisioner scans for devices broadcasting the Unprovisioned Device beacon over BLE advertising.
2. **Invitation**: The Provisioner sends a Provisioning Invite PDU.
3. **Capabilities Exchange**: The unprovisioned device responds with its supported provisioning capabilities (OOB methods, algorithms, public key types).
4. **Authentication**: One of four OOB (Out-of-Band) methods is used to authenticate the device:
   - **No OOB**: No authentication; Static OOB field is set to 0. Lowest security.
   - **Static OOB**: Uses pre-shared static information known to both parties.
   - **Output OOB**: The unprovisioned device outputs a random number (e.g., blinks an LED N times); the user enters that number into the Provisioner.
   - **Input OOB**: The Provisioner generates a random number displayed to the user; the user inputs it into the unprovisioned device.
   - **Certificate-based Provisioning**: Uses Public Key Infrastructure (PKI) to authenticate device UUID and public key. Provides the highest level of security.
5. **Key Distribution**: The Provisioner generates and sends:
   - A **Provisioning Data** PDU containing the Network Key, IV Index, Key Refresh Flag, IV Update Flag, and the unicast address assigned to the device's primary element.
6. **Composition Data Retrieval**: After provisioning, the Provisioner reads the device's **Composition Data** to discover its elements and supported models.
7. **Configuration**: The Provisioner configures the node by binding AppKeys to models and setting publication/subscription addresses.

### Provisioning Bearers

- **PB-ADV**: Provisioning over BLE advertising (no connection required)
- **PB-GATT**: Provisioning over a GATT connection (Mesh Provisioning Service)
- **PB-Remote**: Provisioning relayed over the existing mesh network for devices not in direct radio range of the Provisioner

---

## Models

Models define the standardized behavior of nodes. Each model specifies states, messages (opcodes), and the actions to take upon receiving those messages. Models are bound to elements and communicate using AppKeys.

### Element and Model Organization

- A **device** contains one or more **elements**
- Each **element** has a unique **unicast address** and contains one or more **models**
- The **primary element** is element 0 and must contain the Configuration Server model
- Models within an element cannot share the same state type

### Foundation Models

Foundation models handle device configuration, management, and diagnostics.

| Model | Role |
|---|---|
| Configuration Server | Maintains configuration state (AppKey bindings, publication, subscription, relay, friend, proxy settings) |
| Configuration Client | Controls the Configuration Server; used by Provisioners |
| Health Server | Performs self-tests and reports fault states |
| Health Client | Retrieves self-test results and fault information |
| Remote Provisioning Server | Enables provisioning relay over the mesh network |
| Remote Provisioning Client | Initiates remote provisioning procedures |
| Directed Forwarding Configuration Server | Manages directed forwarding state |
| Directed Forwarding Configuration Client | Configures directed forwarding on nodes |
| Bridge Configuration Server | Manages subnet bridge functionality |
| Bridge Configuration Client | Configures subnet bridge behavior |
| Mesh Private Beacon Server | Configures private beacon (replaces unprovisioned/secure network beacons) |
| Mesh Private Beacon Client | Controls private beacon configuration |
| On-Demand Private Proxy Server | Configures Private Network Identity advertising |
| On-Demand Private Proxy Client | Controls on-demand private proxy |
| SAR Configuration Server | Configures segmentation and reassembly behavior |
| SAR Configuration Client | Controls SAR settings on nodes |
| Solicitation PDU RPL Configuration Server | Manages the Replay Protection List for solicitation PDUs |
| Solicitation PDU RPL Configuration Client | Controls solicitation PDU RPL configuration |
| Opcodes Aggregator Server | Processes sequences of access layer messages in a single PDU |
| Opcodes Aggregator Client | Sends aggregated opcode sequences |
| Large Composition Data Server | Exposes extended composition data and model metadata |
| Large Composition Data Client | Retrieves extended composition data |

### Generic Models

Standardized models for common device behaviors:

| Model | Description |
|---|---|
| Generic OnOff Server/Client | Binary on/off state control |
| Generic Level Server/Client | Signed 16-bit level control |
| Generic Default Transition Time Server/Client | Default state transition timing |
| Generic Power OnOff Server/Client | Behavior on power-up (off, on, restore) |
| Generic Power Level Server/Client | Power level with default and last states |
| Generic Battery Server/Client | Battery status reporting |
| Generic Location Server/Client | Global and local location reporting |
| Generic User Property Server/Client | User-configurable properties |
| Generic Admin Property Server/Client | Admin-configurable properties |
| Generic Manufacturer Property Server/Client | Read-only manufacturer properties |
| Generic Client Property Client | Retrieves property IDs from servers |

### Sensor Models

| Model | Description |
|---|---|
| Sensor Server | Exposes sensor readings and descriptors |
| Sensor Setup Server | Configures sensor cadence and settings |
| Sensor Client | Retrieves sensor data and configures sensors |

### Time and Scenes Models

| Model | Description |
|---|---|
| Time Server | Maintains network time |
| Time Setup Server | Configures time authority and zone |
| Time Client | Synchronizes and retrieves network time |
| Scene Server | Stores and recalls device state snapshots |
| Scene Setup Server | Manages stored scene definitions |
| Scene Client | Triggers scene recall and storage |
| Scheduler Server | Automates scene triggers on a schedule |
| Scheduler Setup Server | Configures scheduler entries |
| Scheduler Client | Controls and retrieves scheduler state |

### Lighting Models

| Model | Description |
|---|---|
| Light Lightness Server/Client | Perceived lightness (photometric) control |
| Light Lightness Setup Server | Configures lightness default and range |
| Light CTL Server/Client | Color Temperature of Lightness control |
| Light CTL Setup Server | Configures CTL default and range |
| Light CTL Temperature Server | Manages color temperature state |
| Light HSL Server/Client | Hue, Saturation, Lightness control |
| Light HSL Setup Server | Configures HSL defaults and ranges |
| Light HSL Hue Server | Manages hue state |
| Light HSL Saturation Server | Manages saturation state |
| Light xyL Server/Client | CIE xyY color space control |
| Light xyL Setup Server | Configures xyL defaults |
| Light LC Server/Client | Light Control (occupancy/ambient sensing) |
| Light LC Setup Server | Configures LC state machine |

### Vendor Models

Vendor models use company-specific opcodes (3 bytes with a 2-byte company identifier prefix as defined by the Bluetooth SIG). They allow manufacturers to define proprietary behavior not covered by SIG-defined models.

### Device Firmware Update Models (Preview)

| Model | Description |
|---|---|
| Firmware Update Server | Receives and applies firmware updates over the mesh |
| Firmware Update Client | Initiates and manages firmware update procedures |
| Firmware Distribution Server | Manages multi-node firmware distribution |
| Firmware Distribution Client | Triggers firmware distribution to a set of nodes |

---

## Message Format and Encryption

### Address Types

| Address Type | Value | Description |
|---|---|---|
| Unassigned Address | `0x0000` | Element not yet configured |
| Unicast Address | `0x0001`–`0x7FFF` | Unique per element; assigned during provisioning |
| Virtual Address | `0x8000`–`0xBFFF` | Derived from a 128-bit Label UUID hash; represents a logical group |
| Group Address | `0xC000`–`0xFEFF` | Multicast; subscribed to by zero or more elements |
| All Proxies | `0xFFFC` | Fixed group for proxy nodes |
| All Friends | `0xFFFD` | Fixed group for friend nodes |
| All Relays | `0xFFFE` | Fixed group for relay nodes |
| All Nodes | `0xFFFF` | Broadcast to all nodes |

### Security Keys

**Network Key (NetKey)**

- Scoped to a subnet
- Used for network-layer encryption and authentication
- Shared among all nodes in the same subnet
- Generates **Flooding Security Material** (for standard broadcast routing) and **Directed Security Material** (for directed forwarding paths)

**Application Key (AppKey)**

- Used to secure communications at the upper transport layer (payload encryption)
- Bound to a specific Network Key
- Multiple AppKeys can exist per network, allowing application-level access control
- Models must be bound to an AppKey before they can communicate

**Device Key (DevKey)**

- A special application key unique to each node
- Known only to the node itself and the Configuration Client (Provisioner)
- Used exclusively for Configuration Server model communications
- Generated during the provisioning process
- Enables the Provisioner to securely configure individual nodes without other nodes being able to decrypt configuration messages

### Encryption Layers

Mesh messages are double-encrypted:

1. **Upper Transport Layer (Application-level)**:
   - Encrypted and authenticated using AppKey (for application models) or DevKey (for Configuration Server model)
   - Produces an encrypted payload with a MIC (Message Integrity Check)

2. **Network Layer**:
   - Encrypted and authenticated using the Network Key
   - Protects source and destination addresses and the upper-transport PDU
   - Prevents traffic analysis by devices outside the subnet

### Message Types

| Type | Description |
|---|---|
| Unacknowledged | Fire-and-forget; no response expected |
| Acknowledged | Requires a response (status message) from the target |

### Segmentation and Reassembly (SAR)

- Large messages that exceed the single-PDU payload are split by the Lower Transport Layer into segments
- Each segment is independently transmitted over the network
- The receiver reassembles segments in the correct order before passing to the upper layer
- SAR parameters (e.g., segment transmission interval, retransmit count) are configurable via the SAR Configuration models

### Replay Protection

- Each message includes a **Sequence Number (SEQ)** incremented monotonically
- Nodes maintain a **Replay Protection List (RPL)** recording the highest sequence number seen from each source address
- Duplicate or replayed messages (same SEQ from same source) are discarded
- The IV Index (32-bit, updated via IV Update Procedure) extends the sequence number space and prevents wrap-around attacks

---

## Network Management

### Node Removal

A node can be removed from the network (blacklisted) by the Provisioner. After removal, a Key Refresh Procedure is performed to prevent the removed node from decrypting future network traffic.

### IV Update Procedure

- Updates the 32-bit **IV Index** value included in all network messages
- Expands the effective sequence number space, preventing SEQ exhaustion
- The procedure has a minimum interval of **96 hours** between successive updates
- All nodes in the network participate in the procedure simultaneously

### Key Refresh Procedure

- Updates compromised or potentially compromised Network Keys and Application Keys
- Executed in three phases: distributing new keys, transitioning to new keys, revoking old keys
- Ensures that a node removed from the network cannot decrypt subsequent communications

### NVS Storage

- Provisioning data and node configuration are stored in Non-Volatile Storage (NVS) on the ESP32
- Data persists across power cycles and reboots
- Includes NetKeys, AppKeys, unicast addresses, subscription lists, and publication settings

---

## Feature List

### Mesh Core

**Provisioning**

- PB-ADV (advertising bearer provisioning)
- PB-GATT (GATT connection provisioning)
- PB-Remote (remote provisioning over mesh)
- OOB Authentication (No OOB, Static OOB, Input OOB, Output OOB)
- Certificate-based Provisioning
- Enhanced Provisioning Authentication

**Networking**

- Relay
- Segmentation and Reassembly (SAR)
- Key Refresh Procedure
- IV Update Procedure
- Friend capability
- Low Power mode
- Proxy Server
- Proxy Client
- Directed Forwarding
- Private Beacon
- Subnet Bridge
- Minor Enhancements (as per Mesh Profile specification errata)
- Device Firmware Update *(Preview)*

**Implementation**

- Multiple simultaneous client model operations (non-blocking)
- NVS storage of provisioning and configuration data

### Mesh Models Supported

**Foundation Models (20 models)**

- Configuration Server + Client
- Health Server + Client
- Remote Provisioning Server + Client
- Directed Forwarding Configuration Server + Client
- Bridge Configuration Server + Client
- Mesh Private Beacon Server + Client
- On-Demand Private Proxy Server + Client
- SAR Configuration Server + Client
- Solicitation PDU RPL Configuration Server + Client
- Opcodes Aggregator Server + Client
- Large Composition Data Server + Client

**Generic Models (15 models)**

- Generic OnOff Server + Client
- Generic Level Server + Client
- Generic Default Transition Time Server + Client
- Generic Power OnOff Server + Client + Setup Server
- Generic Power Level Server + Client + Setup Server
- Generic Battery Server + Client
- Generic Location Server + Client + Setup Server
- Generic User/Admin/Manufacturer Property Server + Client

**Sensor Models**

- Sensor Server + Setup Server + Client

**Time and Scenes Models**

- Time Server + Setup Server + Client
- Scene Server + Setup Server + Client
- Scheduler Server + Setup Server + Client

**Lighting Models**

- Light Lightness Server + Setup Server + Client
- Light CTL Server + Setup Server + Client + Temperature Server
- Light HSL Server + Setup Server + Client + Hue Server + Saturation Server
- Light xyL Server + Setup Server + Client
- Light LC Server + Setup Server + Client

**Device Firmware Update Models *(Preview)***

- Firmware Update Server + Client
- Firmware Distribution Server + Client

### Examples Provided

- Node example
- Provisioner example
- Fast Provisioning example (100 nodes in ~60 seconds)
- Wi-Fi coexistence example

---

## Terminology

### Roles

| Term | Definition |
|---|---|
| Unprovisioned Device | A device not yet part of a mesh network; advertises via Unprovisioned Device beacon |
| Node | A provisioned device that can send, receive, or relay mesh messages |
| Relay Node | A node with the Relay feature enabled; retransmits messages up to 126 hops |
| Proxy Node | A node bridging GATT and advertising bearers; enables smartphone/app mesh access |
| Friend Node | Stores messages and security updates for associated Low Power Nodes |
| Low Power Node (LPN) | Operates at reduced receiver duty cycles; polls Friend Node for queued messages |
| Provisioner | A device that adds unprovisioned devices to the network; typically a gateway or mobile app |

### Composition

| Term | Definition |
|---|---|
| State | A value representing a device condition (e.g., on/off, brightness level, color temperature) |
| Model | Defines node functionality: states it holds, messages it processes, and resulting actions; categorized as SIG or Vendor models |
| Element | An addressable entity within a device; has a unicast address and one or more models; device has at least one element (primary element) |
| Composition Data | Information about a node's elements and supported models; read by the Provisioner after provisioning |

### Features

| Term | Definition |
|---|---|
| Low Power Feature | The ability to operate within a mesh network at significantly reduced receiver duty cycles, with Friend support |
| Friend Feature | The ability to help a Low Power Node operate by storing messages destined for it |
| Relay Feature | The ability to receive and retransmit mesh messages over the advertising bearer to extend network coverage |
| Proxy Feature | The ability to receive and retransmit mesh messages between GATT and advertising bearers |

### Provisioning

| Term | Definition |
|---|---|
| Provisioning | The process of adding an unprovisioned device to a mesh network, managed by a Provisioner |
| PB-ADV | Provisioning Bearer using Generic Provisioning PDUs over BLE advertising channels |
| PB-GATT | Provisioning Bearer using GATT connection with Mesh Provisioning Service |
| PB-Remote | Provisioning Bearer relayed through the existing mesh network |
| Input OOB | User inputs a random number (generated by Provisioner) into the unprovisioned device |
| Output OOB | Unprovisioned device outputs a random number (e.g., LED blinks); user enters it into Provisioner |
| Static OOB | Authentication using pre-shared static information |
| No OOB | No authentication; Static OOB field set to 0 |
| Certificate-based Provisioning | Uses PKI to authenticate device UUID and public key |

### Addresses

| Term | Definition |
|---|---|
| Unassigned Address | `0x0000`; element not yet configured |
| Unicast Address | Unique address per element assigned during provisioning (`0x0001`–`0x7FFF`) |
| Virtual Address | Represents a set of destinations; derived from a 128-bit Label UUID via hash (`0x8000`–`0xBFFF`) |
| Group Address | Multicast address subscribed to by zero or more elements (`0xC000`–`0xFEFF`) |

### Security

| Term | Definition |
|---|---|
| Device Key (DevKey) | A unique application key per node, known only to the node and Configuration Client; used for configuration message encryption |
| Application Key (AppKey) | Secures upper transport layer communications; bound to a specific Network Key |
| Network Key (NetKey) | Secures network-layer communications for all nodes in a subnet |
| Flooding Security Material | Derived from NetKey; enables decryption by any node in the same network subnet |
| Directed Security Material | Derived from NetKey; used specifically for nodes in directed forwarding paths |
| Replay Protection List (RPL) | Per-node record of highest sequence numbers seen per source; prevents replay attacks |
| IV Index | 32-bit value included in network PDUs; updated periodically to extend sequence number space |

### Messaging

| Term | Definition |
|---|---|
| Segmentation and Reassembly (SAR) | Method of splitting large PDUs into smaller segments for transmission and reassembling them at the receiver |
| Unacknowledged Message | A message sent without expecting a response |
| Acknowledged Message | A message that expects a status response from the recipient |
| Heartbeat | A transport control message used to monitor mesh topology and node activity |
| Opcode | A 1, 2, or 3-byte code identifying the operation defined by a model message |

### Network Management

| Term | Definition |
|---|---|
| Key Refresh Procedure | Updates compromised or potentially compromised network and application keys across the network |
| IV Update Procedure | Updates the IV Index value; has a minimum 96-hour interval between successive updates |
| Friendship | The relationship between a Friend Node and a Low Power Node; established via Friend Request/Offer exchange |

---

*This document is based on the ESP-IDF ESP-BLE-MESH documentation for ESP32, implementing Bluetooth Mesh Profile v1.0.1.*
