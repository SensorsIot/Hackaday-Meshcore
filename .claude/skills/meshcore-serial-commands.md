# MeshCore Repeater Serial Commands

Reference guide for all serial commands available on MeshCore repeater firmware.

## Connection Settings
- Baud rate: 115200
- Line ending: CR (`\r`)

---

## General Commands

| Command | Description |
|---------|-------------|
| `ver` | Show firmware version and build date |
| `board` | Show board/manufacturer name |
| `reboot` | Reboot the device |
| `erase` | Factory reset (local serial only) |
| `advert` | Send self-advertisement to mesh |

---

## Clock/Time Commands

| Command | Description |
|---------|-------------|
| `clock` | Show current time (UTC) |
| `clock sync` | Sync clock from sender timestamp |
| `time <epoch>` | Set time to epoch seconds |

---

## Node Configuration

### Get Commands
| Command | Description |
|---------|-------------|
| `get name` | Node name |
| `get lat` | Node latitude |
| `get lon` | Node longitude |
| `get public.key` | Node public key (hex) |
| `get prv.key` | Node private key (local only) |
| `get role` | Node role (repeater/room/etc) |
| `get repeat` | Packet forwarding status (on/off) |
| `get guest.password` | Guest password |

### Set Commands
| Command | Description |
|---------|-------------|
| `set name <name>` | Set node name (max 32 chars) |
| `set lat <latitude>` | Set node latitude |
| `set lon <longitude>` | Set node longitude |
| `set prv.key <hex>` | Set private key (local only) |
| `set repeat <on\|off>` | Enable/disable packet forwarding |
| `set guest.password <pwd>` | Set guest password |
| `password <pwd>` | Change admin password |

---

## Radio Configuration

### Get Commands
| Command | Description |
|---------|-------------|
| `get radio` | Get all radio params: freq,bw,sf,cr |
| `get freq` | Get frequency (MHz) |
| `get tx` | Get TX power (dBm) |
| `get af` | Get airtime factor |
| `get rxdelay` | Get RX delay base |
| `get txdelay` | Get TX delay factor |
| `get direct.txdelay` | Get direct TX delay factor |
| `get flood.max` | Get max flood hops |
| `get int.thresh` | Get interference threshold |
| `get agc.reset.interval` | Get AGC reset interval (seconds) |
| `get multi.acks` | Get multi-ACK setting |

### Set Commands
| Command | Description |
|---------|-------------|
| `set radio <freq>,<bw>,<sf>,<cr>` | Set radio params (requires reboot) |
| `set freq <MHz>` | Set frequency (requires reboot) |
| `set tx <dBm>` | Set TX power (1-30 dBm) |
| `set af <factor>` | Set airtime factor (0-9) |
| `set rxdelay <base>` | Set RX delay base (0-20) |
| `set txdelay <factor>` | Set TX delay factor (0-2) |
| `set direct.txdelay <factor>` | Set direct TX delay factor (0-2) |
| `set flood.max <hops>` | Set max flood hops (0-64) |
| `set int.thresh <value>` | Set interference threshold |
| `set agc.reset.interval <secs>` | Set AGC reset interval |
| `set multi.acks <0\|1>` | Enable/disable multi-ACKs |
| `tempradio <freq>,<bw>,<sf>,<cr>,<mins>` | Temporary radio params |

### Radio Parameter Ranges
- **Frequency**: 300-2500 MHz
- **Bandwidth**: 7.8-500 kHz (common: 62.5, 125, 250, 500)
- **Spreading Factor**: 5-12
- **Coding Rate**: 5-8

---

## Advertisement Settings

| Command | Description |
|---------|-------------|
| `get advert.interval` | Get local advert interval (minutes) |
| `set advert.interval <mins>` | Set local advert interval (60-240 mins, 0=off) |
| `get flood.advert.interval` | Get flood advert interval (hours) |
| `set flood.advert.interval <hrs>` | Set flood advert interval (3-48 hours, 0=off) |
| `get allow.read.only` | Get read-only access setting |
| `set allow.read.only <on\|off>` | Allow read-only client access |

---

## Bridge Configuration (TCP WiFi / RS232 / ESP-NOW)

### Get Commands
| Command | Description |
|---------|-------------|
| `get bridge.type` | Bridge type (rs232/espnow/tcp/none) |
| `get bridge.enabled` | Bridge enabled status |
| `get bridge.delay` | Bridge delay (ms) |
| `get bridge.source` | Packet source (logRx/logTx) |
| `get bridge.baud` | RS232 baud rate |
| `get bridge.channel` | ESP-NOW WiFi channel |
| `get bridge.secret` | ESP-NOW secret |

### Set Commands
| Command | Description |
|---------|-------------|
| `set bridge.enabled <on\|off>` | Enable/disable bridge |
| `set bridge.delay <ms>` | Set bridge delay (0-10000 ms) |
| `set bridge.source <rx\|tx>` | Set packet source |
| `set bridge.baud <rate>` | Set RS232 baud (9600-115200) |
| `set bridge.channel <ch>` | Set ESP-NOW channel (1-14) |
| `set bridge.secret <secret>` | Set ESP-NOW secret |

---

## Neighbor Management

| Command | Description |
|---------|-------------|
| `neighbors` | List known neighbor nodes |
| `neighbor.remove <pubkey_hex>` | Remove neighbor by public key |

---

## Statistics

| Command | Description |
|---------|-------------|
| `stats-core` | Core mesh statistics |
| `stats-radio` | Radio statistics |
| `stats-packets` | Packet statistics |
| `clear stats` | Reset all statistics |

---

## Logging

| Command | Description |
|---------|-------------|
| `log` | Dump packet log file (local only) |
| `log start` | Start packet logging |
| `log stop` | Stop packet logging |
| `log erase` | Erase log file |

---

## Sensor Commands

| Command | Description |
|---------|-------------|
| `sensor list` | List all sensor settings |
| `sensor list <start>` | List sensors starting at index |
| `sensor get <key>` | Get sensor setting value |
| `sensor set <key> <value>` | Set sensor setting value |

---

## GPS Commands (if GPS enabled)

| Command | Description |
|---------|-------------|
| `gps` | Show GPS status |
| `gps on` | Enable GPS |
| `gps off` | Disable GPS |
| `gps sync` | Sync time from GPS |
| `gps setloc` | Save current GPS location to prefs |
| `gps advert` | Show location advertisement policy |
| `gps advert none` | Don't advertise location |
| `gps advert prefs` | Advertise location from prefs |
| `gps advert share` | Advertise live GPS location |

---

## ADC/Battery

| Command | Description |
|---------|-------------|
| `get adc.multiplier` | Get ADC voltage multiplier |
| `set adc.multiplier <value>` | Set ADC multiplier (0=default) |

---

## OTA Update

| Command | Description |
|---------|-------------|
| `start ota` | Start OTA update mode |

---

## Example Usage

```bash
# Configure radio for EU868 band
set radio 869.618,62.5,8,8
reboot

# Set node name and location
set name MyRepeater
set lat 52.5200
set lon 13.4050

# Check status
ver
neighbors
stats-core

# Enable TCP bridge
set bridge.enabled on
```

---

## Notes

- Commands marked "local only" work only via serial, not over mesh
- Radio parameter changes require reboot to apply
- Some commands depend on firmware configuration (GPS, bridge type)
- Max command length: 160 characters
