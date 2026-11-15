# Case (CAD as code)

OpenSCAD model of a housing, a back cover and a battery lid for the Waveshare ESP32-S3-RLCD-4.2 (V2). It replaces the
acrylic back plate and stand. All dimensions are parameters at the top of `case.scad`; the sources of the board numbers
are in `ref/README.md`. The look follows the reference photo the user gave: equal bezel left, right and top, a wide
bottom bezel with the wordmark engraved in it, and no screws on the front.

```
./render.sh        # PNG into renders/, STL into stl/, then the clash check
openscad -D 'part="housing"' case.scad   # one part: housing, cover, lid, board, assembly, section, check
```

Needs `openscad` (2021.01 is enough). Headless, run it under `xvfb-run`. `.venv/` (cadquery) is only for reading the
Waveshare STEP; it is not needed to build the case.

## Parts and how they go together

| Part | What |
|---|---|
| `housing` | front frame and walls in one piece, printed **face down** so the front has no seams or screws. Window = LCD active area + 0.3 mm. Bezel 9.0 mm on the left, right and top (`bezel_w`), 25 mm on the bottom (`bottom_w`), both measured from the edge of the active area. "FocusType_" is engraved 0.4 mm into the bottom bezel. Six bosses with M2 heat-set inserts take the cover screws. Windows for USB-C, microSD (card enters from the +X edge) and tunnels for the three side buttons |
| `cover` | flat back plate. The board hangs on it by its own 4 M2.5 standoffs (screws from the back). Six countersunk M2 screws from the back go into the housing inserts. Window for the 2x9 header. Battery bay under the plate |
| `lid` | plug that closes the battery bay (friction fit) |
| `board` | dummy of the real board built from the STEP numbers; for looking and for the check, not printed |

Assembly: screw the board to the cover, slide the housing over it from the front, fasten the six cover screws, fit the
battery and the lid. The front is sealed; everything is opened from the back.

Why this way (best practice for hidden fasteners): all screws enter from the back; the seam between the housing and the
cover is on the back face; inserts sit deep in bosses far from the front surface so they cannot mark it; the board is not
clamped by the front frame, there is a 0.3 mm gap to the glass (a thin foam strip on the glass edge is better).

## Wordmark

The mockup header uses **IBM Plex Sans, weight 700 (Bold), letter-spacing 0.04 em** for "FocusType" (see the mockup's
Home screen). The case uses the same: `logo_font = "IBM Plex Sans:style=Bold"`, `spacing = 1.04`, loaded from
`../third_party/ibm-plex/IBMPlexSans-Bold.ttf` so it does not depend on installed fonts. The underscore is not in the mockup.

## What has actually been checked

- **Done (on the host only, nothing printed or fitted):** the model renders; `./render.sh` reports no intersection between
  the case and the board dummy. The check was shown to be able to fail: with `-D clear=-1` it reports a clash. The
  section render shows the board between the frame and the cover.
- The dummy is boxes made from bounding boxes in the STEP, so it is only as good as those. The holder footprint is the
  largest section; the real one tapers.
- **Not checked:** anything on a real board or a real print.

## Open items and assumptions

- **Which long edge is the bottom** is a choice: `bottom_neg = true` puts the wide bezel on the battery side, so the three
  buttons (PWR, BOOT, KEY) end up on the narrow top edge, away from the typing hands. Set it to `false` to flip.
  Which way the firmware draws the screen is a separate setting.
- The buttons sit about 0.2 mm inside the PCB edge and the narrow wall is further out, so the housing has tunnels to the
  board edge. Printed button pins are still to be designed; not tested.
- The engraving prints over air when the housing is printed face down (the letters are small bridges). Check it on the
  first print; the alternative is to print it standing up or leave `logo = ""`.
- Insert hole `insert_d = 3.2` for M2 heat-set inserts and the cover screw length are not checked against a part;
  look at the datasheet of the insert you buy. Printer tolerances (`clear`, `win_gap`, lid 0.2) are guesses: print a
  test fit first.
- USB-C window is 13 x 7 for a cable overmould (assumed). The LCD flex wraps round the +X edge (photo): 1.4 mm is
  left on that side, not measured.
- The lid is held by friction only.
- **Not modelled yet:** the RTC cell (needed to keep the time, see `docs/HANDOFF.md`), the speaker connector and vent, a
  stand (the reference has a small leg at the back) and feet. The battery bay under the back (about 8 mm) will have to be
  worked into the stand design.
- Total thickness is about 15 mm plus the 8 mm battery bay.
