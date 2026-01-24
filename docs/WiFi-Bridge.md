# WiFi TCP Bridge for MeshCore

## Overview

The WiFi TCP Bridge enables two MeshCore repeaters to relay packets over a TCP/IP connection,
linking geographically separate LoRa mesh networks via WiFi/internet.

## Use Cases

- **Link distant meshes**: Connect two LoRa networks separated by distance using internet backhaul
- **Building penetration**: Bridge outdoor LoRa network to indoor WiFi-connected repeater
- **Cross-band linking**: Connect meshes operating on different frequencies
- **Redundant paths**: Provide backup connectivity when LoRa path is degraded

## Requirements

- ESP32-based repeater (Heltec V3, LilyGo T3S3, etc.)
- WiFi network access (configured via captive portal)
- Two repeaters: one as server, one as client
- Network connectivity between repeaters (same LAN or routable IPs)

## Architecture

```
Repeater A (Server)                    Repeater B (Client)
┌─────────────────┐                    ┌─────────────────┐
│   LoRa Radio    │                    │   LoRa Radio    │
│       ↓↑        │                    │       ↓↑        │
│    MyMesh       │                    │    MyMesh       │
│       ↓↑        │                    │       ↓↑        │
│  TCPWifiBridge  │◄──── TCP/IP ──────►│  TCPWifiBridge  │
│   (Server)      │     Connection     │   (Client)      │
└─────────────────┘                    └─────────────────┘
```

## Configuration

### Compile-Time (platformio.ini)

```ini
[env:Heltec_v3_repeater_bridge_tcp]
extends = Heltec_lora32_v3
build_flags =
  ${Heltec_lora32_v3.build_flags}
  -D WITH_TCP_WIFI_BRIDGE=1
build_src_filter = ${Heltec_lora32_v3.build_src_filter}
  +<helpers/bridges/BridgeBase.cpp>
  +<helpers/bridges/TCPWifiBridge.cpp>
  +<helpers/bridges/TCPWifiPortal.cpp>
  +<../examples/simple_repeater>
```

No WiFi credentials needed at compile time - configured via captive portal.

### Standard Repeater Commissioning (existing)

Repeaters are currently configured via:
- **USB Serial** - https://config.meshcore.dev web tool
- **Remote LoRa** - MeshCore mobile app's Remote Management

These methods still work. The TCP bridge adds WiFi-specific setup.

### TCP Bridge Setup (Captive Portal)

1. Flash firmware with `WITH_TCP_WIFI_BRIDGE=1`
2. Device starts AP: **"MeshCore-TCP-Setup"**
3. Connect to AP from phone/laptop
4. Browser opens config page (or navigate to 192.168.4.1)
5. Enter WiFi credentials and TCP bridge settings
6. Device saves config and reboots

The captive portal only appears when WiFi is not configured. Normal repeater settings (name, freq, power) are still configured via USB or LoRa remote management.

### Runtime (Serial CLI)

| Command | Description | Default |
|---------|-------------|---------|
| `tcp mode 0` | Set as server (listens for connections) | 0 |
| `tcp mode 1` | Set as client (connects to remote) | - |
| `tcp port 5555` | Set TCP port | 5555 |
| `tcp host 192.168.1.100` | Set remote host (client mode) | - |
| `tcp status` | Show connection status | - |
| `tcp save` | Save configuration | - |
| `tcp wifi reset` | Clear WiFi config, restart portal | - |

## Setup Example

### Scenario: Link two meshes across sites (appearing as single node)

**Step 1: Set same node name on both repeaters**
```
set name LinkedRepeater
```

**Step 2: Configure Site A (Server)**

Via captive portal or CLI:
```
tcp mode 0
tcp port 5555
tcp save
```

**Step 3: Configure Site B (Client)**

Via captive portal or CLI:
```
tcp mode 1
tcp host 192.168.1.100
tcp port 5555
tcp save
```

**Result:** Users on both meshes see a single repeater named "LinkedRepeater"

### Connection Flow

1. Server starts listening on configured port
2. Client connects to server's IP:port
3. Once connected, packets flow bidirectionally
4. If connection drops, client auto-reconnects

## Packet Flow

```
[Site A Mesh]                              [Site B Mesh]
     │                                          │
     ▼                                          ▼
┌─────────┐    LoRa    ┌──────────┐        ┌──────────┐    LoRa    ┌─────────┐
│  Node   │◄─────────►│ Repeater │        │ Repeater │◄─────────►│  Node   │
│         │           │ (Server) │        │ (Client) │           │         │
└─────────┘           └────┬─────┘        └────┬─────┘           └─────────┘
                           │                   │
                           │   TCP/IP Link     │
                           └───────────────────┘
```

## Technical Details

### Protocol

Uses same framing as RS232 bridge:
- Magic header: `0xC03E` (2 bytes)
- Length: big-endian (2 bytes)
- Payload: mesh packet (variable)
- Checksum: Fletcher-16 (2 bytes)

### Duplicate Prevention

The bridge uses `SimpleMeshTables` to track seen packets, preventing:
- Packets from looping back through the bridge
- Duplicate transmissions of the same packet

### Configuration Storage

TCP bridge settings stored in `/tcp_bridge.cfg` (separate from main prefs):
- Mode (server/client)
- Port number
- Remote host
- WiFi credentials

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Captive portal not showing | Connect to "MeshCore-TCP-Setup" AP, navigate to 192.168.4.1 |
| WiFi not connecting | Run `tcp wifi reset` to clear config and restart portal |
| Client can't connect | Check server IP, port, firewall rules |
| Packets not flowing | Verify `bridge_enabled` is on, check `tcp status` |
| Frequent disconnects | Check WiFi signal, increase reconnect interval |

## Limitations

- ESP32 only (requires WiFi hardware)
- Single TCP connection (point-to-point only)
- No encryption over TCP link (use VPN for security over internet)
- WiFi configured via captive portal (one-time setup required)

## See Also

- [RS232 Bridge](./faq.md) - Serial bridge for wired connections
- [ESP-NOW Bridge](./faq.md) - Direct WiFi bridge without router
