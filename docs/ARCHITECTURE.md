# Architecture

## Components

```
main/            app_main, app.c (state machine), bring-up test
components/
  board/         pin map (header only)
  st7305/        panel driver: init, SPI, packer           [ESP-IDF] + st7305_pack.c [pure]
  gfx/           1-bit framebuffer, shapes, UTF-8 text      [ESP-IDF for alloc/flush], fonts_generated.c
  usb_kbd/       USB host + HID                             [ESP-IDF] + kbd_hid.c [pure]
  storage/       mount microSD, fallback to flash FAT       [ESP-IDF]
  fsutil/        atomic save / load                         [pure POSIX]
  notes/         one .txt + optional .meta per note, listing, search  [pure POSIX]; note_meta.c [pure, no FS]
  pcf85063/      RTC over I2C                               [ESP-IDF] + pcf_regs.c [pure]
  editor/        buffer, cursor, wrap, title, word count    [pure]
  ui/            screens + formatters                       [ESP-IDF via gfx] + ui_format.c [pure]
  util/          UTF-8 decode/encode, case-insensitive find [pure]
tools/gen_fonts.py   IBM Plex -> bitmap fonts
tests/host/          host unit tests (plain C, Makefile)
```

"Pure" means no ESP-IDF headers, so it compiles on the host and is covered by `tests/host`.

Dependencies point downward: `main` -> `ui`, `notes`, `editor`, `usb_kbd`, `storage`, `pcf85063`, `util`, `st7305`;
`ui` -> `gfx`, `editor`, `util`; `gfx` -> `st7305`; `notes` -> `fsutil`, `editor`, `util`.

Name clashes with ESP-IDF to avoid: `rtc.h` / `rtc_init` (hence `pcf85063` and the `pcf_` prefix).

## Display pipeline

```
ui_* draws          gfx_flush()                st7305_flush()
into framebuffer -> st7305_flush(fb) ->  pack to panel layout -> SPI RAMWR (15000 bytes)
(400x300, 1 bit,
 row-major, MSB
 first, 1 = ink)
```

- The framebuffer is `ST7305_FB_SIZE` = 15000 bytes in internal RAM (`gfx.c`).
- `st7305_pack()` converts it to the panel's layout: each byte is a 2-column x 4-row block, bytes
  ordered by block column then block row, vertical axis flipped, a cleared bit is black.
  Full details in `st7305_pack.h`. This was checked against the Waveshare reference method.
- The whole frame is sent every time. The panel keeps its own RAM and refreshes itself, so the MCU
  can stop sending and sleep.
- Not thread-safe: only the app task touches `gfx` and `st7305`.

## Text

- Fonts are `gfx_font_t` tables (`gfx_text.h`): sorted glyph list, byte-aligned 1-bit bitmaps,
  ascent and line height. Proportional or monospace, same struct.
- `gfx_draw_text` takes a baseline. `ui.c`'s `baseline()` centres text in a CSS-like line box
  (`y + (lh - line_h)/2 + ascent`) so mockup measurements carry over.
- Glyph set: ASCII plus `· — … “ ” ‘ ’ ← ↑ → ↓`. `▸ ● ✕ ⚑` are not in the fonts and are drawn as
  shapes.

## Editor core (`editor.h`)

- Flat UTF-8 buffer in caller-provided storage, cursor as a byte offset on a codepoint boundary.
- Soft wrap is computed lazily into `starts[]` (byte offset of each visual line) whenever text
  changes. Breaks after the last fitting space, or inside a too-long word; spaces may hang past the
  edge. A trailing `\n` leaves an empty last line so the cursor has somewhere to be.
- Width comes from a callback (`ed_advance_fn`), so any font works. The app passes the editor
  font's advance (`ui_editor_advance`) and a width of `UI_EDITOR_TEXT_W`.
- Up/Down keep a desired pixel column (`want_x`). On a wrapped line, End and vertical moves stop one
  character before the line end, because the position after the last character is the next line's
  start.
- Cost: full relayout per edit, O(n). Fine for notes up to tens of KB.

## Notes and storage

- `storage_mount()` mounts the microSD (SDMMC 1-bit, CLK 38, CMD 21, D0 39) at `/data`; if the pins are
  unset or mounting fails it mounts the internal `storage` FAT partition there instead, silently. It runs once at boot:
  there is no card detection or remount (see `docs/HANDOFF.md`, "Known gaps"). The SD card is never formatted by the app.
- FATFS long file names are on (`CONFIG_FATFS_LFN_HEAP`, in `sdkconfig.defaults`). Without them the atomic save's
  `ft-0001.txt.tmp` is not a valid 8.3 name and every save fails.
- Notes live in `/data/notes/ft-NNNN.txt`. `notes_list()` derives title (first non-blank line),
  word count and mtime from each file; there is no index.
- Optional metadata sits beside it in `ft-NNNN.meta` (`key=value` lines; see `note_meta.h`): manual title, status, category,
  tags, flag, archived, created. `fill_info()` merges it, so `note_info_t.title` is the manual title if set, else the
  first-line `auto_title`. The `.meta` is saved atomically and the `.txt` is never touched by metadata edits.
- `notes_search()` loads every note and looks for the query (ASCII case-insensitive, `tf_find_nocase`), returning a
  snippet around the first match (`note_hit_t`).
- `fs_save_atomic()`: write `<path>.tmp`, `fflush`, `fsync`, `fclose`, unlink old, rename. FAT cannot
  rename over an existing file, so power loss between unlink and rename leaves a complete `.tmp`;
  `fs_load()` and `notes_list()` fall back to it.
- A new empty note is never written; Esc on it simply leaves.

## Keyboard

```
USB device -> usb_host lib task -> hid_host (background task) -> report callback
   -> kbd_hid_parse() -> kbd_event_t queue (32) -> usb_kbd_next() in the app task
```

- Works on the board (2026-10-29) through an OTG adapter that supplies its own 5 V.
- Boot-protocol 8-byte reports only; US layout in `kbd_hid_translate()`.
- Events: `KBD_KEY_CHAR` (with `ch` and `mods`) and named keys. With Ctrl held, `ch` is the
  unshifted letter, so the app checks `mods & KBD_MOD_CTRL` and `ch == 's'`.
- Key repeat is a 10 ms `esp_timer` that re-emits the held key after 400 ms every 35 ms.
- Synthetic events `KBD_KEY_CONNECTED` / `KBD_KEY_DISCONNECTED` drive the "Keyboard not detected"
  screen.
- If the USB host was never started, `usb_kbd_next()` just sleeps for the timeout, so the app loop
  does not spin.

## App state machine (`main/app.c`)

States: `S_HOME`, `S_LIST`, `S_EDITOR`, `S_KBD_ERR`, `S_CLOCK`, `S_SEARCH`, `S_LOOKUP`, `S_PREVIEW`, `S_DETAIL`.
`S_DETAIL` has sub-panels (`P_RENAME`, `P_STATUS`, `P_TAGS`, `P_ARCHIVE`, `P_MSG`). One loop:

```
wait up to 250 ms for a key -> handle_key() -> tick() (autosave, keyboard wait) -> render if dirty
```

- Autosave: 3 s after the last edit while `UI_UNSAVED`.
- Save failure sets `UI_SAVE_FAILED`: banner shown, Esc dismisses it ("Keep writing"), Ctrl+S retries.
  The editor never discards text on a failed save.
- Navigation: Home -> Search / All writing / Editor / Set clock (Ctrl+T); All writing -> Details (Right) -> Rename and the
  other panels; Editor -> Look up (Ctrl+K, saves the draft first) -> Preview -> back, with Ctrl+Enter inserting `[[Title]]`.
  Search and Look up share the query and result code (`search_input`); a debounce in `tick()` runs the search 250 ms after the
  last key. Archived notes are filtered out of All writing and the Home shortcut in `refresh_notes()` but not out of Search.
- Panels over another screen are drawn after `ui_dim()` (a checkerboard of paper pixels), as in the mockup.
- At boot `pcf_init()` sets the system clock from the RTC; if the chip lost its time the app opens Set clock.
- Keyboard disconnect saves a dirty draft first, then shows the error screen; reconnect returns to
  the previous state. With the USB stage on, a keyboard not seen within 5 s of boot also shows it.

## Memory

- 15 KB framebuffer and 15 KB panel buffer (DMA) in internal RAM.
- 64 KB editor buffer and a second 64 KB buffer for the preview, preferably in PSRAM (8 MB octal, enabled in
  `sdkconfig.defaults`).
- `A` in `app.c` (internal RAM) holds the note list (64), the search results with snippets (64) and the Details state.
- Fonts: about 16 KB of bitmaps in flash.

## Build configuration

- `sdkconfig.defaults`: ESP32-S3, 240 MHz, 16 MB flash, octal PSRAM, custom partitions, FATFS long file names, USB
  Serial/JTAG console.
- `sdkconfig.usbhost`: overlay that moves the console to UART0 and sets `CONFIG_FT_ENABLE_USB_KBD`.
- Kconfig (`main/Kconfig.projbuild`): `FT_APP` (default on), `FT_ENABLE_USB_KBD`, `FT_ENABLE_SD`
  (the last is read only by the bring-up code path).
- `partitions.csv`: nvs, phy_init, 3 MB factory app, 3 MB FAT `storage`.
- Managed component: `espressif/usb_host_hid` (declared in `components/usb_kbd/idf_component.yml`),
  downloaded on first build, so the first build needs network.
