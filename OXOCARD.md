# Doom on Oxocard — Porting Notes

This document covers the Oxocard-specific changes layered on top of the
ESP32 PrBoom port.  All new hardware is gated behind Kconfig options so the
original Wrover Kit configuration still builds unchanged.

---

## Quick Start

1. `make menuconfig` — select **Oxocard** under *Hardware to run on*
2. Pick **ST7789V 240×240 LCD** as the LCD type
3. Fill in every GPIO number to match your board (defaults are all 0)
4. `make` and flash

## Build Environment

This project uses ESP-IDF 4.3 (release/v4.3 branch):

```bash
# Set up ESP-IDF environment
export IDF_PATH=/opt/esp-idf
export PATH="$HOME/.espressif/tools/xtensa-esp32-elf/esp-2020r3-8.4.0/xtensa-esp32-elf/bin:$PATH"
export PATH="$HOME/.espressif/python_env/idf4.3_py3.11_env/bin:$PATH"
export IDF_PYTHON_ENV_PATH="$HOME/.espressif/python_env/idf4.3_py3.11_env"

# Build
make -j4

# Flash (adjust port as needed)
python $IDF_PATH/components/esptool_py/esptool/esptool.py \
  --chip esp32 --port /dev/cu.wchusbserial210 --baud 115200 \
  --before default_reset --after hard_reset write_flash -z \
  --flash_mode dio --flash_freq 40m --flash_size 8MB \
  0x1000 build/bootloader/bootloader.bin \
  0x8000 build/partitions.bin \
  0x10000 build/esp32-doom.bin \
  0x100000 doom1-cut.wad
```

Toolchain: xtensa-esp32-elf-gcc 8.4.0 (esp-2020r3)

---

## What Changed

### Configuration (`Kconfig.projbuild`)

| New option | Purpose |
|---|---|
| `HW_OXOCARD` | Board selection; enables the sections below |
| `HW_LCD_TYPE_ST7789V` | Selects the ST7789V init sequence and 240×240 geometry |
| `HW_LCD_XOFFSET` / `HW_LCD_YOFFSET` | Panel address offset (set to 0 for ST7789V 240×240) |
| `HW_OXOCARD_INPUT` | Enables 5-button GPIO input; auto-enabled when `HW_OXOCARD` is selected |
| `HW_OXOCARD_BTN_*_GPIO` | One entry per button (Forward, Back, Turn Left, Turn Right, Shoot) |
| `HW_OXOCARD_BUZZER_GPIO` | GPIO for external piezo buzzer (not populated on Science board) |

A hidden `HW_USER_PINS` bool ties `HW_OXOCARD` and the existing `HW_CUSTOM`
together so LCD-pin menu entries appear for both without duplicating them.

### Display (`doomdef.h`, `i_video.c`, `spi_lcd.c`)

* `SCREENWIDTH` / `SCREENHEIGHT` set to **240**.  The raycaster, status bar,
  and all screen allocations scale automatically.
* `spi_lcd.c` defines `LCD_WIDTH`, `LCD_HEIGHT`, `LCD_XOFFSET`, `LCD_YOFFSET`
  from the controller type.  All previously hardcoded `320*240` values now use
  these macros, so ILI9341 builds are unaffected.
* The ST7789V init-command sequence (gated by `CONFIG_HW_LCD_TYPE == 1`) is used.
  The MADCTL byte is `0xA0` (MY + MX + MV).  Color inversion is enabled via `INVON`.
* Framebuffer allocation changed from internal RAM to SPIRAM (`MALLOC_CAP_SPIRAM`)
  as 240×240 = 57,600 bytes exceeds available internal RAM.

### Buttons (`oxobuttons.c`, `oxobuttons.h`, `gamepad.c`)

Five GPIO buttons with **simple direct mapping**:

| Physical button | Doom action |
|---|---|
| UP | Move forward (`key_up`) |
| DOWN | Move backward (`key_down`) |
| LEFT | Turn left (`key_left`) |
| RIGHT | Turn right (`key_right`) |
| MIDDLE | Fire weapon (`key_fire`) |

* `oxobuttons.c` reads each pin via `gpio_get_level` and applies a
  **two-poll debounce** (20–40 ms at the 50 Hz polling rate).  Buttons are
  active-low (internal pull-up).  GPIO 36–39 on ESP32 have no internal
  pull-up — wire an external one if you use those pins.
* `gamepad.c` tracks which Doom key is currently posted per button and
  generates `ev_keydown` / `ev_keyup` events via `D_PostEvent`.
* The entire Oxocard path is compiled only when `CONFIG_HW_OXOCARD_INPUT` is
  set; otherwise the original PSX-controller code compiles unchanged.

### Audio (`i_sound.c`)

The Oxocard Science **has no speaker or buzzer**.  GPIO 25/26 are I2S
microphone pins (MIC_CLK / MIC_DATA), not audio output — confirmed via the
CircuitPython board definition (`pins.c`) and hardware testing.  The
`AUDIO_P` / `AUDIO_N` / `AUDIO_SD` labels in earlier versions of this
document were incorrect.

All sound functions in `i_sound.c` are stubs.  Sound and music are disabled
via `nosfxparm=true` / `nomusicparm=true` in `d_main.c`.

To add audio, wire an external piezo buzzer to a free GPIO, set
`CONFIG_HW_OXOCARD_BUZZER_GPIO` in menuconfig, and implement LEDC tone
generation in the `#ifdef CONFIG_HW_OXOCARD` section of `i_sound.c`.

---

## Oxocard Science 1.5 — GPIO Map

Pin assignments sourced from the CircuitPython board definition (PR adafruit/circuitpython#8600)
and verified against the oxocard-hardware schematics.

### Display (SPI)

| Signal | GPIO | Notes |
|---|---|---|
| MOSI | 13 | SPI data out |
| CLK | 14 | SPI clock |
| CS | 15 | Chip select (active low) |
| DC | 27 | Data / Command select |
| RST | 4 | Panel reset (active low) |
| BL | 19 | Backlight enable |

### Buttons

The Science 1.5 has **5 physical buttons** (4 directional + 1 center), mapped directly
to Doom movement controls:

| Kconfig entry | GPIO | Physical button | Polarity | Doom action |
|---|---|---|---|---|
| `BTN_FWD` | 36 | UP | Active-HIGH | Move forward |
| `BTN_BACK` | 37 | DOWN | Active-HIGH | Move backward |
| `BTN_TURNL` | 38 | LEFT | Active-HIGH | Turn left |
| `BTN_TURNR` | 39 | RIGHT | Active-HIGH | Turn right |
| `BTN_SHOOT` | 0 | MIDDLE (Boot button) | Active-LOW | Fire weapon |

**IMPORTANT:** GPIO 0 (Boot button) is **active-LOW** (pressed = level 0), while GPIOs 36-39
are **active-HIGH** (pressed = level 1). The `oxobuttons_poll()` function in `oxobuttons.c`
handles this mixed polarity automatically by checking the GPIO number.

GPIO 36–39 are input-only on ESP32 with no internal pull-up; the Oxocard
Science has external pull-ups on these lines on-board.

### Microphone (I2S) — NOT speaker output

| Signal | GPIO | Notes |
|---|---|---|
| MIC_CLK | 25 | I2S clock for on-board MEMS microphone |
| MIC_DATA | 26 | I2S data from on-board MEMS microphone |

**The Oxocard Science has no speaker, buzzer, or audio amplifier.**
GPIO 25/26 are microphone inputs only.  GPIO 20 is the VOC sensor reset,
not an audio shutdown pin.  To add sound, wire an external piezo buzzer
to one of the free GPIOs and use LEDC PWM.

### macOS serial port

The Oxocard Science uses a **CH340** USB-UART chip (vendor 0x1a86).
macOS does **not** ship a CH340 driver.  Install the WCH driver before
`make flash`:

1. Download from <https://www.wch.cn/downloads/CH341SER_MAC.zip>
2. Mount the `.dmg`, run the `.pkg` installer
3. Reboot (required by the kext)
4. The device will appear as `/dev/cu.wchusb*`
5. Update `CONFIG_ESPTOOLPY_PORT` in `sdkconfig` to match

---

## Tuning After First Flash

* **Image orientation** — wrong mirror/rotation → change the MADCTL byte in
  `spi_lcd.c` (the `0x36` command's data byte in the ST7735 init array).
* **Image offset** — shifted by 1–2 px → set `HW_LCD_XOFFSET` /
  `HW_LCD_YOFFSET` in menuconfig.
* **Button order** — if the physical d-pad doesn't match Forward/Back/Left/Right,
  swap the GPIO numbers for `BTN_FWD` / `BTN_BACK` / `BTN_TURNL` / `BTN_TURNR`
  in `sdkconfig` and rebuild.
* **Audio** — the Science board has no speaker.  Wire an external piezo to a
  free GPIO and update `CONFIG_HW_OXOCARD_BUZZER_GPIO` in menuconfig.

---

## Current Status (2026-02-06)

### Working Features

- **Display**: 240×240 ST7789V LCD working correctly
- **Button Input**: All 5 GPIO buttons functional with proper debouncing
- **Mixed GPIO Polarity**: Correctly handles GPIO 0 (active-LOW) and GPIOs 36-39 (active-HIGH)
- **Keydown/Keyup Matching**: MIDDLE button properly tracks which key was posted on keydown
  to ensure matching keyup events, even if `menuactive` changes between press and release
- **Menu Navigation**: All directional buttons and MIDDLE button (menu_enter) work in menus
- **Gameplay**: Movement (forward/back), turning (left/right), and shooting (MIDDLE) all functional

### Implementation Details

**Button Polarity Handling** (oxobuttons.c:43-61):
```c
void oxobuttons_poll(void)
{
    for (int i = 0; i < OXO_NUM_BTNS; i++) {
        int raw = gpio_get_level(btn_gpio[i]);
        if (raw == state[i].last_raw) {
            // GPIO 0 (Boot button) is active-LOW: pressed = 0
            // GPIOs 36-39 are active-HIGH: pressed = 1
            if (btn_gpio[i] == 0) {
                state[i].confirmed = (raw == 0) ? 1 : 0;
            } else {
                state[i].confirmed = (raw == 1) ? 1 : 0;
            }
        }
        state[i].last_raw = raw;
    }
}
```

**Context-Aware MIDDLE Button** (gamepad.c:68-91):
- Tracks which key was posted on keydown (`middle_button_last_key`)
- Posts `key_menu_enter` in menus, `key_fire` in gameplay
- Always posts the same key on keyup as was posted on keydown
- Prevents stuck keys if player exits menu while holding MIDDLE button

### Fixes Applied

1. **Mixed Polarity Support**: Added GPIO-specific polarity handling in `oxobuttons_poll()`
   to correctly interpret both active-LOW (GPIO 0) and active-HIGH (GPIOs 36-39) buttons

2. **Keydown/Keyup Matching**: Added `middle_button_last_key` tracking to ensure the MIDDLE
   button always posts matching keydown/keyup pairs, even when `menuactive` changes

3. **Removed Debug Logging**: Cleaned up all `lprintf()` debug statements from `gamepad.c`

### Pending Work

- **Audio**: No on-board speaker — requires external piezo buzzer on a free GPIO
- **Display Optimization**: Fine-tune ST7789V colors and timing if needed
