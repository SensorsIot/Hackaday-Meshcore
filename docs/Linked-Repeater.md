# Linked Repeater for MeshCore

## Overview

Linked Repeaters enable two or more MeshCore repeaters to relay packets between each other, linking geographically separate LoRa mesh networks. This creates a unified mesh that spans multiple locations connected via WiFi, internet, or serial links.

## Link Types

| Type | Connection | Use Case |
|------|------------|----------|
| **TCP WiFi** | WiFi/Internet | Link over LAN or internet |
| **ESP-NOW** | Direct WiFi | Short-range WiFi without router |
| **RS232** | Serial cable | Wired point-to-point connection |

## Use Cases

- **Link distant meshes**: Connect LoRa networks separated by distance using internet backhaul
- **Building penetration**: Bridge outdoor LoRa network to indoor WiFi-connected repeater
- **Cross-band linking**: Connect meshes operating on different frequencies
- **Redundant paths**: Provide backup connectivity when LoRa path is degraded
- **Site interconnection**: Link multiple buildings or locations into one mesh

## Requirements

### TCP WiFi Bridge
- ESP32-based repeater (Heltec V3, LilyGo T3S3, T-Beam, etc.)
- WiFi network access (configured via captive portal)
- Two repeaters: one as server, one as client
- Network connectivity between repeaters (same LAN or routable IPs)

### ESP-NOW Bridge
- ESP32-based repeater
- Two repeaters within WiFi range (~200m line of sight)
- No router or internet required

### RS232 Bridge
- Any supported platform (ESP32, nRF52, RP2040, STM32)
- Serial cable between devices
- Matching baud rate configuration

## Architecture

```
Site A                                      Site B
┌─────────────────┐                        ┌─────────────────┐
│   LoRa Radio    │                        │   LoRa Radio    │
│       ↓↑        │                        │       ↓↑        │
│    MyMesh       │                        │    MyMesh       │
│       ↓↑        │                        │       ↓↑        │
│  Linked Repeater│◄───── Link ──────────►│  Linked Repeater│
│   (Server)      │  (TCP/ESPNow/RS232)    │   (Client)      │
└─────────────────┘                        └─────────────────┘
        │                                          │
        ▼                                          ▼
   [Local Mesh]                               [Local Mesh]
```

## Available Build Targets

### Heltec V3
- `Heltec_v3_repeater_bridge_tcp`
- `Heltec_v3_repeater_bridge_espnow`
- `Heltec_v3_repeater_bridge_rs232`

### LilyGo T3S3
- `LilyGo_T3S3_sx1262_repeater_bridge_tcp`
- `LilyGo_T3S3_sx1262_repeater_bridge_espnow`

### LilyGo T-Beam
- `Tbeam_SX1262_repeater_bridge_tcp`
- `Tbeam_SX1262_repeater_bridge_espnow`
- `Tbeam_SX1276_repeater_bridge_tcp`
- `Tbeam_SX1276_repeater_bridge_espnow`

### LilyGo T-Beam Supreme
- `T_Beam_S3_Supreme_SX1262_repeater_bridge_tcp`
- `T_Beam_S3_Supreme_SX1262_repeater_bridge_espnow`

### LilyGo T-Deck
- `LilyGo_TDeck_repeater_bridge_tcp`

### LilyGo T-LoRa v2.1
- `LilyGo_TLora_V2_1_1_6_repeater_bridge_tcp`
- `LilyGo_TLora_V2_1_1_6_repeater_bridge_espnow`
- `LilyGo_TLora_V2_1_1_6_repeater_bridge_rs232`

## TCP WiFi Bridge Setup

### Captive Portal Configuration

1. Flash firmware with TCP bridge target (e.g., `Heltec_v3_repeater_bridge_tcp`)
2. Device starts AP: **"MeshCore-TCP-Setup"**
3. Connect to AP from phone/laptop
4. Browser opens config page (or navigate to 192.168.4.1)
5. Enter WiFi credentials and TCP bridge settings:
   - WiFi SSID and password
   - Mode: Server or Client
   - TCP Port (default: 5555)
   - Remote Host (client mode only)
6. Device saves config and reboots

### CLI Commands

| Command | Description | Default |
|---------|-------------|---------|
| `tcp mode 0` | Set as server (listens for connections) | 0 |
| `tcp mode 1` | Set as client (connects to remote) | - |
| `tcp port 5555` | Set TCP port | 5555 |
| `tcp host 192.168.1.100` | Set remote host (client mode) | - |
| `tcp status` | Show connection status | - |
| `tcp save` | Save configuration | - |
| `tcp wifi reset` | Clear WiFi config, restart portal | - |

### Example: Link Two Sites

**Site A (Server):**
```
tcp mode 0
tcp port 5555
tcp save
```

**Site B (Client):**
```
tcp mode 1
tcp host 192.168.1.100
tcp port 5555
tcp save
```

## ESP-NOW Bridge Setup

ESP-NOW provides direct WiFi linking without a router.

### CLI Commands

| Command | Description |
|---------|-------------|
| `bridge on` | Enable bridge |
| `bridge off` | Disable bridge |
| `bridge channel 1` | Set WiFi channel (1-14) |
| `bridge secret MYKEY` | Set encryption key |

Both devices must use the same channel and secret.

## RS232 Bridge Setup

### CLI Commands

| Command | Description |
|---------|-------------|
| `bridge on` | Enable bridge |
| `bridge off` | Disable bridge |
| `bridge baud 115200` | Set baud rate |

### Wiring

Connect TX of one device to RX of the other, and vice versa. Ground must be common.

## Common Bridge Settings

All bridge types share these settings:

| Command | Description | Default |
|---------|-------------|---------|
| `bridge on` | Enable bridge | on |
| `bridge off` | Disable bridge | - |
| `bridge delay 500` | Packet processing delay (ms) | 500 |
| `bridge src 0` | Packet source: 0=TX, 1=RX | 0 |

## Transparent Operation

For mesh users to see linked repeaters as a single logical node:

1. **Same node name** - Configure both repeaters with identical `node_name`
2. **Bridge filters duplicates** - Prevents packet loops
3. **Seamless routing** - Messages route to whichever side receives them first

## Technical Details

### Protocol

All bridge types use the same framing:
- Magic header: `0xC03E` (2 bytes)
- Length: big-endian (2 bytes)
- Payload: mesh packet (variable)
- Checksum: Fletcher-16 (2 bytes)

### Duplicate Prevention

The bridge uses `SimpleMeshTables` to track seen packets, preventing:
- Packets from looping back through the bridge
- Duplicate transmissions of the same packet

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Captive portal not showing | Connect to "MeshCore-TCP-Setup" AP, navigate to 192.168.4.1 |
| WiFi not connecting | Run `tcp wifi reset` to clear config and restart portal |
| Client can't connect | Check server IP, port, firewall rules |
| Packets not flowing | Verify `bridge on`, check `tcp status` or `bridge status` |
| Frequent disconnects | Check WiFi signal strength |
| ESP-NOW not linking | Verify same channel and secret on both devices |
| RS232 not working | Check TX/RX wiring, verify matching baud rate |

## Building Firmware

### Manual Build

```bash
# Build all linked repeater firmwares
FIRMWARE_VERSION=v1.0.0 sh build.sh build-linked-repeater-firmwares

# Build specific target
FIRMWARE_VERSION=v1.0.0 pio run -e Heltec_v3_repeater_bridge_tcp
```

### GitHub Actions

Push a tag to trigger automated builds:
```bash
git tag linked-repeater-v1.0.0
git push --tags
```

Or trigger manually from Actions → "Build Linked Repeater Firmwares" → Run workflow.

## OTA Firmware Updates

When connected to WiFi, TCP bridge devices expose an OTA (Over-The-Air) update endpoint. This allows firmware updates without physical access to the device.

### Web Interface

Navigate to `http://<device-ip>/` to see the device info and link to firmware update page.

### OTA Update via curl

```bash
# Upload firmware to device
curl -F "MD5=$(md5sum firmware.bin | cut -d' ' -f1)" \
     -F "firmware=@firmware.bin" \
     http://192.168.0.89/update
```

The device will reboot automatically after a successful update.

## Bridge Status Display

The OLED displays a compact 4-line layout for linked repeaters:

```
MyRepeater
906.875/12 125.0/5
Bridge: Connected
192.168.0.89
```

| Y | Content | Format |
|---|---------|--------|
| 0 | Node name | `MyRepeater` |
| 15 | freq/SF BW/CR | `906.875/12 125.0/5` |
| 30 | Bridge status | `Bridge: Connected` |
| 45 | IP address | `192.168.0.89` |

Bridge status values:

| Display | Meaning |
|---------|---------|
| `Off` | Bridge not initialized |
| `Portal Mode` | Captive portal active, awaiting WiFi config |
| `WiFi Disconn` | WiFi configured but not connected |
| `Searching...` | WiFi connected, looking for peer |
| `Connecting...` | Peer discovered, establishing connection |
| `Connected` | Fully connected to peer |

## Discovering Device IP Addresses

### Method 1: OLED Display

The device shows its IP address on the OLED screen (line 4) after WiFi connects:
```
MyRepeater
906.875/12 125.0/5
Bridge: Connected
192.168.0.89
```

### Method 2: Network Scan for MeshCore Devices

Scan your network and identify MeshCore devices by their web page response:

```bash
# Scan and show all MeshCore devices with their IPs
for ip in 192.168.0.{1..254}; do
  (timeout 0.3 curl -s "http://$ip/" 2>/dev/null | grep -q "MeshCore" && echo "$ip") &
done 2>/dev/null; wait
```

Each device responds with its MAC address in the web interface, allowing identification.

### Method 3: DHCP Reservation (Recommended)

For permanent installations, configure static DHCP leases in your router using the known MAC addresses:

| MAC Address | Reserved IP | Device |
|-------------|-------------|--------|
| 90:15:06:CE:10:F4 | 192.168.0.89 | Bridge #1 |
| 10:06:1C:16:D6:38 | 192.168.0.90 | Bridge #2 |

This ensures devices always get the same IP after reboot.

## Test Hardware

| Device | MAC Address | IP (DHCP) | Role |
|--------|-------------|-----------|------|
| LilyGo T-LoRa V2.1-1.6 #1 | 90:15:06:CE:10:F4 | 192.168.0.89 | TCP Bridge |
| LilyGo T-LoRa V2.1-1.6 #2 | 10:06:1C:16:D6:38 | 192.168.0.90 | TCP Bridge |

## Time Synchronization

Linked repeaters automatically sync their RTC from companion nodes (devices connected to smartphones).

### How It Works

1. Companion nodes (ADV_TYPE_CHAT) broadcast advertisements containing their timestamp
2. Companions get correct time from smartphone apps
3. Repeaters extract timestamp from received advertisements
4. If local clock is off by >60 seconds, RTC is updated

### Network Time

- All timestamps are **UTC** (Unix epoch)
- Provides consistent time reference across the mesh
- No timezone configuration needed
- Encapsulated in `MyMesh::syncTimeFromCompanion()`

## Limitations

- **TCP WiFi**: ESP32 only, single connection, no built-in encryption (use VPN)
- **ESP-NOW**: ESP32 only, limited range (~200m), requires same WiFi channel
- **RS232**: Requires physical cable, limited by cable length
- **Time sync**: Requires companion node in range to sync RTC

## Fork Changes Summary

This fork adds the following features to the original MeshCore project:

### New Files
| File | Purpose |
|------|---------|
| `src/helpers/bridges/TCPWifiBridge.cpp/h` | TCP WiFi bridge implementation |
| `src/helpers/bridges/TCPWifiPortal.cpp/h` | Captive portal for WiFi configuration |
| `.github/workflows/build-linked-repeater-firmwares.yml` | CI/CD for bridge builds |
| `docs/Linked-Repeater.md` | This documentation |

### Modified Files
| File | Changes |
|------|---------|
| `src/helpers/AbstractBridge.h` | Added `getStatusString()` virtual method |
| `src/helpers/bridges/BridgeBase.h` | Default status string implementation |
| `examples/simple_repeater/UITask.cpp` | OLED shows bridge status + IP |
| `examples/simple_repeater/main.cpp` | Bridge initialization |
| `variants/*/platformio.ini` | Build targets for TCP bridges |
| `build.sh` | Build script for linked repeater firmwares |

### Features Added
1. **TCP WiFi Bridge** - Link repeaters over IP networks with auto-discovery
2. **Captive Portal** - Web-based WiFi and bridge configuration
3. **OTA Updates** - Firmware updates over WiFi (no physical access needed)
4. **Status Reporting** - Accurate connection state on OLED display
5. **IP Display** - Shows device IP on OLED when connected
6. **Bridge Logging** - Enhanced logs with packet type and src/dest hashes
7. **Time Sync** - RTC sync from companion nodes (UTC network time)

### Encapsulation
Changes are well-encapsulated:
- Bridge logic is isolated in `TCPWifiBridge.*` and `TCPWifiPortal.*`
- Only interface change is `getStatusString()` in `AbstractBridge.h`
- UI changes are conditional (`#ifdef ESP_PLATFORM`)
- Build targets are additive (don't modify existing targets)
- Time sync in `MyMesh::syncTimeFromCompanion()` with clear comments
- Bridge logging in `TCPWifiBridge::logPacket()` helper
