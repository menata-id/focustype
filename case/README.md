# Case (CAD as code)

OpenSCAD model of a back shell, front bezel and battery lid for the Waveshare ESP32-S3-RLCD-4.2 (V2). It replaces the
acrylic back plate and stand. All dimensions are parameters at the top of `case.scad`; their sources are in `ref/README.md`.

```
./render.sh        # PNG into renders/, STL into stl/, then the clash check
openscad -D 'part="shell"' case.scad     # one part: shell, bezel, lid, board, assembly, section, check
```

Needs `openscad` (2021.01 is enough). Headless, run it under `xvfb-run`. `.venv/` (cadquery) is only for reading the
Waveshare STEP; it is not needed to build the case.

## Parts

| Part | What |
|---|---|
| `shell` | tray around the PCB; floor with 4 M2.5 screw holes into the board's own standoffs; windows for the 2x9 header, USB-C, microSD (card enters from the +X edge), the 3 side buttons; battery bay under the back; 4 pilot holes for the bezel screws |
| `bezel` | 1.5 mm front frame, window = LCD active area + 0.3 mm, 4 countersunk holes for M2 self-tapping screws into the shell walls |
| `lid` | plug that closes the battery bay (friction fit) |
| `board` | dummy of the real board built from the STEP numbers; for looking and for the check, not printed |

## What has actually been checked

- **Done (on the host only, nothing printed or fitted):** the model renders; `./render.sh` reports no intersection between
  the case and the board dummy. The check was shown to be able to fail: with `-D clear=-1` it reports a clash.
- The dummy is boxes made from bounding boxes in the STEP, so it is only as good as those. The holder footprint is the
  largest section; the real one tapers.
- **Not checked:** anything on a real board or a real print.

## Open items and assumptions

- Printer tolerances: `clear = 0.3`, lid clearance 0.2 are guesses. Print a test fit first.
- USB-C window is 13 x 7 for a cable overmould (assumed), not measured on a plug.
- `fpc_extra = 1.0` at the +X end for the LCD flex that wraps round that edge (photo): not measured.
- The buttons sit about 0.2 mm inside the PCB edge, so the windows only let a pin or fingertip reach them. Whether that
  presses them was not tested.
- The lid is held by friction only; the bezel is held by four M2 self-tapping screws that were not tried.
- **Not modelled yet:** the RTC cell (needed to keep the time, see `docs/HANDOFF.md`), the speaker connector and vent,
  a stand, and any feet. The RTC connector is on the PCB but its position was not read from the STEP.
- Total thickness is about 15 mm plus 8 mm for the battery bay; the stand angle of the original (60 degrees) is not reproduced.
