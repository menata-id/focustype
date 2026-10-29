# Handoff

Last updated: 2026-10-29. Read this first.

## 1. Where things stand

The firmware builds with ESP-IDF v5.5 (esp32s3) with no warnings, and `make -C tests/host` passes all seven
host tests. It runs on the real board. "Run on the board" below means the user flashed it and used it; "host only"
means it compiles, its logic is host-tested and its screen was rendered on the host and looked at as an image, but
nobody has seen it on the panel yet. Never call a host-only item "working".

| Area | Status |
|---|---|
| ST7305 driver, framebuffer, text, fonts | **Run on the board.** Test pattern correct in landscape 400x300 with no rotation; text readable, no flicker. |
| Editor core (typing, wrap, scroll) | **Run on the board.** |
| Notes store and atomic save (`notes`, `fsutil`) | **Run on the board** (2026-10-29): typed text saves and survives a reset. This needed `CONFIG_FATFS_LFN_HEAP=y` (see below). |
| USB keyboard (`usb_kbd`) | **Run on the board** (2026-10-29): a mechanical keyboard through an OTG adapter *with its own power* is detected and types. The board's USB-C still gives no 5 V by itself (see `docs/HARDWARE.md`). |
| microSD (`storage`) | **Run on the board:** with a card inserted at boot, notes on it are read and written. Pins 38/21/39 (SDMMC 1-bit) come from Waveshare's example and are not yet checked against the schematic. The card is mounted once at boot; see "Known gaps". |
| PCF85063 RTC (`pcf85063`) | **Run on the board:** the Set clock screen works and the time is kept while powered. **The time is lost on every unplug** because no RTC cell is fitted (the 18650 does not feed the RTC when the board is off, tested). Needs a rechargeable ML1220-type 3 V cell, see `docs/HARDWARE.md`. |
| Screens seen on the board | Home, All writing, Editor with save states, Save failed banner, Set clock. |
| Screens host only | Search, Look up (Ctrl+K in the editor), Preview, Details, Rename and its panels (Change status, Tags & category, Archive, Export notice), the empty-note hint (B1). |
| Metadata (`<id>.meta`) | Host only: `test_meta`, `test_notes`. |

Per-screen status and the key bindings are in `docs/DESIGN.md`.

### What changed since the first flash (2026-10-27 to 2026-10-29)

- **Saving failed on every note** because `CONFIG_FATFS_LFN_NONE` (8.3 names only) rejects the atomic-save temp file
  `ft-0001.txt.tmp`. `sdkconfig.defaults` now sets `CONFIG_FATFS_LFN_HEAP=y`. Verified on the board.
- **microSD** pins were `-1`; now set (see above).
- **RTC** driver and the Set clock screen (`components/pcf85063`, state `S_CLOCK`). It opens at boot when the chip lost its
  time, or with Ctrl+T on Home. Its starting values are the firmware build time (`__DATE__`/`__TIME__`), not a real
  clock. Esc skips it and the system clock then stays at 1970. The component is named `pcf85063` because ESP-IDF owns
  `rtc.h` and `rtc_init`.
- **Search** (Home, Ctrl+K): full text, ASCII case-insensitive, snippet with the match bold and underlined.
- **Look up** (Ctrl+K in the editor): saves the draft, panel over the dimmed editor, Enter previews read-only in a second
  64 KB buffer, Ctrl+Enter inserts `[[Title]]`, Esc steps back. The note being written is hidden from its own results.
- **Metadata, Details, Rename:** see `docs/DESIGN.md` ("Metadata storage") for the format and for every decision that is
  mine rather than the mockup's (statuses, tag format, what Archive does, Export being only a notice).
- **B1:** placeholder and dashed hint box in a new empty note. Not italic: the font set has no italic.
- Shared helpers: `util/textfind.c` (`tf_find_nocase`), `ui_dim`, `footer_at`, `hit_row`.

Use `. ~/esp/esp-idf/export.sh` in each new shell. Note that it puts a Python venv first in `PATH` that has no Pillow;
use `/usr/bin/python3 -I` for anything that needs Pillow (fonts, host renders).

## 2. Flashing

The current `sdkconfig` was generated with the `sdkconfig.usbhost` overlay (USB host on, UART0 console), so
`/dev/ttyACM0` only appears in download mode. **Hold the rightmost button (BOOT) while plugging in USB** (the board has
no RESET button), then `idf.py -p /dev/ttyACM0 flash`, then unplug and plug in again without BOOT. For a normal flash with
the console on USB, delete `sdkconfig` and run `idf.py reconfigure` without the overlay. With the host build, logs are only on
UART0 (GPIO43/44, needs a USB-UART adapter). Reading serial without a TTY: open the port with pyserial, toggle RTS to reset.
After inserting a battery the board needs USB power once to activate (PWR short press powers on, long press powers off).

Because the app gives no on-screen indication of where notes are stored, check the log line `microSD mounted` or
`microSD mount failed ... using internal flash` when in doubt.

## 3. Known gaps (deferred by the user, not forgotten)

- **SD card is mounted once, at boot.** There is no card-detect pin and no remount. Pulling the card while the board is
  on makes reads and writes fail until a restart, and a restart *without* a card silently falls back to the internal
  flash `storage` partition, where new notes then live separately from the card's notes (same path, `/data/notes`).
  Nothing on screen says which one is in use.
- **A failed note listing shows "No writing yet"** (`refresh_notes` treats an error as zero notes), which makes notes look
  lost when they are only unreadable.
- Proposed fixes, not started: remount on failure and a periodic retry, show the storage in use ("SD card" / "Internal
  memory") on the Status screen, show an error instead of "No writing yet", and optionally copy notes from flash to the
  card when it returns (needs rules so nothing is overwritten).
- The card is never formatted by the app (`format_if_mount_failed = false`); only the internal flash fallback is.

## 4. Open hardware questions

| Question | State |
|---|---|
| 5 V for a USB keyboard | Not from the USB-C port alone (tested). A powered OTG adapter works. The header silkscreen shows a **VBUS** pin next to GPIO19/20; not tested electrically, so unknown whether it could power a keyboard. |
| RTC cell | Needed to keep the time. ML1220 or a compatible 3 V/3.3 V rechargeable coin cell (Waveshare FAQ), on the 2-pin 1.0 mm connector, "+" on the left pin in the product photo. Check polarity on the real PCB; never a CR-type cell. |
| microSD pins | Taken from Waveshare's example and they work with a card inserted at boot; not checked against the schematic. |
| PWR button | The middle button; holding it powers the board off, so it is a power control, not an app key. KEY (left, GPIO18) and BOOT (right, GPIO0) are readable; KEY was not tested. |
| Battery ADC (GPIO4) | Not tested; the UI must say "Not calibrated" until readings are verified. |
| Does USB-C charge the 18650? | Not confirmed (watch the CHG LED). |
| Flash size | 16 MB, set. The partition table is still 3 MB app + 3 MB FAT; about 10 MB are unused. Growing `storage` reformats `/data`. |
| BLE keyboard | No longer urgent now that a powered OTG adapter works, but a portable device would still need it: the ESP32-S3 has Bluetooth LE only (no Classic), so only BLE HID keyboards can work. ESP-IDF has `examples/bluetooth/esp_hid_host`. |

## 5. Next steps, in order

1. **Look at the host-only screens on the board** (list in section 1) and report what is wrong. Layout numbers come from
   the mockup's CSS and were only rendered on the host, so expect small tweaks.
2. **F1 Status & settings** (not built): rows Keyboard, microSD (Ready, free space), Battery ("Not calibrated"), Date & time
   (Enter opens the existing Set clock), Autosave, Firmware. The mockup does not say how to open it; a shortcut from Home is
   the proposal. It also answers "which storage is in use".
3. **A1 Recovery draft** (not built, needs a design first). Autosave currently overwrites the note every 3 s, so there is no
   "draft" separate from the "saved version" as in the mockup. Decide when a draft is written and when it counts as saved.
4. The SD gaps in section 3, if the user wants them.
5. KEY/PWR button fallbacks and the battery ADC.
6. BLE keyboard (`ble_kbd` with the same `kbd_event_t` interface as `usb_kbd`, pairing screen, bonds in NVS, auto-reconnect).
7. Display: tune SPI speed (10 MHz now), consider partial windows (each keypress sends the full 15 KB, about 12 ms), and the
   low-power sequence (datasheet section 7.11; only the HPM/LPM command is sent now).

## 6. Known risks in the existing code

- **`notes_list` limit.** It reads up to 256 ids in directory order, fills at most 64, then sorts by mtime. With more than 64
  notes the newest may be missing. Fix: sort ids before reading, or keep a small index. It also reads each note's `.meta`.
- **Search cost.** `notes_search` loads every note's text for each query (after a 250 ms pause); it may feel slow on a large card.
- **`notes` is not reentrant** (static id tables). Call it from the app task only.
- **Editor limits.** The buffer is a fixed 64 KB (PSRAM); the preview has a second one. Out-of-memory while growing the line
  table is not handled beyond keeping the lines laid out so far. Tab is ignored. No undo. Text fields in the Details panels
  and the search box take ASCII only (the keyboard path only produces ASCII).
- **Memory.** `A` in `app.c` holds the note list, the search results and their snippets (tens of KB of internal RAM). Watch it
  if the limits grow.
- **Wear.** Autosave rewrites the whole note every 3 s of idle after an edit. Fine for the SD card; the internal flash fallback is
  wear-levelled but meant for bring-up.
- **Metadata.** `<id>.meta` is optional: delete one by hand and the note is unaffected, but its title, status, tags, flag and
  created time go back to the defaults.
- **Time.** FAT times are wrong while the clock is unset; "created" is only written once the clock reads 2025 or later.
- **Init sequence provenance.** Taken from the Waveshare driver as ported in `kylehase/ESPHome-ST7305-RLCD` (Apache-2.0),
  including command `0x62` that the datasheet does not describe. The datasheet's own example is for a 264x320 panel.
- **`main/main.c`** keeps the bring-up code behind `CONFIG_FT_APP`; with the app on, that code is unreachable but still compiled.

## 7. Decisions already made (do not redo without reason)

- ESP-IDF, not Arduino/PlatformIO (best USB host and SD support). Chosen by the user.
- Notes are plain UTF-8 `.txt` files, one per note, no database. Metadata lives in a sidecar `.meta` per note.
- Fonts are IBM Plex rendered to bitmaps (SIL OFL 1.1; license text in `third_party/ibm-plex`).
- The user's keyboard is wired USB (HID boot protocol, US layout), used through a powered OTG adapter.
- The mockup is the source of truth for screens: the Claude Design canvas "FocusType — RLCD Screens"
  (`https://claude.ai/artifact/T8zGCdmbkZBKXvRYoEt3x9`, private to the owner's account; read it with the Artifact tool).
  Anything the mockup does not draw is a decision of the implementer and is listed in `docs/DESIGN.md`.
