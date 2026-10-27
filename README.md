# FocusType firmware

Distraction-free writing device on the Waveshare ESP32-S3-RLCD-4.2: 400x300 reflective LCD
(ST7305), a keyboard (BLE planned), notes stored as plain text files. Built with ESP-IDF 5.x.
The screens are specified in the FocusType mockup.

**Status: early work in progress.** The display and the first screen run on the real board; the
keyboard, microSD, and most screens do not work yet. See [Status](#status).

## Documentation

- `docs/HANDOFF.md`: current status, blockers, what to do first, known risks. **Start here.**
- `docs/ARCHITECTURE.md`: components, display pipeline, editor, storage, keyboard, app flow.
- `docs/HARDWARE.md`: pin map, ST7305 facts with datasheet pages, USB host notes, font license.
- `docs/DESIGN.md`: mockup-to-code mapping, what each screen needs, key bindings.
- `CLAUDE.md`: short rules for AI-assisted sessions.

## Layout

| Path | What it is |
|---|---|
| `components/board` | Pin map (`board.h`) |
| `components/st7305` | Panel driver: init sequence, SPI, pixel packing |
| `components/gfx` | 1-bit framebuffer, drawing primitives, UTF-8 text with bitmap fonts |
| `tools/gen_fonts.py` | Renders IBM Plex into `components/gfx/fonts_generated.c` (`python3 -I tools/gen_fonts.py`, needs Pillow) |
| `third_party/ibm-plex` | Plex TTFs and the OFL 1.1 license text |
| `components/ui` | Screens from the mockup (Home, All writing, Editor, Keyboard not detected) and text formatters |
| `components/notes` | Notes as plain `.txt` files, titles/word counts derived from the files (POSIX, host-testable) |
| `components/fsutil` | Atomic save and load with recovery from an interrupted save |
| `main/app.c` | App state machine: key handling, autosave, navigation |
| `components/editor` | Editor core: UTF-8 buffer, cursor, word wrap, title and word count (no ESP-IDF dependency) |
| `components/util` | UTF-8 decode/encode |
| `components/usb_kbd` | USB host keyboard, HID boot-report parser, key repeat (unproven; BLE replaces it) |
| `components/storage` | microSD mount, atomic save, load with recovery fallback |
| `main` | Bring-up firmware (test pattern, keyboard demo) |
| `tests/host` | Host unit tests for the pixel packer and HID parser |

## Build

```
. $IDF_PATH/export.sh
idf.py set-target esp32s3
idf.py build flash monitor
```

By default the app runs (`CONFIG_FT_APP`). Without the USB keyboard stage it only shows Home.
Notes live on the microSD once its pins are set in `board.h`, otherwise on the 3 MB `storage` flash
partition (see `partitions.csv`).

Built so far: Home, All writing, Editor (typing, wrap, scroll, autosave after 3 s, Ctrl+S, save-failed
banner), Keyboard not detected. Not built yet: Search (Ctrl+K), preview, Details and Rename,
recovery draft on boot, Device status, the KEY/PWR button fallbacks, battery and RTC.

For the bring-up test pattern instead, turn `FocusType > Run the app` off in `idf.py menuconfig`.

Stage 1 draws a test pattern. The three corner squares (24, 12 and 6 px) show whether the image is
mirrored or rotated.

Stage 2 enables the USB host keyboard. **This did not work on the real board**: its USB-C port does
not supply 5 V, so no keyboard can be used that way (see Status). The native USB port becomes the
host, so logs move to UART0:

```
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.usbhost" reconfigure build
```

Each keypress then advances a bar along the bottom of the screen. With this build the board no
longer shows up as `/dev/ttyACM0` on the PC. Hold the rightmost button (BOOT) while plugging in USB
to enter download mode (the board has no RESET button), or flash over UART0.

## Status

Verified on the real board (2026-10-27):

- The firmware builds with ESP-IDF v5.5 (esp32s3, no warnings) and the host tests pass
  (`make -C tests/host`; needs `build-essential`).
- The ST7305 panel works: the test pattern is correct in landscape 400x300 with no rotation, and
  the app boots, mounts the internal-flash FAT partition at `/data`, and draws its first screen with
  readable fonts and no flicker. Flash is 16 MB.
- The board runs from an 18650 cell (needs USB power once to activate).

Not working or not yet verified:

- **Keyboard.** The USB host path was tried and the board's USB-C does not source 5 V, which also
  defeats a portable device. The plan is a **BLE keyboard** (the ESP32-S3 has Bluetooth LE only, no
  Bluetooth Classic). Not started.
- microSD (pins unknown, so notes use internal flash), the KEY button, battery reading, and the RTC.
- Screens other than the first one have never been looked at on the panel.
- Whether the board's USB-C charges the battery is unconfirmed.

Open items and next steps are in `docs/HANDOFF.md`; hardware facts and sources are in
`docs/HARDWARE.md`.

## License

MIT, see `LICENSE`. Bundled IBM Plex fonts stay under the SIL OFL 1.1 (`third_party/ibm-plex/OFL.txt`).

## Credits

The ST7305 init sequence and panel address windows follow the Waveshare reference driver as ported
in [kylehase/ESPHome-ST7305-RLCD](https://github.com/kylehase/ESPHome-ST7305-RLCD) (Apache-2.0).
Command semantics are from the Sitronix ST7305 datasheet v0.2.

Fonts are bitmap renderings of [IBM Plex](https://github.com/IBM/plex), Copyright © 2017 IBM Corp.
with Reserved Font Name "Plex", under the SIL Open Font License 1.1 (`third_party/ibm-plex/OFL.txt`).
