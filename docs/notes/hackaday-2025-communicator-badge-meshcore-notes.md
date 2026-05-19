# Hackaday 2025 Communicator Badge — Display, Keyboard, and MeshCore Port Notes

## Purpose

This document summarizes the known hardware and firmware interface details for the Hackaday 2025 Communicator Badge, with a focus on implementing or porting MeshCore to the badge.

The information is based on the public Hackaday badge repository and the Meshtastic firmware port for the badge. The goal is to avoid reverse engineering where the Meshtastic implementation already provides exact answers.

## Relevant repositories

- Hackaday badge repository: <https://github.com/Hack-a-Day/2025-Communicator_Badge>
- Meshtastic firmware repository: <https://github.com/meshtastic/firmware>
- Meshtastic Hackaday Communicator variant:
  - `variants/esp32s3/hackaday-communicator/variant.h`
  - `variants/esp32s3/hackaday-communicator/platformio.ini`
- Meshtastic display implementation:
  - `src/graphics/TFTDisplay.cpp`
- Meshtastic keyboard implementation:
  - `src/input/HackadayCommunicatorKeyboard.cpp`
  - `src/input/kbI2cBase.cpp`
  - `src/input/cardKbI2cImpl.cpp`
  - `src/input/InputBroker.h`

## Meshtastic board target

The Meshtastic board target is:

```text
hackaday-communicator
```

The PlatformIO variant defines:

```cpp
-D HACKADAY_COMMUNICATOR
-D BOARD_HAS_PSRAM
```

The target uses the ESP32-S3 base environment and a 16 MB partition table.

## High-level hardware summary

| Subsystem | Confirmed implementation |
|---|---|
| MCU | ESP32-S3 |
| PSRAM | Enabled in Meshtastic target |
| LoRa radio | SX1262 |
| Display controller | NV3007 |
| Display bus | SPI, write-only |
| Display resolution used by Meshtastic | 428 × 142 pixels |
| Keyboard controller | TCA8418 I2C keyboard controller |
| Keyboard matrix | 8 rows × 10 columns |
| Keyboard interrupt | GPIO13 |
| I2C pins | SDA GPIO47, SCL GPIO14 |

## Display implementation

### Display type

The Meshtastic port uses the TFT display path, not an LVGL application stack.

The variant enables:

```cpp
#define HAS_SCREEN 1
#define USE_TFTDISPLAY 1
```

The display size exposed to the Meshtastic UI is:

```cpp
#define TFT_WIDTH  428
#define TFT_HEIGHT 142
```

Therefore, MeshCore can treat the badge display as a 428 × 142 pixel display.

### Display controller

The controller class used in the Meshtastic display implementation is:

```cpp
Arduino_NV3007
```

This confirms that the badge display controller is NV3007.

### Display pins

| Function | GPIO |
|---|---:|
| Backlight | 2 |
| Data/Command | 39 |
| Chip Select | 41 |
| Reset | 40 |
| SPI SCK | 38 |
| SPI MOSI | 21 |
| SPI MISO | Not used |
| SPI host | HSPI |

The display bus is initialized in Meshtastic as:

```cpp
bus = new Arduino_ESP32SPI(
    TFT_DC,          // GPIO 39
    TFT_CS,          // GPIO 41
    38,              // SCK
    21,              // MOSI
    GFX_NOT_DEFINED, // no MISO
    HSPI
);
```

The panel is initialized as:

```cpp
tft = new Arduino_NV3007(
    bus,
    40,        // reset pin
    0,         // rotation
    false,     // IPS flag
    142,       // width argument to driver
    428,       // height argument to driver
    12, 0,     // col/row offset 1
    14, 0,     // col/row offset 2
    nv3007_279_init_operations,
    sizeof(nv3007_279_init_operations)
);
```

### Display orientation and geometry

The display is physically and practically used as a wide, low-height communicator display.

Meshtastic exposes it as:

```text
428 pixels wide
142 pixels high
```

The underlying driver constructor receives `142, 428`, with rotation and offsets applied by the display driver. For a MeshCore UI, the practical target canvas should be considered 428 × 142.

### Rendering model used by Meshtastic

Meshtastic does not render a full RGB GUI framebuffer for this target. It uses the normal Meshtastic screen rendering model, originally developed around monochrome OLED displays.

The internal screen buffer is effectively 1-bit. The TFT driver maps pixels to two RGB565 colors:

- foreground: `TFT_MESH`
- background: `TFT_BLACK`

The variant defines:

```cpp
#define TFT_BLACK 0
```

`TFT_MESH` defaults in `TFTDisplay.cpp` to:

```cpp
COLOR565(0x67, 0xEA, 0x94)
```

Meshtastic converts these colors to big-endian 16-bit form:

```cpp
colorTftMesh  = __builtin_bswap16(TFT_MESH);
colorTftBlack = __builtin_bswap16(TFT_BLACK);
```

For this badge, updated display areas are sent using:

```cpp
tft->draw16bitBeRGBBitmap(...)
```

rather than the usual `pushRect(...)` path.

### Practical display conclusion for MeshCore

For MeshCore, there are two realistic UI options:

1. **Efficient monochrome-style UI**
   - Use a 1-bit or low-color framebuffer.
   - Map pixels to foreground/background RGB565 colors.
   - This matches the Meshtastic approach and is memory-efficient.
   - It is sufficient for chat, node list, status, menus, and settings.

2. **Full RGB565 UI**
   - Use the NV3007 as a normal color TFT.
   - Requires more memory bandwidth and a different rendering path.
   - Potentially useful for richer icons or graphical screens, but not necessary for a first MeshCore port.

For a first MeshCore port, the efficient monochrome-style rendering model is recommended.

## What Meshtastic displays on the badge

Because the badge enables `HAS_SCREEN` and `USE_TFTDISPLAY`, it uses the normal Meshtastic screen UI stack.

Relevant Meshtastic renderers include:

```cpp
ClockRenderer
DebugRenderer
MenuHandler
MessageRenderer
NodeListRenderer
NotificationRenderer
UIRenderer
```

The display can show the usual Meshtastic screens and overlays.

### Confirmed display content categories

| Screen / UI area | Displayed information |
|---|---|
| Header/status area | Battery, time, mail/message indicator, and device status elements through the common header renderer |
| GPS status | `No GPS`, `GPS off`, `No Lock`, `No Sats`, or satellite count depending on GPS state |
| Coordinates | Lat/Lon, UTM, MGRS, Open Location Code, OSGR, Maidenhead, or DMS depending on configuration |
| Node count | Online and total node count |
| Favorite node / node screen | Node short name, long name, signal quality, hop count, last heard, uptime, and distance where available |
| Message screens | Message display and text input through the Meshtastic message renderer and text input overlay |
| Notifications | Banners, selection prompts, node picker, number picker, and other overlay states |
| Menus | Meshtastic menu system through `MenuHandler` |
| Debug screen | Diagnostic/debug information through `DebugRenderer` |
| Clock screen | Clock rendering through `ClockRenderer` |

### GPS note

The badge variant defines:

```cpp
#define GPS_DEFAULT_NOT_PRESENT 1
```

So by default the badge is treated as not having a GPS receiver present. Unless GPS is added or configured differently, the UI should not be expected to show live GPS coordinates.

For MeshCore, this means the display should primarily be used for:

- chat/messages
- node list
- signal/link status
- channel or group information
- battery/status indicators
- settings/menu navigation
- text composition feedback

## Keyboard implementation

### Keyboard controller

The keyboard is not scanned directly by ESP32-S3 GPIOs in the Meshtastic port.

It uses a TCA8418 I2C keyboard controller.

The Meshtastic configuration defines the TCA8418 keyboard address as:

```cpp
#define TCA8418_KB_ADDR 0x34
```

The I2C pins are:

```cpp
#define I2C_SDA 47
#define I2C_SCL 14
```

The keyboard interrupt pin is:

```cpp
#define KB_INT 13
```

The variant initializes the interrupt pin as:

```cpp
pinMode(KB_INT, INPUT);
```

### Keyboard matrix size

The Hackaday keyboard implementation defines:

```cpp
#define _TCA8418_COLS 10
#define _TCA8418_ROWS 8
#define _TCA8418_NUM_KEYS 80
```

So the physical keyboard matrix is:

```text
8 rows × 10 columns = 80 possible key positions
```

### Keyboard event handling

The TCA8418 event FIFO is read from:

```cpp
TCA8418_REG_KEY_EVENT_A + i
```

Pressed events have bit `0x80` set. The low 7 bits contain the key number.

Meshtastic converts the TCA8418 key number to row and column using:

```cpp
row = (key - 1) / 10;
col = (key - 1) % 10;
next_key = row * 10 + col;
```

This gives a zero-based key index used by the Hackaday Communicator keyboard mapping.

### Keyboard class used by Meshtastic

When `HACKADAY_COMMUNICATOR` is defined, Meshtastic instantiates:

```cpp
HackadayCommunicatorKeyboard
```

This is implemented in:

```text
src/input/HackadayCommunicatorKeyboard.cpp
```

### Key map overview

The keyboard is mapped as a QWERTY-style text keyboard with navigation and function keys.

The top/function row includes:

```text
FUNCTION_F1
+
9
8
7
FUNCTION_F2
FUNCTION_F3
FUNCTION_F4
FUNCTION_F5
ESC
```

Letter rows include normal QWERTY characters:

```text
q w e r t y u i o
a s d f g h j k l
z x c v b n m , .
```

Other mapped keys include:

```text
TAB
BACKSPACE
ENTER / SELECT
ESC
UP
DOWN
LEFT
RIGHT
space
0–9
+ - * / = [ ] \ ' ; , .
```

Shifted mappings are implemented for many keys, including:

```text
q / Q
w / W
e / E
...
1 / !
2 / @
3 / #
4 / $
5 / %
6 / ^
[ / {
] / }
, / <
. / >
' / "
; / :
/ / ?
```

### Function keys

The badge has function-key mappings. Meshtastic maps them to input broker events:

```cpp
INPUT_BROKER_FN_F1 = 0xf1
INPUT_BROKER_FN_F2 = 0xf2
INPUT_BROKER_FN_F3 = 0xf3
INPUT_BROKER_FN_F4 = 0xf4
INPUT_BROKER_FN_F5 = 0xf5
```

For MeshCore these can be used as direct UI shortcuts, for example:

| Function key | Possible MeshCore use |
|---|---|
| F1 | Main chat screen |
| F2 | Node list |
| F3 | Channels / rooms |
| F4 | Settings |
| F5 | System/status/debug |

### Shift/modifier behavior

Two shift keys are implemented:

```cpp
modifierRightShiftKey = 30;
modifierLeftShiftKey  = 76;
```

Both set the same shift flag:

```cpp
modifierRightShift = 0b0001;
modifierLeftShift  = 0b0001;
```

The modifier timeout is:

```cpp
1500 ms
```

The implementation names this value `_TCA8418_MULTI_TAP_THRESHOLD`, but the keyboard also uses it as the shift/modifier timeout.

### Practical keyboard conclusion for MeshCore

MeshCore does not need to implement direct GPIO keyboard matrix scanning.

A MeshCore port should implement:

```text
I2C bus on SDA GPIO47 / SCL GPIO14
TCA8418 at address 0x34
Optional interrupt input on GPIO13
8 × 10 matrix mapping
QWERTY key translation table
Navigation key events
Function key events F1–F5
Shift/modifier handling
```

The Meshtastic `HackadayCommunicatorKeyboard.cpp` file is the best reference for the exact key map.

## LoRa radio implementation

The badge uses an SX1262 LoRa radio.

The Meshtastic variant defines these LoRa pins:

| SX1262 signal | GPIO |
|---|---:|
| SCK | 8 |
| MISO | 9 |
| MOSI | 3 |
| CS / NSS | 17 |
| RESET | 18 |
| DIO1 / IRQ | 16 |
| BUSY | 15 |

Additional radio configuration:

```cpp
#define USE_SX1262
#define SX126X_CS 17
#define SX126X_DIO1 16
#define SX126X_BUSY 15
#define SX126X_RESET 18
#define SX126X_DIO2_AS_RF_SWITCH
#define SX126X_DIO3_TCXO_VOLTAGE 1.8
```

For MeshCore this means the radio side is conventional SX1262 integration with:

- DIO1 interrupt
- BUSY line handling
- reset line
- DIO2 as RF switch
- DIO3 supplying 1.8 V TCXO control

## Suggested MeshCore hardware abstraction

A clean MeshCore board support package could be structured as follows:

```text
boards/hackaday_communicator/
  board.h
  display_nv3007.cpp
  display_nv3007.h
  keyboard_tca8418.cpp
  keyboard_tca8418.h
  radio_sx1262.cpp
  radio_sx1262.h
  input_map.h
  power.cpp
  power.h
```

### Board constants

```cpp
// Display
#define BADGE_TFT_WIDTH   428
#define BADGE_TFT_HEIGHT  142
#define BADGE_TFT_BL      2
#define BADGE_TFT_DC      39
#define BADGE_TFT_CS      41
#define BADGE_TFT_RST     40
#define BADGE_TFT_SCK     38
#define BADGE_TFT_MOSI    21

// Keyboard
#define BADGE_I2C_SDA     47
#define BADGE_I2C_SCL     14
#define BADGE_KB_INT      13
#define BADGE_TCA8418_ADDR 0x34
#define BADGE_KB_ROWS     8
#define BADGE_KB_COLS     10

// LoRa SX1262
#define BADGE_LORA_SCK    8
#define BADGE_LORA_MISO   9
#define BADGE_LORA_MOSI   3
#define BADGE_LORA_CS     17
#define BADGE_LORA_RST    18
#define BADGE_LORA_DIO1   16
#define BADGE_LORA_BUSY   15
```

## Recommended first MeshCore UI

The display is wide and short. It is well suited to a compact text communicator UI.

A first usable MeshCore screen layout could be:

```text
+----------------------------------------------------+
| MeshCore  Ch:Main  Nodes:12  Batt:82%  RSSI:-94    |
+----------------------------------------------------+
| HB9BLA: Test message from the badge                |
| Node23: Ack. SNR 7.5 dB                            |
| Andreas: This keyboard is usable                   |
|                                                    |
+----------------------------------------------------+
| > compose message here_                            |
+----------------------------------------------------+
```

The 428 × 142 display should comfortably support:

- one compact status bar
- three to five message lines depending on font size
- one input line
- simple scroll indicators
- small icons if desired

## Recommended key behavior for MeshCore

| Key | Recommended action |
|---|---|
| Arrow Up / Down | Move selection or scroll messages |
| Arrow Left / Right | Change field, page, or tab |
| Enter / Select | Open selected item / send message when in compose mode |
| Backspace | Delete character |
| Tab | Switch focus between message list and input field |
| Esc | Back / cancel / close overlay |
| F1 | Chat |
| F2 | Nodes |
| F3 | Channels |
| F4 | Settings |
| F5 | Status/debug |
| Shift | Uppercase and shifted symbols |

## Main implementation risks

### Low risk

- Keyboard controller is known: TCA8418.
- Keyboard matrix size is known: 8 × 10.
- Key map exists in Meshtastic.
- Radio pins and SX1262 configuration are known.
- Display controller and pins are known.

### Medium risk

- NV3007 initialization sequence must be reused correctly.
- Display offsets and rotation must match the physical display.
- Efficient partial redraw should be used to avoid flicker and unnecessary SPI traffic.
- Keyboard modifier behavior should be tested for usability.

### High-value reuse from Meshtastic

For the fastest MeshCore port, reuse these parts conceptually:

1. NV3007 display initialization and offset values from `TFTDisplay.cpp`.
2. 1-bit-to-RGB565 rendering approach from Meshtastic.
3. TCA8418 event reading from `HackadayCommunicatorKeyboard.cpp`.
4. Exact keyboard key map from `HackadayCommunicatorKeyboard.cpp`.
5. SX1262 pin configuration from the board variant.

## Porting checklist

### Display

- [ ] Bring up SPI display bus on HSPI-equivalent peripheral.
- [ ] Use SCK GPIO38 and MOSI GPIO21.
- [ ] Use CS GPIO41, DC GPIO39, reset GPIO40.
- [ ] Enable backlight on GPIO2.
- [ ] Reuse NV3007 initialization operations.
- [ ] Confirm visible canvas is 428 × 142.
- [ ] Implement monochrome-style framebuffer or direct text renderer.
- [ ] Test text baseline, clipping, and rotation.

### Keyboard

- [ ] Bring up I2C on SDA GPIO47 and SCL GPIO14.
- [ ] Detect TCA8418 at address 0x34.
- [ ] Configure 8 rows × 10 columns.
- [ ] Use GPIO13 as interrupt input or poll the event FIFO.
- [ ] Decode key press/release events.
- [ ] Apply Hackaday key map.
- [ ] Implement shift timeout.
- [ ] Map arrows, Enter, Backspace, Esc, Tab, and F1–F5 to MeshCore UI actions.

### Radio

- [ ] Bring up SX1262 SPI using SCK GPIO8, MISO GPIO9, MOSI GPIO3.
- [ ] Use CS GPIO17.
- [ ] Use reset GPIO18.
- [ ] Use DIO1 GPIO16 as IRQ.
- [ ] Use BUSY GPIO15.
- [ ] Configure DIO2 as RF switch.
- [ ] Configure DIO3 TCXO voltage to 1.8 V.
- [ ] Verify transmit and receive timing.

### UI

- [ ] Define compact status bar.
- [ ] Implement message list screen.
- [ ] Implement text compose line.
- [ ] Implement node list.
- [ ] Implement settings/menu navigation.
- [ ] Add notification/banner overlay.
- [ ] Add debug/status page.

## Conclusion

The Hackaday 2025 Communicator Badge is a strong MeshCore target because the critical hardware details are already exposed by the Meshtastic port.

The confirmed essentials are:

```text
Display:  NV3007, SPI, 428 × 142, GPIO BL=2 DC=39 CS=41 RST=40 SCK=38 MOSI=21
Keyboard: TCA8418, I2C address 0x34, SDA=47 SCL=14, INT=13, 8 × 10 matrix
Radio:    SX1262, SCK=8 MISO=9 MOSI=3 CS=17 RESET=18 DIO1=16 BUSY=15
```

A practical first MeshCore port should use a monochrome-style UI on the color TFT, with a compact chat/message interface and direct use of the hardware keyboard for text entry.
