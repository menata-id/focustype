# FocusType: notes for AI sessions

ESP-IDF firmware for a distraction-free writing device: ESP32-S3 + Waveshare 400x300 ST7305
reflective LCD + USB keyboard + notes as files. Start with `docs/HANDOFF.md` (status, next steps,
blockers), then `docs/ARCHITECTURE.md` and `docs/HARDWARE.md`.

## State in one paragraph
The code builds and runs on the board. **Run on the board (2026-10-29): panel, Home, All writing, the editor, saving, the
USB keyboard (through a powered OTG adapter), the microSD at boot, and the RTC time setting. Built and only host-checked (rendered
on the host, never seen on the panel): Search, Look up, Preview, Details, Rename and its panels, the empty-note hint.** Say which
of the two a thing is; never call a host-only screen "working". Remaining from the mockup: Device status and the recovery draft.
Known gaps (SD hot-swap, RTC needs a backup cell) are in `docs/HANDOFF.md`. The mockup (Claude Design artifact
`T8zGCdmbkZBKXvRYoEt3x9`, readable with the Artifact tool) is the source of truth for screens; list every choice the mockup does
not make. Source ESP-IDF with `. ~/esp/esp-idf/export.sh` (its Python venv has no Pillow; use `/usr/bin/python3 -I` for that).

## Conventions
- C11, ESP-IDF 5.x components under `components/`, app in `main/`. 4-space indent, no tabs.
- Code, comments, docs and commit messages in English. The user writes in Indonesian; answer them in
  Indonesian.
- Keep logic that does not need the hardware free of ESP-IDF headers so it runs in `tests/host`
  (`editor`, `notes` incl. `note_meta.c`, `fsutil`, `util`, `usb_kbd/kbd_hid.c`, `st7305/st7305_pack.c`,
  `pcf85063/pcf_regs.c`, `ui/ui_format.c`).
- Do not hand-edit `components/gfx/fonts_generated.c` or `include/gfx_fonts.h`; run
  `python3 -I tools/gen_fonts.py` (needs Pillow).
- Pixel convention everywhere: framebuffer bit 1 = ink (black). The panel packer inverts it.

## Verify before claiming anything works
- Host tests: `make -C tests/host` (needs a host C compiler, `build-essential`).
- Firmware: `idf.py build`, then flash and look at the panel. The test pattern
  (`FocusType > Run the app` off in menuconfig) has three corner squares of 24/12/6 px to reveal
  mirroring or rotation.
- Never report "works" for anything only compiled or only host-tested; say what was actually run.

## Do not
- Do not guess pins. Unknown pins are `-1` in `components/board/include/board.h` on purpose.
- Do not change the ST7305 init sequence or address windows without a reference: they come from the
  Waveshare driver (see `docs/HARDWARE.md`), not from the datasheet's 264x320 example.
- The current `sdkconfig` was generated with the `sdkconfig.usbhost` overlay (USB host on, UART0 console), so `/dev/ttyACM0` only appears in download mode (hold the rightmost button, BOOT, while plugging in USB).
- Do not enable the USB keyboard and expect USB Serial/JTAG logs: the native USB port becomes the
  host PHY. Use the UART0 overlay `sdkconfig.usbhost`.
