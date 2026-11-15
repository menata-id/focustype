# Case (CAD as code)

OpenSCAD model of a two-part case (front housing and back cover) for the Waveshare ESP32-S3-RLCD-4.2 (V2). It replaces the
acrylic back plate and stand. All dimensions are parameters at the top of `case.scad`; the sources of the board numbers
are in `ref/README.md`. The look follows the reference photo the user gave: equal bezel left, right and top, a wide
bottom bezel with the wordmark engraved in it, and no screws on the front.

```
./render.sh        # PNG into renders/, STL into stl/, then the clash check
openscad -D 'part="housing"' case.scad   # one part: housing, cover, board, assembly, section, check
```

Needs `openscad` (2021.01 is enough). Headless, run it under `xvfb-run`. `.venv/` (cadquery) is only for reading the
Waveshare STEP; it is not needed to build the case.

## Parts and how they go together

| Part | What |
|---|---|
| `housing` | front frame and walls in one piece, printed **face down** so the front has no seams or screws. Window = LCD active area + 0.3 mm. Bezel 9.0 mm on the left, right and top (`bezel_w`), 25 mm on the bottom (`bottom_w`), both measured from the edge of the active area. "FocusType_" is engraved 0.4 mm into the bottom bezel. Six bosses with M2 heat-set inserts take the cover screws. Windows for USB-C, microSD (card enters from the +X edge) and tunnels for the three side buttons |
| `cover` | flat back plate with a closed pocket for the 18650 cell: the cell lies on the PCB and sticks out about 4 mm past the holder frame, so the pocket is a half tube that follows the cell and the holder outline, 19.1 mm deep inside (see Battery below). Six countersunk M2 screws from the back go into the housing inserts. Window for the 2x9 header (UART0 pins stay reachable) |
| `board` | dummy of the real board built from the STEP numbers; for looking and for the check, not printed |

Assembly: put the board in the housing from the back, fit the cover, fasten the six cover screws. The board is not screwed
to anything: it is clamped between the front frame (a thin foam strip on the glass edge, about 0.5 mm, compressed into the
0.3 mm gap) and the cover, which presses on the ends of the board's four standoffs. To change the battery take the six
screws out and lift the cover: the holder is then open, no second lid. The front is sealed.

Edges: plan-view corner radius 3 mm (`corner_r`), 0.8 mm 45 degree chamfer round the front face (`edge_front`) and 1.0 mm
round the back face of the cover (`edge_back`). Chamfers, not fillets, because a fillet on the face that lies on the print
bed starts as a steep overhang. The 9.0 mm bezel (8.7 mm visible, 7.9 mm flat) is the same left, right and top.

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
  look at the datasheet of the insert you buy. Printer tolerances (`clear`, `win_gap`) are guesses: print a
  test fit first.
- **USB-C window** (`usb_win_w`, `usb_win_h`, now 13 x 7, assumed). The socket itself is only 8.94 x 3.16 mm (shell 9.58 x
  4.16) and sits flush with the board edge, so its face is 3.5 mm behind the outside of the wall; a plug only seats fully if
  its overmould can pass through the wall, so the window is sized for the overmould. USB-IF gives no overmould limit I could
  find; plugs vary. Measure the plug or OTG adapter you will use and set the window to that plus 1 mm each way. The LCD flex wraps round the +X edge (photo): 1.4 mm is
  left on that side, not measured.
- The board is only clamped: with the cover off it can fall out of the housing if the case is held face down, and a loose
  fit may rattle. Small locating ribs in the corners are an option if that shows up in a test.
- **Battery:** Waveshare states no size for the 18650; the model uses the size the user gave, a cylinder 18 mm x 65 mm
  (`cell_d`, `cell_len`; see `docs/HARDWARE.md`). The pocket is a half tube sized for the worst case, not for the nominal cell:
  18.6 mm (a cell with its wrap) plus 0.5 mm all round for printing tolerance and swelling (`cell_dmax`, `pocket_clear`), so
  the inside is 19.6 mm across and 19.1 mm below the PCB. It also widens to the outline of the holder frame, which was read from
  sections of the STEP. `./render.sh` checks the nominal cell (18 x 65) and a worst-case protected cell (18.6 x 70). The pocket
  makes the case about 27 mm thick at the battery (6.15 mm in front of the PCB back plus 21.1 mm behind it) and 15.15 mm elsewhere.
  Measure the real cell and its distance from the back of the PCB.
- **Not modelled yet:** the RTC cell (needed to keep the time, see `docs/HANDOFF.md`), the speaker connector and vent, a
  stand (the reference has a small leg at the back) and feet. The battery pocket under the back (about 12 mm) will have to be
  worked into the stand design.
