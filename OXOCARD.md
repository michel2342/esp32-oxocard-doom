# Doom on Oxocard — Porting Notes

This document covers the Oxocard-specific changes layered on top of the
ESP32 PrBoom port.  All new hardware is gated behind Kconfig options so the
original Wrover Kit configuration still builds unchanged.

---

## Quick Start

1. `make menuconfig` — select **Oxocard** under *Hardware to run on*
2. Pick **ST7735 LCD** as the LCD type
3. Fill in every GPIO number to match your board (defaults are all 0)
4. `make` and flash

---

## What Changed

### Configuration (`Kconfig.projbuild`)

| New option | Purpose |
|---|---|
| `HW_OXOCARD` | Board selection; enables the sections below |
| `HW_LCD_TYPE_ST7735` | Selects the ST7735 init sequence and 128×128 geometry |
| `HW_LCD_XOFFSET` / `HW_LCD_YOFFSET` | Panel address offset (some 128×128 ST7735 modules start at column 2 / row 1) |
| `HW_OXOCARD_INPUT` | Enables 6-button GPIO input; auto-enabled when `HW_OXOCARD` is selected |
| `HW_OXOCARD_BTN_*_GPIO` | One entry per button (Forward, Back, Turn Left, Turn Right, Shoot, Strafe) |
| `HW_OXOCARD_BUZZER_GPIO` | GPIO driving the piezo buzzer |

A hidden `HW_USER_PINS` bool ties `HW_OXOCARD` and the existing `HW_CUSTOM`
together so LCD-pin menu entries appear for both without duplicating them.

### Display (`doomdef.h`, `i_video.c`, `spi_lcd.c`)

* `SCREENWIDTH` / `SCREENHEIGHT` set to **128**.  The raycaster, status bar,
  and all screen allocations scale automatically.
* `spi_lcd.c` defines `LCD_WIDTH`, `LCD_HEIGHT`, `LCD_XOFFSET`, `LCD_YOFFSET`
  from the controller type.  All previously hardcoded `320*240` values now use
  these macros, so ILI9341 and ST7789V builds are unaffected.
* A full ST7735 init-command sequence is added (gated by `CONFIG_HW_LCD_TYPE == 2`).
  The MADCTL byte is `0x48` (MX + BGR).  If the image is mirrored or rotated
  change it in `spi_lcd.c` — common alternatives are `0xC8`, `0x08`, `0xC0`.
* `MEM_PER_TRANS` is raised to 4096 words for ST7735 so that 128×128 = 16 384
  pixels divides into exactly 4 DMA chunks with no leftover.

### Buttons (`oxobuttons.c`, `oxobuttons.h`, `gamepad.c`)

Six GPIO buttons with a **strafe-toggle** scheme:

| Physical button | Strafe released | Strafe held |
|---|---|---|
| Forward | Move forward | Move forward |
| Back | Move back | Move back |
| Turn Left | Turn left | **Strafe left** |
| Turn Right | Turn right | **Strafe right** |
| Shoot | Fire weapon | Fire weapon |
| Strafe | *(modifier)* | *(modifier)* |

* `oxobuttons.c` reads each pin via `gpio_get_level` and applies a
  **two-poll debounce** (20–40 ms at the 50 Hz polling rate).  Buttons are
  active-low (internal pull-up).  GPIO 36–39 on ESP32 have no internal
  pull-up — wire an external one if you use those pins.
* `gamepad.c` tracks *which Doom key is currently posted* per button so that
  toggling the Strafe modifier while a turn button is already held cleanly
  releases the old key and presses the new one in the same tick.
* The entire Oxocard path is compiled only when `CONFIG_HW_OXOCARD_INPUT` is
  set; otherwise the original PSX-controller code compiles unchanged.

### Audio (`i_sound.c`)

A single piezo buzzer cannot mix audio, so the implementation maps the most
audible Doom sound effects to short tones:

| Sound | Frequency | Duration | Event |
|---|---|---|---|
| `pistol` | 800 Hz | 80 ms | Pistol shot |
| `shotgn` | 300 Hz | 120 ms | Shotgun |
| `dshtgn` | 250 Hz | 140 ms | Double shotgun |
| `chgun` | 700 Hz | 60 ms | Chaingun |
| `rlaunc` | 450 Hz | 150 ms | Rocket launcher |
| `plasma` | 600 Hz | 100 ms | Plasma gun |
| `bfg` | 200 Hz | 250 ms | BFG |
| `sawhit` | 500 Hz | 60 ms | Chainsaw hit |
| `punch` | 200 Hz | 80 ms | Fist punch |
| `itemup` | 1200 Hz | 100 ms | Item pickup |
| `wpnup` | 1000 Hz | 80 ms | Weapon pickup |
| `getpow` | 1500 Hz | 150 ms | Power-up |
| `doropn` | 220 Hz | 300 ms | Door open |
| `dorcls` | 180 Hz | 200 ms | Door close |
| `plpain` | 150 Hz | 100 ms | Player hit |
| `swtchn` | 500 Hz | 40 ms | Switch on |
| `telept` | 900 Hz | 120 ms | Teleport |
| `barexp` | 180 Hz | 200 ms | Barrel explosion |

A new tone preempts any currently playing tone (no mixing).  Tone generation
uses the ESP32 **LEDC** peripheral (hardware PWM); an `esp_timer` one-shot
callback silences the buzzer after the mapped duration.

`I_GetSfxLumpNum` returns the `sfxenum_t` index so that `I_StartSound` can
index the tone table directly — no string matching at runtime.

Music stubs remain empty; a buzzer cannot play continuous music.

---

## Tuning After First Flash

* **Image orientation** — wrong mirror/rotation → change the MADCTL byte in
  `spi_lcd.c` (the `0x36` command's data byte in the ST7735 init array).
* **Image offset** — shifted by 1–2 px → set `HW_LCD_XOFFSET` /
  `HW_LCD_YOFFSET` in menuconfig.
* **Buzzer volume / tone quality** — adjust frequencies and durations in the
  `tone_table[]` in `i_sound.c`.  Piezo buzzers vary significantly between
  units; some respond better to 500–1000 Hz than to very low or very high
  frequencies.
