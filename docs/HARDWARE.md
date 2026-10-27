# Hardware notes

Target: Waveshare **ESP32-S3-RLCD-4.2** ([product page](https://www.waveshare.com/esp32-s3-rlcd-4.2.htm)),
4.2" reflective monochrome LCD (ST7305), no backlight, 8 MB octal PSRAM, microSD slot, PCF85063 RTC,
SHTC3 sensor, ES8311/ES7210 audio, 18650 battery holder with ADC monitor, USB-C for power, programming
and charging. SoC: ESP32-S3 (Xtensa LX7 dual-core 240 MHz, Wi-Fi + BLE 5, USB OTG full-speed).

## Pin map

From the [ESPHome device page](https://devices.esphome.io/devices/waveshare-esp32-s3-rlcd-42/);
mirrored in `components/board/include/board.h`.

| GPIO | Function |
|---|---|
| 0 | BOOT button (active low) |
| 4 | Battery ADC (ADC1_CH3) |
| 5 | LCD DC (A0) |
| 8 / 9 / 10 / 16 / 45 | I2S DOUT / BCLK / DIN / MCLK / LRCLK |
| 11 | LCD SPI CLK |
| 12 | LCD SPI MOSI |
| 13 / 14 | I2C SDA / SCL |
| 18 | KEY button (active low) |
| 40 | LCD CS |
| 41 | LCD RESET |
| 46 | Speaker amp enable |
| 19 / 20 | Native USB D- / D+ (ESP32-S3 fixed pins; host PHY for the keyboard) |
| 43 / 44 | UART0 TX / RX (log console when USB is a host) |

**Buttons, verified on the real board (2026-10-27):** three tactile buttons sit in a row on the board
edge, unlabelled in a photo. The **rightmost is BOOT**. Holding it while plugging in USB makes the board
enumerate as `303a:1001` (`/dev/ttyACM0`) in ROM download mode; `esptool` then reached the stub. The
left and middle buttons did not do that. The **middle is PWR**: holding it while the screen is on
turns the board off (user-observed), so it acts on power hardware (a latch or power-path chip), not as
a plain app button. The left is presumably KEY (GPIO18), not yet checked. No separate RESET button was found; reset by replugging USB.

**Battery and power (Waveshare product page, [wiki](https://docs.waveshare.com/ESP32-S3-RLCD-4.2) and its
FAQ; not yet verified on the board):**
- 18650 holder on the board, plus a PH1.0 header for a *rechargeable* RTC cell (never a CR1220). The page
  mentions a "battery charge and discharge management circuit" but names no chip and no charge current.
- Indicators: **CHG** lights while charging and goes off when full; **WRN** stays on if the battery is
  reverse-connected.
- PWR: short press = power on, long press = power off. FAQ: after inserting a battery, **connect a
  Type-C supply once to activate the board**; after that the battery keeps it running without USB.
- Tested 2026-10-27: board running from the 18650, keyboard on the board's USB-C through a plain OTG
  adapter, no hub: the keyboard's lights stayed off, so **the USB-C port did not supply 5 V to the keyboard**
  in that setup. Not tested with a multimeter. With a hub that sat beside the board (power bank into the
  hub, board and keyboard both on the hub's downstream ports) the keyboard lit but could not talk to the
  board, since the hub was the host.
- Not stated anywhere I could read: whether USB-C charges the battery, and whether any pin on the
  headers carries 5 V. Still needs the schematic.

**Unknown (not in any source consulted):** microSD pins, PWR button, battery charge/power-latch
control, whether VBUS can be sourced. Check the Waveshare schematic. Do not guess.

## ST7305 (Sitronix datasheet v0.2, 112 pages)

The datasheet is `ST_7305_V0_2.pdf` from `files.waveshare.com/wiki/common/`. Page numbers below are the
document's own page numbers.

- **Interface** (p. 34-38): 4-line SPI with A0 as DC is what the board wires (DC=5). Datasheet
  also describes 8-bit 8080 parallel, dual 4-line and 3-line SPI. SDA is latched on the rising SCLK
  edge, MSB first; we use SPI mode 0.
- **RAM** (p. 42-43): 264 x 320 x 1 bit. Column address 0x13..0x28, row 0..0x9F for that panel. The
  Waveshare 400x300 panel uses other windows (below).
- **Data packing** (p. 39-41): pixels are written in blocks, not linearly; see `st7305_pack.h`.
- **Windows** (p. 61-63): `CASET (2Ah)`, `RASET (2Bh)`, `RAMWR (2Ch)`. A window must be set before
  writing; column counter increments per pixel group, wraps to the next row at XE.
- **MADCTL 0x36** (p. 64-65): MY/MX/MV/DO/GS bits; the driver uses `0x48` (MX=1, DO=1).
- **DTFORM 0x3A** (p. 67): BPS selects 4 vs 3 writes per 24-bit group; driver sends `0x11`.
- **Power modes** (p. 53, 66-67): HPM (~16/32/25.5/51 Hz, set by FRCTRL/OSCSET) for interaction, LPM
  (0.25-8 Hz) for idle. Switching has required delays (300 ms, 20 ms, 100 ms) and re-sending voltage
  settings; **only the mode command is sent today**.
- **Sleep** (p. 53): `SLPIN 0x10` then 100 ms; `SLPOUT 0x11` then 100-120 ms. The image stays in RAM.
- **Reset and power-up** (p. 48-50): wait for supplies, 50 ms, hardware reset, init, write image.
  Power-down: from sleep-out mode VDDA/VDDI must stay up at least 120 ms after RSTB is released;
  from sleep-in mode 0 ms is enough.
- **Tearing effect** pin exists; not wired/used here.

### Init values and windows actually used

From the Waveshare reference driver as ported in
[`kylehase/ESPHome-ST7305-RLCD`](https://github.com/kylehase/ESPHome-ST7305-RLCD) (Apache-2.0),
`components/st7305_rlcd/st7305_rlcd.cpp`, model `WAVESHARE_400X300`:

- Column window `0x12..0x2A` (25 columns), row window `0x00..0xC7` (200 rows), 3 bytes per address
  = 15000 bytes per frame. Gate lines `0x64`. MADCTL `0x48`. DTFORM `0x11`. Gamma `0xB9=0x20` (mono).
  Panel setting `0xB8=0x29`. Display inversion on (`0x21`). Full table in `st7305.c`.
- Its landscape pixel layout is what `st7305_pack()` implements.
- Writing: HPM and display-on commands, CASET, RASET, then RAMWR with CS held low across the data.

## Radio (ESP32-S3 datasheet v2.2, p. 1)

2.4 GHz Wi-Fi (802.11b/g/n) and **Bluetooth 5 (LE)** with Bluetooth Mesh; one antenna shared by both.
**No Bluetooth Classic**, so only BLE keyboards (HID over GATT) can work. The Waveshare page also says
"Wi-Fi & BLE". 8 MB octal PSRAM and 16 MB flash were confirmed in the boot log. ESP-IDF v5.5 here has
`examples/bluetooth/esp_hid_host` (BLE HID host).

## USB host and the keyboard (superseded: BLE is the plan)

The USB keyboard path was tested and the board's USB-C does not give the keyboard 5 V (details under
"Battery and power" above). The notes below are kept for reference.

- ESP32-S3 USB OTG (datasheet ESP32-S3 p. 4) can act as a full-speed host. ESP-IDF provides the
  `usb` host stack; HID comes from the managed component `espressif/usb_host_hid`.
- **VBUS:** a keyboard needs 5 V. Whether this board's USB-C port can source it is unconfirmed. If not,
  use a powered OTG adapter/hub or feed 5 V separately.
- **One port, two jobs:** the same native USB pins are used for flashing/logging (USB Serial/JTAG)
  and as the host. While hosting, flash with the BOOT button (rightmost) held while plugging in USB (or over UART0) and read logs on
  UART0 (`sdkconfig.usbhost`).
- Keyboard model: HID boot protocol keyboards, US layout.

## Fonts and license

IBM Plex Mono/Sans, Copyright (c) 2017 IBM Corp., Reserved Font Name "Plex", SIL Open Font License
1.1: free to use, embed and bundle, even commercially; the font may not be sold by itself;
derivatives must stay under the OFL and must not use the reserved name. The license text is in
`third_party/ibm-plex/OFL.txt` and is referenced in `fonts_generated.c` and the README. The
generated identifiers are named `plex_*`; rename them if the product is distributed widely and
the reserved-name rule is a concern.

## Sources

- ESP32-S3 Series Datasheet v2.2 (Espressif), provided by the user.
- ST7305 datasheet v0.2 (Sitronix), `files.waveshare.com/wiki/common/ST_7305_V0_2.pdf`.
- Waveshare ESP32-S3-RLCD-4.2 product page; ESPHome device page; `kylehase/ESPHome-ST7305-RLCD`.
