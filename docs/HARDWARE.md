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
| 13 / 14 | I2C SDA / SCL (PCF85063 RTC at 0x51) |
| 15 | RTC INT (schematic net `RTC_INT`, not used) |
| 21 / 38 / 39 | microSD CMD / CLK / D0 (SDMMC 1-bit; from Waveshare's `06_SD_Card` example, works with a card at boot, not yet checked against the schematic) |
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

**RTC (PCF85063ATL, I2C address 0x51 on GPIO13/14, INT on GPIO15; schematic net `RTC_INT`):**
- Registers used (from the Waveshare SensorLib): CTRL1 0x00 (bit 1 = 12/24 h, bit 5 = STOP), time
  registers 0x04..0x0A in BCD, bit 7 of the seconds register = oscillator-stop flag (time invalid).
- The chip has its own backup input, net `RTC_BAT`, on the PH1.0 header for a rechargeable cell.
- **Tested 2026-10-29: with no RTC cell fitted the chip loses the time every time USB is unplugged**
  (the app shows Set clock at each boot). So the RTC is not fed from the 18650/USB while the board is
  off. **Also tested 2026-10-29 with the 18650 fitted: after switching the board off and on again the time
  was still lost**, so the 18650 does not keep the RTC alive while the board is off. Not tested with an RTC
  cell fitted. To keep the time, fit a rechargeable PH1.0 RTC cell (check its voltage
  and polarity against the Waveshare accessory page before buying; never a non-rechargeable CR cell).

**RTC cell circuit (read from the schematic text, 2026-10-29):** connector J7 is a "SH1.0 2P" (1.0 mm pitch,
beige, vertical SMD; the wiki calls it PH1.0, so check the pitch on the real connector before buying). Net
`RTC_BAT` is fed from `VCC3V3` through D2 (B5819WS Schottky) with C11 1 uF to GND, and goes to the PCF85063
backup supply. I found no series resistor or charger IC: the diode charges the cell straight from 3.3 V, so
the cell sees about 3.0 V (3.3 V minus the diode drop) with no current limit. Hence "rechargeable only" and a
cell that is happy at about 3 V (3 V lithium-manganese rechargeable type such as ML-series coin cells). A
non-rechargeable cell or a 3.7 V LiPo is wrong. Pin 1/2 polarity of J7 was not legible in the text; check
with a multimeter on a powered board (RTC_BAT side is the one near the diode) before connecting.

**RTC cell, from the Waveshare FAQ (https://docs.waveshare.com/ESP32-S3-RLCD-4.2/FAQ):** use a secondary
(rechargeable) lithium cell, recommended model **ML1220** or a compatible 3 V/3.3 V rechargeable coin cell.
Do not use non-rechargeable coin cells such as CR1220: they can be damaged or damage the circuit while the
board charges the cell. This matches the schematic reading above. The FAQ calls it a "battery holder" while
the schematic shows a 2-pin SH1.0 connector, so a bare ML1220 may need a lead or adapter; check the real
connector before buying.

**RTC connector polarity, from the Waveshare product photo (`ESP32-S3-RLCD-4.2-HW`, item 10, 2026-10-29):** the
silkscreen next to the 2-pin connector labelled "RTC" shows **"+" on the left pin and "-" on the right pin**, in the
photo orientation (TF card slot at the top left, 18650 holder at the bottom, "RTC" text under the connector). It is a
small white 2-pin connector, visibly smaller than the 3-pin "SPK" one beside it, consistent with 1.0 mm pitch. This is
a product photo, not checked on the user's board; confirm the "+" mark on the real PCB before plugging in a cell.

**Header silkscreen (same photo, item 11):** the 2x9 header is labelled 3V3, GND, GP0, GP1, GP2, GP3, GP17, GP18 on
one row and VBUS, GND, GP19, GP20, TXD, RXD, SDA, SCL (and more) on the other. So the header has a pin labelled
**VBUS** and the native USB pins GP19/GP20. Not tested electrically: whether VBUS is the 5 V from the USB-C port
(it would feed a keyboard when USB-C is plugged into a charger, but the board's own port is the one used as host).

**microSD:** SDMMC 1-bit, CLK 38, CMD 21, D0 39, taken from Waveshare's `esp-idf/06_SD_Card` example
(`sdcard_bsp.h` defaults). Not yet checked against the schematic. Mounting not yet confirmed.

**Unknown (not in any source consulted):** PWR button, battery charge/power-latch
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

## USB host and the keyboard

**2026-10-29: it works.** A mechanical USB keyboard through an OTG adapter that has its own 5 V input is detected and
types. The board's USB-C alone still does not give the keyboard 5 V (details under "Battery and power" above), so this
setup needs external power and is not portable; BLE stays the plan for a portable device (`docs/HANDOFF.md`).

- ESP32-S3 USB OTG (datasheet ESP32-S3 p. 4) can act as a full-speed host. ESP-IDF provides the
  `usb` host stack; HID comes from the managed component `espressif/usb_host_hid`.
- **VBUS:** a keyboard needs 5 V. This board's USB-C port does not source it (tested), so a powered OTG adapter is used.
  The header silkscreen shows a VBUS pin (see above); untested.
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
