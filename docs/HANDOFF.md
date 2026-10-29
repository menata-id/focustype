# Handoff

Last updated: 2026-10-29 (USB keyboard works with a powered OTG adapter; saving notes verified; SD pins from the Waveshare example, untested). Read this first.

## 1. Where things stand

| Area | Written | Compiled | Run on hardware | Checked another way |
|---|---|---|---|---|
| ST7305 driver (`components/st7305`) | yes | yes | no | pixel packer matches the reference LUT (Python port) |
| Framebuffer, drawing, text (`components/gfx`) | yes | yes | no | font output previewed as an image |
| Fonts (`tools/gen_fonts.py`) | yes | n/a | no | rendered preview looked right |
| Editor core (`components/editor`) | yes | yes | no | wrap/cursor logic ported to Python against the test expectations |
| Notes store, atomic save (`notes`, `fsutil`) | yes | yes | yes (2026-10-29: typed text saves and survives reset, on internal flash) | host test passes |
| USB keyboard (`components/usb_kbd`) | yes | yes | yes (2026-10-29: mechanical keyboard through a powered OTG adapter is detected and types) | host test passes |
| Storage mount (`components/storage`) | yes | yes | flash fallback yes; microSD not confirmed | SD pins 38/21/39 (1-bit SDMMC) from Waveshare `esp-idf/06_SD_Card`, not yet checked against the schematic |
| Screens (`components/ui`) | 4 of 14 | yes | no | none, never rendered |
| App state machine (`main/app.c`) | yes | yes | no | none |

The first sessions had no toolchain. On 2026-10-27 the machine got the apt packages, ESP-IDF v5.5
(`~/esp/esp-idf`, shallow clone, installed with the system Python 3.14, which worked) and
`make -C tests/host` passes all five tests. `idf.py set-target esp32s3 && idf.py build` succeeds with
no warnings (`focustype.bin` about 340 KB of the 3 MB partition). The only compile error was a missing
`PRIV_REQUIRES esp_timer` in `main/CMakeLists.txt`, now fixed. **First flash done on a real board (/dev/ttyACM0):** the test pattern shows three corner squares
(24 px top-left, 12 px top-right, 6 px bottom-left), so the panel works and landscape 400x300 needs no
rotation or MADCTL change. This also verified the init sequence, address windows and packer on hardware.
A bug found and fixed on the way: `SPI_TRANS_CS_KEEP_ACTIVE` returns `ESP_ERR_INVALID_ARG` unless the bus
is held with `spi_device_acquire_bus()`; `st7305_flush` now does that. The full app (`FT_APP` on) was also flashed: it boots, mounts internal flash FAT at `/data`
(formatted on first boot), and the first screen renders with readable, uncut fonts and no flicker
(user-checked by eye; which screen exactly was not recorded). **Other screens are still untested.** Flash is now set to 16 MB (boot log
confirms `SPI Flash Size : 16MB`; partition table unchanged: 3 MB app + 3 MB FAT).

**USB keyboard was tried and does not work on this board (2026-10-27).** The board's USB-C port does not
source 5 V: with the board on its 18650 and a plain OTG adapter the keyboard stayed dark (not measured
with a multimeter). Plugging the keyboard and the board into one hub made the keyboard light but gave
no data path, because the hub was the host. The app showed "Keyboard not detected" throughout. A
portable device cannot depend on external 5 V, so **the plan is a BLE keyboard** (section 3, 4). The
`usb_kbd` component stays in the tree but is unproven; `usb_host_hid` was never seen to enumerate
anything. Current `sdkconfig` was generated with `sdkconfig.usbhost` (UART0 console, USB host on);
the previous one is `sdkconfig.bak-before-usbhost`.
Compiled means only that it compiles: the `usb_host_hid` calls, SPI behavior and all UI are unverified.
Use `. ~/esp/esp-idf/export.sh` in each new shell.

**2026-10-29 Search.** Home > Search (or Ctrl+K) opens a query box; `notes_search` scans every note's full text
(ASCII case-insensitive; UTF-8 letters match only exactly) 250 ms after the last keystroke. Host-tested
(`test_notes`); compiled; **not yet seen on the board**. Scans up to 64 results from up to 256 files, so it
may feel slow on a large SD card. No snippet or match highlight yet.

**2026-10-29 RTC.** `components/pcf85063` reads/writes the PCF85063 over I2C (`pcf_init` at boot sets the system
clock; `pcf_set` writes the chip and the system clock). Register logic is in `pcf_regs.c` with host test
`test_pcf`. The Set clock screen (`ui_clock`, state `S_CLOCK`) opens at boot if the chip lost its time, or
with Ctrl+T on Home. Verified on the board: setting the time works. **The time is lost on every unplug
because no RTC cell is fitted**, see `docs/HARDWARE.md`; this is hardware, not a bug. Until a cell is
fitted, Set clock appears at each boot (Esc skips it, the system clock then stays at 1970). The component
is named `pcf85063` because ESP-IDF already owns `rtc.h` and `rtc_init`.

**2026-10-29 fixes.** Saving failed on every note because `CONFIG_FATFS_LFN_NONE` (8.3 names only) rejects the
atomic-save temp file `ft-0001.txt.tmp`. `sdkconfig.defaults` now sets `CONFIG_FATFS_LFN_HEAP=y`; verified on the
board. The microSD was not detected because `FT_SD_PIN_*` were `-1`; they are now set from the Waveshare example.
Whether the card mounts is still to be confirmed (needs UART0 log or reading the card on a laptop).

## 2. Do this first

Done on 2026-10-27: toolchain, `dialout`, first flash, panel orientation, full app boot, 16 MB flash.

Flashing: with the USB host build, `/dev/ttyACM0` does not appear. **Hold the rightmost button (BOOT)
while plugging in USB** to enter download mode (this board has no RESET button), then
`idf.py -p /dev/ttyACM0 flash`. For a normal flash with console on USB, delete `sdkconfig` and run
`idf.py reconfigure` without the overlay. Reading serial without a TTY: open the port with pyserial, toggle
RTS to reset. After inserting a battery the board needs USB power once to activate (PWR short press
powers on, long press powers off).

Next: pick a BLE keyboard (the user has none yet, only a wired USB one), then build `ble_kbd`
(section 4). Meanwhile KEY/battery work and the other screens can proceed without a keyboard.

## 3. Open hardware questions (blockers for features)

| Question | Why it matters | Where to look |
|---|---|---|
| Can the USB-C port supply 5 V to a keyboard? | **Answered: no** in the OTG test (see `docs/HARDWARE.md`). USB keyboard is dropped in favour of BLE. Only revisit if a header 5 V pin plus GPIO19/20 turns out to be exposed. | Waveshare schematic. |
| microSD pins | `FT_SD_PIN_*` are `-1`, so notes use internal flash. | Schematic. Fill `board.h`; `storage.c` assumes SDMMC 1-bit. |
| PWR button | **Partly answered:** it is the middle button; holding it powers the board off, so it works on power hardware, not as a normal app button. Do not use it as a navigation key. KEY (left, GPIO18, unverified) and BOOT (right, GPIO0) are the readable ones. | Schematic, for any GPIO that reads PWR. |
| Flash size | **Answered:** 16 MB, now set. The partition table was left at 3 MB app + 3 MB FAT; about 10 MB are unused. Growing `storage` reformats `/data`. | |
| Panel orientation | **Answered:** landscape 400x300, no rotation needed (test pattern correct on the board). | |
| Which logs when USB host is on | Only relevant if USB host stays. The native USB port is the host, so USB Serial/JTAG logging stops. | `sdkconfig.usbhost` moves logs to UART0 (GPIO43/44, needs a USB-UART adapter). BLE does not need this, so USB logging can come back. |
| Battery charging | The board has an 18650 holder and a charge/discharge circuit (chip and current not published on the pages read). CHG and WRN LEDs exist. Whether USB-C charges the cell is not stated. | Schematic; or watch the CHG LED with USB-C connected. |
| Which BLE keyboard | ESP32-S3 has Bluetooth 5 LE only, no Bluetooth Classic, so a Classic or 2.4 GHz-dongle keyboard will not work. | See section 4 item 3. |

## 4. Next steps after it builds, in order

1. Confirm the panel shows the pattern and text correctly. If mirrored/rotated, adjust MADCTL
   (`0x36`, now `0x48`) or the packer in `st7305_pack.c`, with a host test change to match.
2. Tune SPI speed (10 MHz now) and check for flicker; consider the datasheet's partial window
   (CASET/RASET) once the full-frame path works. Each keypress currently repaints and sends all
   15 KB (about 12 ms at 10 MHz).
3. Keyboard over **BLE HID (HOGP)**, replacing USB as the main path. The ESP32-S3 has Bluetooth 5 LE
   only. ESP-IDF ships `examples/bluetooth/esp_hid_host` and the `esp_hid` component. Plan: a
   `ble_kbd` component with the same `kbd_event_t` interface as `usb_kbd`, so `app.c` is unchanged;
   a pairing screen (scan, pick, passkey), bonds stored in NVS, auto-reconnect at boot, key repeat
   (see `kbd_hid.c`, already host-tested), and a battery-draw check. Needs a US-layout keyboard that
   advertises BLE (not "Bluetooth 3.0", not a dongle-only one). Not started; the user has no such
   keyboard yet. Then drop `sdkconfig.usbhost` and bring USB Serial/JTAG logging back.
4. Confirm the SD card mounts, then PWR/KEY buttons and battery ADC (GPIO4, show "Not calibrated" until
   verified). RTC is done in software; it needs a backup cell to keep the time (see above).
5. Remaining screens and features, see `docs/DESIGN.md` (Search, preview, Details, Rename,
   recovery on boot, Device status).
6. Low-power mode: only the HPM/LPM command is sent; the datasheet's voltage re-setting sequence
   (section 7.11) is not implemented.

## 5. Known risks in the existing code

- **`usb_host_hid` API.** `usb_kbd.c` was written from memory of the component's `hid_host` example
  (`hid_host_device_get_params`, `hid_host_device_open/start/close`, `hid_class_request_set_protocol`,
  `hid_class_request_set_idle`, `hid_host_device_get_raw_input_report_data`, event enum names).
  Names or struct fields may differ in the version the component manager fetches.
- **SPI details.** `SPI_TRANS_CS_KEEP_ACTIVE` is used to hold CS low from the RAMWR command through
  the 15 KB data burst; verify the data lands and the commands (sent one byte per transaction, CS
  toggling) are accepted.
- **Init sequence provenance.** Taken from the Waveshare driver as ported in
  `kylehase/ESPHome-ST7305-RLCD` (Apache-2.0), including command `0x62` that the datasheet does not
  describe. The datasheet's own example is for a 264x320 panel and uses different values.
- **`notes_list` limit.** It reads up to 256 ids in directory order, fills at most 64, then sorts by
  mtime. With more than 64 notes the newest may be missing. Also FAT mtimes are wrong without an RTC.
  Fix: sort ids before reading, or keep a small index.
- **`notes` is not reentrant** (static id tables). Call it from the app task only.
- **Editor limits.** Buffer is a fixed 64 KB (PSRAM). Out-of-memory while growing the line table is
  not handled beyond keeping the lines laid out so far. Tab is ignored. No undo.
- **Wear.** Autosave rewrites the whole note every 3 s of idle after an edit. Fine for the SD card;
  the internal flash fallback is wear-levelled but is meant only for bring-up.
- **Untested UI geometry.** Layout numbers come from the mockup's CSS; nothing was rendered.
  Likely tweaks: baseline alignment of text in boxes, footer item spacing, the 6th list row being
  clipped by the footer on purpose.
- **`main/main.c`** keeps the bring-up code behind `CONFIG_FT_APP`; when the app is on, that code is
  unreachable but still compiled.

## 6. Decisions already made (do not redo without reason)

- ESP-IDF, not Arduino/PlatformIO (best USB host and SD support). Chosen by the user.
- Notes are plain UTF-8 `.txt` files, one per note, no database. Metadata such as manual title,
  status, tags and flag is **not** stored yet; the plan is a sidecar file per note.
- Fonts are IBM Plex rendered to bitmaps (SIL OFL 1.1; license text in `third_party/ibm-plex`).
- The user's keyboard is wired USB (HID boot protocol, US layout).
- Mockup is the source of truth for screens; see `docs/DESIGN.md`.
