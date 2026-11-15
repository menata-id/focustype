# Case (CAD as code)

OpenSCAD model of a two-part case (front housing and back cover) for the Waveshare ESP32-S3-RLCD-4.2 (V2). It replaces the
acrylic back plate and stand. All dimensions are parameters at the top of `case.scad`; the sources of the board numbers
are in `ref/README.md`. The look follows the reference photo the user gave: equal bezel left, right and top, a wide
bottom bezel with the wordmark engraved in it, and no screws on the front.

```
./render.sh        # PNG into renders/, STL into stl/, the clash and clearance checks, and the section drawings
openscad -D 'part="housing"' case.scad   # one part: housing, cover, board, assembly, section, check
```

Needs `openscad` (2021.01 is enough). Headless, run it under `xvfb-run`. The section drawings (`sections.py`) need a venv at
`case/.venv` with `trimesh networkx shapely matplotlib` (`cadquery` was only used to read the Waveshare STEP); `render.sh` skips
them if it is missing. The venv is not committed.

## Parts and how they go together

| Part | What |
|---|---|
| `housing` | front frame and walls in one piece, printed **face down** so the front has no seams or screws. Window = LCD active area + 0.3 mm. Bezel 9.0 mm on the left, right and top (`bezel_w`), 25 mm on the bottom (`bottom_w`), both measured from the edge of the active area. "FocusType_" is engraved 0.4 mm into the bottom bezel. Six bosses with M2 heat-set inserts take the cover screws; they are kept at least 0.6 mm from the board (5.6 mm wide on the narrow edges, 6.4 mm in the wide band). Windows for USB-C, microSD (card enters from the +X edge) and tunnels for the three side buttons |
| `cover` | flat back plate with a closed pocket for the 18650 cell: the cell lies on the PCB and sticks out about 4 mm past the holder frame, so the pocket is a half tube that follows the cell and the holder outline, 19.1 mm deep inside (see Battery below). Four countersunk M2.5 screws from the back go into the board's own four standoffs, as the Waveshare acrylic plate does (`board_screws`). Six countersunk M2 screws go into the housing inserts. Window for the 2x9 header (UART0 pins stay reachable) |
| `board` | dummy of the real board built from the STEP numbers; for looking and for the check, not printed |

Assembly: screw the board to the cover with four M2.5 screws into its standoffs, slide the two into the housing from the
back, fasten the six M2 cover screws. The glass sits 0.3 mm behind the front frame (a thin foam strip on the glass edge, about
0.5 mm, compressed into that gap, is better than leaving it bare). To change the battery take out the six M2 screws, pull the
cover with the board out of the housing, and take the four M2.5 screws out to free the board: the holder is then open. The front
is sealed. If that is too fiddly, set `board_screws = false`: the board is then only clamped between the front frame and the
cover (which presses on the ends of the standoffs) and the battery is reached by lifting the cover alone, but the board is not fixed.

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

- **Done (on the host only, nothing printed or fitted):** the model renders. `./render.sh` runs three checks and stops on a failure:
  no intersection between the case and the board dummy (cell 18 x 65), no part of the case closer than 0.45 mm sideways to the board
  (this is what caught the top bosses at 0.24 mm from the PCB and the button rib at 0.34 mm, both now 0.6 mm or more), and a
  worst-case protected cell (18.6 x 70) still fits. The clash check was shown to be able to fail: with `-D clear=-1` it reports a clash.
- **Section drawings** (`sections.py`, written to `renders/`): `sections_front.png` (cuts along the long side seen from the front,
  like the section in the mockup), `sections_side.png` (cuts across it) and `sections_plan.png` (flat cuts at six depths). Grey is the
  case, green the board, orange the cell, and the smallest distances between them are labelled in mm; `gaps.txt` has the same numbers.
  Smallest sideways gaps now: PCB to the button rib 0.60, to the top bosses 0.64, to the left bosses 1.14, to the +X wall 1.34 (room for
  the flex), glass to the walls 1.4, battery holder to the pocket 0.49 all round and 0.8 at its ends, the cell to the pocket floor 1.1
  (0.5 with a 18.6 mm cell). Vertical: glass to the front frame 0.30 and the standoffs resting on the cover (0.01 in the model, a
  deliberate contact).
- The dummy is boxes and a hull made from bounding boxes and sections of the STEP, so it is only as good as those.
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
- **USB-C window** (`usb_win_w`, `usb_win_h`, 13 x 7 mm). The user's OTG adapter is a plain standard USB-C male plug. The
  USB-IF compliance document ([Type-C Connectors and Cable Assemblies Compliance Document](https://www.usb.org/sites/default/files/USB_Type-C_Compliance_Document_rev_1_2.pdf)) allows
  a plug overmould of up to 12.35 x 6.5 mm (a maximum, not a fixed size; vendors differ), so 13 x 7 clears any compliant plug with 0.3 mm
  each side in width and 0.25 mm in height. The window cannot be much smaller than that: the socket itself is only 8.94 x 3.16 mm and
  sits flush with the board edge, 3.5 mm behind the outside of the wall, so the plug's overmould has to pass through the wall to seat
  the plug fully. If the actual plug is measured smaller, the window can shrink to that plus about 0.6 mm. Not tried with a real plug. The
  LCD flex wraps round the +X edge (photo): 1.3 mm is left on that side, not measured.
- **microSD window** (`sd_win_w`, `sd_win_h`, `sd_win_zc`): 11.6 x 2.2 mm, was 12.5 x 3.3. A microSD card is 11.0 x 15.0 x 1.0 mm. The
  holder on the board is a thin metal shell 1.85 mm tall (Z -1.85..0 in the Waveshare STEP), so the card lies at Z -0.93, centred on
  Y 19.15; the window is the card plus 0.3 mm each side in width, and the holder's own height plus 0.35 mm. The old window was also
  about 0.3 mm off the card's height. The card sits 4.6 mm behind the outside of the wall, so a push-push card pops out into the
  window and is taken out with a fingernail or tweezers; not tried with a real card.
- **Bezel width:** 9.0 mm is already the smallest equal bezel with the flex gap kept. The +X side sets it: the PCB reaches 5.46 mm
  past the active area there, the flex needs about 1.3 mm and the wall is 2.2 mm (5.46 + 1.3 + 2.2). `case.scad` stops with an error
  if `bezel_w` is made smaller than that. A thinner bezel needs a thinner +X wall or a measured, smaller flex gap.
- The 5.6 mm bosses leave 1.2 mm of plastic round a 3.2 mm insert hole; that is on the thin side for heat-set inserts. The bottom
  ones (6.4 mm) have more. A 3.0 mm insert, or a thread-forming M2 screw into a 1.8 mm hole, would give more margin.
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
