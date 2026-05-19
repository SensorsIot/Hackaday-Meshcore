"""Pre-build hook for the hackaday_communicator env.

Arduino_GFX 1.6.1's `Arduino_ESP32RGBPanel.cpp` references
`esp_rgb_panel_t`, an internal IDF type that was renamed between
platform-espressif32 6.x releases. The badge uses SPI, not the RGB
panel interface, so this file is dead code on this target — we just
neuter it so the build no longer touches the missing type.

Idempotent. Runs after lib install, before compile.
"""

Import("env")  # noqa: F821  (provided by PlatformIO build engine)

import os

def _neuter(path):
    if not os.path.exists(path):
        return False
    with open(path, "rb") as f:
        head = f.read(64)
    if head.startswith(b"// neutered"):
        return False
    with open(path, "w") as f:
        f.write("// neutered by variants/hackaday_communicator/patch_arduino_gfx.py\n")
    return True

target = os.path.join(
    env["PROJECT_DIR"],
    ".pio", "libdeps", env["PIOENV"],
    "GFX Library for Arduino", "src", "databus",
    "Arduino_ESP32RGBPanel.cpp",
)
if _neuter(target):
    print("[patch_arduino_gfx] neutered " + target)
