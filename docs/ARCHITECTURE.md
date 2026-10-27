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
  notes/         one .txt per note, listing                 [pure POSIX]
  editor/        buffer, cursor, wrap, title, word count    [pure]
  ui/            screens + formatters                       [ESP-IDF via gfx] + ui_format.c [pure]
  util/          UTF-8 decode/encode                        [pure]
tools/gen_fonts.py   IBM Plex -> bitmap fonts
tests/host/          host unit tests (plain C, Makefile)
```

"Pure" means no ESP-IDF headers, so it compiles on the host and is covered by `tests/host`.

Dependencies point downward: `main` -> `ui`, `notes`, `editor`, `usb_kbd`, `storage`, `st7305`;
`ui` -> `gfx`, `editor`; `gfx` -> `st7305`; `notes` -> `fsutil`, `editor`.

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

- `storage_mount()` mounts the microSD (SDMMC 1-bit) at `/data`; if the pins are unset or mounting
  fails it mounts the internal `storage` FAT partition there instead.
- Notes live in `/data/notes/ft-NNNN.txt`. `notes_list()` derives title (first non-blank line),
  word count and mtime from each file; there is no index.
- `fs_save_atomic()`: write `<path>.tmp`, `fflush`, `fsync`, `fclose`, unlink old, rename. FAT cannot
  rename over an existing file, so power loss between unlink and rename leaves a complete `.tmp`;
  `fs_load()` and `notes_list()` fall back to it.
- A new empty note is never written; Esc on it simply leaves.

## Keyboard

```
USB device -> usb_host lib task -> hid_host (background task) -> report callback
   -> kbd_hid_parse() -> kbd_event_t queue (32) -> usb_kbd_next() in the app task
```

- Boot-protocol 8-byte reports only; US layout in `kbd_hid_translate()`.
- Events: `KBD_KEY_CHAR` (with `ch` and `mods`) and named keys. With Ctrl held, `ch` is the
  unshifted letter, so the app checks `mods & KBD_MOD_CTRL` and `ch == 's'`.
- Key repeat is a 10 ms `esp_timer` that re-emits the held key after 400 ms every 35 ms.
- Synthetic events `KBD_KEY_CONNECTED` / `KBD_KEY_DISCONNECTED` drive the "Keyboard not detected"
  screen.
- If the USB host was never started, `usb_kbd_next()` just sleeps for the timeout, so the app loop
  does not spin.

## App state machine (`main/app.c`)

States: `S_HOME`, `S_LIST`, `S_EDITOR`, `S_KBD_ERR`. One loop:

```
wait up to 250 ms for a key -> handle_key() -> tick() (autosave, keyboard wait) -> render if dirty
```

- Autosave: 3 s after the last edit while `UI_UNSAVED`.
- Save failure sets `UI_SAVE_FAILED`: banner shown, Esc dismisses it ("Keep writing"), Ctrl+S retries.
  The editor never discards text on a failed save.
- Keyboard disconnect saves a dirty draft first, then shows the error screen; reconnect returns to
  the previous state. With the USB stage on, a keyboard not seen within 5 s of boot also shows it.

## Memory

- 15 KB framebuffer and 15 KB panel buffer (DMA) in internal RAM.
- 64 KB editor buffer, preferably in PSRAM (8 MB octal, enabled in `sdkconfig.defaults`).
- Fonts: about 16 KB of bitmaps in flash.

## Build configuration

- `sdkconfig.defaults`: ESP32-S3, 240 MHz, 8 MB flash, octal PSRAM, custom partitions, USB
  Serial/JTAG console.
- `sdkconfig.usbhost`: overlay that moves the console to UART0 and sets `CONFIG_FT_ENABLE_USB_KBD`.
- Kconfig (`main/Kconfig.projbuild`): `FT_APP` (default on), `FT_ENABLE_USB_KBD`, `FT_ENABLE_SD`
  (the last is read only by the bring-up code path).
- `partitions.csv`: nvs, phy_init, 3 MB factory app, 3 MB FAT `storage`.
- Managed component: `espressif/usb_host_hid` (declared in `components/usb_kbd/idf_component.yml`),
  downloaded on first build, so the first build needs network.
