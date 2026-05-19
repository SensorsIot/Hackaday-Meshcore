# Hackaday-Meshcore Design

Long-lived MIT fork of [MeshCore](https://github.com/meshcore-dev/MeshCore) adapted for the Hackaday 2025 Communicator Badge. Established 2026-05-19 via a structured grilling session that locked 19 architectural decisions.

This document is the **canonical project reference**. Hardware facts (pin maps, controller IDs, key map) live in the companion notes doc `hackaday-2025-communicator-badge-meshcore-notes.md`. This file captures architecture, scope, license posture, and the milestone plan.

## Project Overview

- **Target hardware:** Hackaday 2025 Communicator Badge — ESP32-S3-WROOM-1, NV3007 428×142 SPI display, TCA8418 I2C keyboard (8×10 matrix), Wio-SX1262-N LoRa module, Li-Po + MCP73831 charger + AP2112K-3.3 LDO, USB-C, two switches, two LEDs. No GPS, no buzzer, no microSD.
- **Fork repo:** `SensorsIot/Hackaday-Meshcore` (to be created as a fork of `meshcore-dev/MeshCore`).
- **Workspace:** `~/Hackaday/` IS the fork root. Container `hackaday`, SSH host port 2471. Single-purpose container.
- **Sync:** long-running `hackaday` branch, merge-from-upstream-main; no rebase.
- **License:** MIT (matches upstream MeshCore).

## Guiding Principle — No Artificial Simplifications

Default to **full MeshCore feature parity**. Do not propose "v1 minimum" reductions that strip features upstream already supports. Feature reductions are only acceptable when driven by real constraints (hardware that doesn't exist on the device, license incompatibility, legal restriction).

Valid reductions on this project:
- **No GPS** in firmware paths — BOM confirms no GNSS chip is populated.
- **No buzzer** — BOM confirms no buzzer is populated.
- **No battery percentage indicator in v1** — `BAT_ADC_PIN` routing on GPIO4 is unverified in the schematic; Meshtastic's own port commented it out. Revisit once probed on hardware.

Invalid reductions (rejected during grilling):
- "USB-only flashing, defer OTA" — rejected. MeshCore has `AsyncElegantOTA` natively; un-gate it for the badge from v1.

## Architectural Decisions

### 1. Goal — long-lived fork
Personal/community fork, not an upstream PR. Custom UI/UX layer is the reason the fork exists; the variant itself could theoretically be upstreamed later, so keep it upstream-PR-shaped.

### 2. Code Layout
Standard MeshCore conventions:
- **BSP:** `variants/hackaday_communicator/` — pin macros, `HackadayBoard.cpp/h`, `NV3007Display.cpp/h`, `TCA8418Keyboard.cpp/h`, `target.cpp/h`, `platformio.ini`.
- **App:** `examples/badge_chat/` — fork of `companion_radio/`. Replace its `ui-orig/` and `ui-new/` with our `BadgeUITask : public AbstractUITask` and screens.

### 3. Starting Point
Fork **`examples/companion_radio/`** (not `simple_secure_chat`). Reasons:
- BLE companion path to MeshCore phone app is built in — phone-app compatibility was a roadmap requirement.
- `AbstractUITask` contract (`newMsg`, `notify`, `msgRead`, `loop`) matches our needs.
- `DataStore` over SPIFFS solves persistence with zero work.
- `NodePrefs` (22 fields) is the editable settings schema.

### 4. Display Driver — Native NV3007
- **Class:** `NV3007Display : public DisplayDriver` in `variants/hackaday_communicator/`.
- **Approach:** direct ESP32-S3 SPI (HSPI), no Arduino_GFX or LVGL.
- **Rendering model:** 1-bit monochrome DRAM framebuffer (~7.6 KB), flushed in `endFrame()` as 16-bit big-endian RGB565 with two-color mapping (`DARK`→`TFT_BLACK`, anything else→`TFT_MESH`).
- **Init sequence:** byte sequence from the NV3007 init operations (hardware facts; not copied source); equivalent to what Meshtastic's `Arduino_NV3007` does.
- **DisplayDriver compliance:** implement all required virtual methods (`startFrame`, `endFrame`, `setCursor`, `print`, `fillRect`, `drawRect`, `drawXbm`, `getTextWidth`, etc.) so future MeshCore UI primitives could in theory render against this driver.

### 5. Keyboard Driver — TCA8418
- **Class:** `TCA8418Keyboard` in `variants/hackaday_communicator/` (variant-private; can be promoted to `src/helpers/ui/` later if reused).
- **Wiring:** I2C on SDA GPIO47, SCL GPIO14, address `0x34`, interrupt on GPIO13 (`KB_INT`).
- **Mode:** **interrupt-driven**. ISR sets a flag only; main loop drains the TCA8418 event FIFO over I2C when the flag is set. No I2C transactions inside the ISR.
- **API to UI:** `bool getNextKey(KeyEvent& out)` event-queue pattern, drained from `BadgeUITask::loop()`.
- **`KeyEvent` type:**
  ```cpp
  struct KeyEvent {
    enum Type { CHAR, FUNCTION, NAV, MODIFIER };
    Type type;
    union {
      char chr;        // for CHAR
      uint8_t fn_num;  // 1..5 for F1..F5
      enum NavKey { UP, DOWN, LEFT, RIGHT, ENTER, ESC, BACKSPACE, TAB } nav;
    };
    bool pressed;  // true=press, false=release
  };
  ```
- **Keymap:** re-implemented clean-room from documented physical layout. Two shift keys (positions 30 and 76), 1500 ms modifier timeout.

### 6. UI Architecture — Flat Screens + Router
- **Base class:**
  ```cpp
  class Screen {
  public:
    virtual void onEnter() {}
    virtual void onExit()  {}
    virtual void draw(DisplayDriver& d) = 0;
    virtual bool handleKey(const KeyEvent& k) = 0;  // true = consumed
    virtual void tick() {}
  };
  ```
- **Router:** `BadgeUITask` owns five `Screen*` (chat/nodes/channels/settings/status). F1–F5 swap `_current`. ESC pops a screen-internal modal or returns to chat.
- **Internal modals:** settings screen runs its edit-value flow as a `_mode` enum inside `SettingsScreen` rather than a global nav stack.
- **On-screen F-key labels strip:** the bottom 13 px shows five labels above each physical F-key, so each screen advertises its key-shortcut surface contextually.

### 7. F-Key Assignments
- F1 → Chat
- F2 → Nodes
- F3 → Channels
- F4 → Settings
- F5 → Status / Debug

Each screen may override F-key meanings while a modal is active (labels strip updates to reflect modal context).

### 8. Fonts
- **Bundled:** `ArialMT_Plain_10` (13 px line height) + `ArialMT_Plain_16` (19 px) from MeshCore's `OLEDDisplayFonts.cpp` (MIT, ThingPulse).
- **Usage:** Arial 10 for status bar, F-key labels strip, dense info. Arial 16 for message body, settings rows, compose line.
- **Decision rationale:** Meshtastic uses Arial 16+24 on this badge because they navigate with arrow keys (no labels strip). We need the smaller font for the labels strip. Revisit on hardware if 10pt isn't readable.

### 9. Power Model — Light Sleep on Idle
- Display backlight off after ~30 s idle.
- ESP32-S3 enters **light sleep** when idle; not deep sleep.
- LoRa radio (SX1262) stays in continuous RX mode.
- Wake sources: `KB_INT` GPIO13 (any key press), `LORA_DIO1` GPIO16 (incoming packet).
- Expected average current: ~20–40 mA, ~15–30 h on a typical Li-Po.
- **No battery indicator in v1** (BAT_ADC_PIN routing unverified — probe GPIO4 with real hardware first).

### 10. LoRa Region — Multi-Region
- **All bands compiled in.** Region selectable from settings screen.
- Default: **EU868** (Switzerland).
- One firmware binary works regardless of where the badge is used.

### 11. BLE Companion — In v1
- Inherited from `companion_radio` base. MeshCore phone app pairs to the badge as a normal companion device immediately on first boot.
- Provides phone-app message composition, settings UI redundancy, and history sync — all for free.

### 12. OTA — MeshCore-Native, Un-Gated for Badge
- **Mechanism:** existing `arch/esp32/AsyncElegantOTA/` (WiFi SoftAP + web upload).
- **Trigger:** Settings screen → "Firmware Update" row. Badge brings up a temporary `MeshCore-OTA` SoftAP; user uploads `.bin` via browser.
- **Why now (v1):** rejected the "USB-only in v1" simplification per the guiding principle. MeshCore has the path; we un-gate it for the badge by removing the `ADMIN_PASSWORD && !DISABLE_WIFI_OTA` restriction.

### 13. Settings Screen — Full Edit Surface
On-device editing for **all** applicable `NodePrefs` fields plus badge-specific. Hide only fields whose hardware is absent.

**`NodePrefs` fields exposed on-device:**
- **Identity:** `node_name` (text)
- **LoRa radio:** `freq` (number), `sf` (enum 7..12), `cr` (enum 5..8), `bw` (enum), `tx_power_dbm` (signed int), `rx_boosted_gain` (bool)
- **Mesh:** `airtime_factor` (float), `multi_acks` (number), `manual_add_contacts` (bool), `rx_delay_base` (float), `autoadd_config` (bitmask), `autoadd_max_hops` (number 0..64), `path_hash_mode` (enum), `client_repeat` (bool)
- **Telemetry:** `telemetry_mode_base/loc/env` (each enum DENY/ALLOW_FLAGS/ALLOW_ALL)
- **Location:** `advert_loc_policy` (enum NONE/SHARE)
- **BLE:** `ble_pin` (number)
- **Channels:** `default_scope_name` (text), `default_scope_key` (hex/QR)
- **Hidden (hardware-absent):** `gps_enabled`, `gps_interval`, `buzzer_quiet`

**Badge-specific additions:**
- LoRa region (enum EU868/US915/...)
- Display brightness (0–255, drives backlight PWM on GPIO2)
- Auto-off timeout (seconds)
- Firmware Update (action row → triggers AsyncElegantOTA flow)

**Edit modals:**
- Text → on-screen QWERTY using badge keyboard, with cursor + scroll.
- Number → digit-entry with arrow up/down for increment.
- Enum → vertical picker, arrows to select, Enter to commit.
- Bool → toggle on Enter.
- Action → confirm modal, then execute.

## Hardware Pin Map (canonical)

Reference. Authoritative source is the notes doc.

### Display (NV3007, SPI, HSPI)
| Function | GPIO |
|---|---:|
| Backlight (PWM) | 2 |
| DC | 39 |
| CS | 41 |
| RESET | 40 |
| SCK | 38 |
| MOSI | 21 |
| MISO | — (unused) |

### Keyboard (TCA8418, I2C)
| Function | GPIO / value |
|---|---|
| SDA | 47 |
| SCL | 14 |
| INT (active-low) | 13 |
| I2C address | 0x34 |
| Matrix | 8 rows × 10 cols |

### LoRa (SX1262, SPI)
| Function | GPIO |
|---|---:|
| SCK | 8 |
| MISO | 9 |
| MOSI | 3 |
| CS / NSS | 17 |
| RESET | 18 |
| DIO1 / IRQ | 16 |
| BUSY | 15 |
| DIO2 | (internal — RF switch) |
| DIO3 | (TCXO 1.8 V) |

## Build / PlatformIO Configuration

Variant `platformio.ini` extends `esp32s3_base` per MeshCore convention. Build flags include:

```ini
[env:hackaday_communicator]
extends = esp32s3_base
board = esp32-s3-devkitc-1   ; or custom JSON in boards/
board_build.partitions = default_16MB.csv
build_src_filter =
  ${esp32s3_base.build_src_filter}
  +<../variants/hackaday_communicator>
build_flags = ${esp32s3_base.build_flags}
  -D HACKADAY_COMMUNICATOR
  -D BOARD_HAS_PSRAM
  -D ARDUINO_USB_CDC_ON_BOOT=1
  -I variants/hackaday_communicator
  ; Display
  -D DISPLAY_CLASS=NV3007Display
  -D NV3007_WIDTH=428
  -D NV3007_HEIGHT=142
  -D PIN_TFT_BL=2 -D PIN_TFT_DC=39 -D PIN_TFT_CS=41 -D PIN_TFT_RST=40
  -D PIN_TFT_SCK=38 -D PIN_TFT_MOSI=21
  ; Keyboard
  -D PIN_I2C_SDA=47 -D PIN_I2C_SCL=14 -D PIN_KB_INT=13
  -D TCA8418_KB_ADDR=0x34
  ; LoRa
  -D USE_SX1262
  -D P_LORA_SCLK=8 -D P_LORA_MISO=9 -D P_LORA_MOSI=3
  -D P_LORA_NSS=17 -D P_LORA_RESET=18 -D P_LORA_DIO_1=16 -D P_LORA_BUSY=15
  -D SX126X_DIO2_AS_RF_SWITCH
  -D SX126X_DIO3_TCXO_VOLTAGE=1.8
```

(Final values to be set during M1 scaffolding; this block is illustrative.)

## Milestones

### M1 — Bring-up (highest risk; do this first)
- [ ] Variant `platformio.ini` compiles against `esp32s3_base`.
- [ ] `HackadayBoard.cpp/h` — extends `ESP32Board`, `begin()` initializes GPIOs, returns manufacturer name.
- [ ] `NV3007Display.cpp/h` — direct-SPI driver implementing `DisplayDriver`; 1-bit framebuffer; renders "Hello, Hackaday-Meshcore" at boot.
- [ ] `TCA8418Keyboard.cpp/h` — I2C bring-up, interrupt on KB_INT, event queue functional; key echoes to display.
- [ ] LoRa SX1262 TX + RX smoke test (hardcoded freq, payload), verified by a second SX1262 device or LoRa spectrum analyzer.
- [ ] M1 deliverable: a `.bin` you can flash and physically demonstrate all three subsystems working.

### M2 — Chat MVP
- [ ] `BadgeUITask : public AbstractUITask` with `ChatScreen` only — others stubbed.
- [ ] Status bar with: time, channel name, node count, RSSI.
- [ ] Incoming-message display path: `newMsg(...)` → append to chat ring buffer → redraw.
- [ ] Outgoing-message compose path: full QWERTY input, shift modifier, Enter to send.
- [ ] Single default channel only.
- [ ] BLE companion verified — phone app sees the badge.
- [ ] M2 deliverable: two badges can chat with each other on default channel.

### M3 — Feature Parity
- [ ] All 5 screens implemented: `ChatScreen`, `NodesScreen`, `ChannelsScreen`, `SettingsScreen`, `StatusScreen`.
- [ ] On-screen F-key labels strip implemented (context-aware).
- [ ] Settings screen — full edit surface for all `NodePrefs` (minus GPS/buzzer) + badge-specific (region, brightness, auto-off, firmware update).
- [ ] Edit modals: text, number, enum, bool, action.
- [ ] LoRa region selector wired through to runtime radio config.
- [ ] Display brightness wired to backlight PWM.
- [ ] Auto-off timeout wired to display blank.
- [ ] M3 deliverable: feature-complete badge port. All MeshCore phone-app functionality also reachable on-device.

### M4 — Polish
- [ ] `AsyncElegantOTA` un-gated for badge; "Firmware Update" action row launches SoftAP + web upload.
- [ ] Light sleep on idle implemented; wake on KB_INT + LORA_DIO1.
- [ ] Battery monitoring: probe GPIO4 on real hardware. If divider populated, wire up; otherwise document permanent absence.
- [ ] Power consumption measured and reported in milestone notes.
- [ ] Edge cases: empty contact list, no channel set, OTA failure recovery, etc.
- [ ] Real-world range testing.
- [ ] M4 deliverable: production-ready firmware.

## Immediate Next Steps (Repo Setup)

These are the literal commands/actions to execute when starting M1. **Verify each step before moving on** — destructive on `~/Hackaday/`.

1. **Stash the existing `~/Hackaday/` contents** (the `.devcontainer/`, the notes md, this design doc):
   ```bash
   mkdir -p ~/Hackaday-stash
   mv ~/Hackaday/.devcontainer ~/Hackaday-stash/
   mv ~/Hackaday/hackaday-2025-communicator-badge-meshcore-notes.md ~/Hackaday-stash/
   mv ~/Hackaday/"Hackaday-Meshcore Design.md" ~/Hackaday-stash/
   mv ~/Hackaday/.claude-data ~/Hackaday-stash/ 2>/dev/null || true
   rmdir ~/Hackaday
   ```

2. **Create the fork on GitHub** (web UI or CLI):
   ```bash
   gh repo fork meshcore-dev/MeshCore --org SensorsIot --fork-name Hackaday-Meshcore --clone=false
   ```

3. **Clone the fork to `~/Hackaday/`**:
   ```bash
   gh repo clone SensorsIot/Hackaday-Meshcore ~/Hackaday
   cd ~/Hackaday
   git remote add upstream https://github.com/meshcore-dev/MeshCore.git
   git fetch upstream
   git checkout -b hackaday
   ```

4. **Replace MeshCore's bundled `.devcontainer/` with ours** (ours has PIO + Claude + port 2471):
   ```bash
   rm -rf ~/Hackaday/.devcontainer
   mv ~/Hackaday-stash/.devcontainer ~/Hackaday/
   ```

5. **Move documentation back into the fork**:
   ```bash
   mkdir -p ~/Hackaday/docs/notes
   mv ~/Hackaday-stash/hackaday-2025-communicator-badge-meshcore-notes.md ~/Hackaday/docs/notes/
   mv ~/Hackaday-stash/"Hackaday-Meshcore Design.md" ~/Hackaday/docs/notes/
   mv ~/Hackaday-stash/.claude-data ~/Hackaday/ 2>/dev/null || true
   rmdir ~/Hackaday-stash
   ```

6. **Update `.gitignore`** to keep `.claude-data/` untracked but allow the container config:
   ```bash
   cd ~/Hackaday
   echo ".claude-data/" >> .gitignore
   # devcontainer.json is intentionally tracked (project setup)
   ```

7. **Rebuild the devcontainer** if needed (paths inside are unchanged so this is usually a no-op):
   ```bash
   docker stop hackaday && docker rm hackaday
   cd ~/Hackaday && devcontainer up --workspace-folder .
   docker update --restart unless-stopped hackaday
   ```

8. **First commit on `hackaday` branch**:
   ```bash
   cd ~/Hackaday
   git add .devcontainer/ docs/notes/ .gitignore
   git commit -m "Add devcontainer and badge design docs"
   git push -u origin hackaday
   ```

9. **Begin M1**: scaffold `variants/hackaday_communicator/` (empty `platformio.ini`, stubs for `HackadayBoard`, `NV3007Display`, `TCA8418Keyboard`, `target`). Make it compile against `esp32s3_base` before adding any logic.

## Related

- [[hackaday-2025-communicator-badge-meshcore-notes]] — hardware facts: pin maps, NV3007 init, TCA8418 keymap, SX1262 config
- [[MeshCore upstream]] — https://github.com/meshcore-dev/MeshCore
- [[Meshtastic hackaday-communicator variant]] — https://github.com/meshtastic/firmware/tree/master/variants/esp32s3/hackaday-communicator — used as fact reference only, source never copied (GPL-3.0 vs our MIT)
- [[Hackaday badge hardware repo]] — https://github.com/Hack-a-Day/2025-Communicator_Badge
