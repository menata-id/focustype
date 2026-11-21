# Case (CAD as code)

OpenSCAD model of a two-part case (front housing and back cover) for the Waveshare ESP32-S3-RLCD-4.2 (V2). It replaces the
acrylic back plate and stand. All dimensions are parameters at the top of `case.scad`; the sources of the board numbers
are in `ref/README.md`. There are **two types**, in their own folders, built from the same model:

| Type | Folder | Bezel |
|---|---|---|
| 1 `classic` | `classic/` | the look of the reference photo the user gave: 9 mm left, right and top, a wide 25 mm bottom bezel with the wordmark engraved in it, no screws on the front |
| 2 `equal` | `equal/` | 9 mm on all four sides (see "Type 2" below) |

```
case.scad            the shared model (every parameter); not used directly
classic/classic.scad type 1: includes ../case.scad          classic/stl, classic/step, classic/renders
equal/equal.scad     type 2: includes ../case.scad, sets equal_bezel = true     equal/stl, equal/step, equal/renders
render.sh [type]     checks, STL, PNG views and section drawings for one type (no argument: both)
render_views.py [type]  seven coloured PNG views offscreen (VTK), when there is no display for OpenSCAD's own PNGs
sections.py [type]   the section drawings and gaps.txt (called by render.sh)
stl2step.py [type]   the printed parts as STEP, <type>/step/*.stp (cadquery, see "STEP files")
```

```
./render.sh equal                                   # one type; ./render.sh does both
cd equal && openscad -D 'part="housing"' equal.scad # one part: housing, cover, buttons, board, assembly, section, check
```

Needs `openscad` (2021.01 is enough). The PNG views need a display (under `xvfb-run` if headless; `render.sh` skips them otherwise). The section drawings (`sections.py`) and
the STEP files (`stl2step.py`) need a venv at `case/.venv` with `trimesh networkx shapely matplotlib cadquery`; `render.sh` skips
the sections if it is missing. The venv is not committed. The rest of this file describes both types except where it says otherwise;
paths like `stl/` and `renders/` are inside the type's folder.

## Parts and how they go together

| Part | What |
|---|---|
| `housing` | front frame and walls in one piece, printed **face down** so the front has no seams or screws. Window = LCD active area + 0.3 mm. Bezel 9.0 mm on the left, right and top (`bezel_w`), 25 mm on the bottom (`bottom_w`), both measured from the edge of the active area (this is type `classic`; `equal` has 9 mm on all four sides). On `classic` only, "FocusType_" is engraved 0.4 mm into the bottom bezel; `equal` has no engraving. Six bosses with 1.8 mm pilot holes take the cover screws (M2 x 6 thread-forming, no inserts); they are kept at least 0.6 mm from the board (5.6 mm wide on the narrow edges, 6.4 mm in the wide band). Windows for USB-C, microSD (card enters from the +X edge) and tunnels for the three side buttons |
| `cover` | flat back plate with a closed pocket for the 18650 cell: the cell lies on the PCB and sticks out about 4 mm past the holder frame, so the pocket is a half tube that follows the cell and the holder outline, 19.1 mm deep inside (see Battery below). Four countersunk M2.5 screws from the back go into the board's own four standoffs, as the Waveshare acrylic plate does (`board_screws`). Six countersunk M2 screws go into the housing pilot holes. Window for the 2x9 header (UART0 pins stay reachable) |
| `buttons` | three printed pins for the BOOT, PWR and KEY switches on the narrow top edge (see Buttons below) |
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
cover is on the back face; pilot holes sit deep in bosses far from the front surface so they cannot mark it; the board is not
clamped by the front frame, there is a 0.3 mm gap to the glass (a thin foam strip on the glass edge is better).

## Buttons

The three side switches (BOOT, PWR, KEY) sit in a notch of the board: the +Y edge of the PCB is cut back 1.0 mm between X -15 and 15
(`pcb_notch_x`, `pcb_notch_d`; from the Waveshare STEP, and visible in the photos), and each switch has a plunger 2.0 x 1.0 mm, centred
on Z -0.9, whose tip is at Y 34.36.
Each button is a printed pin (`buttons`, `button_pin()`) with **one cross-section from head to flange**: 7.0 x 5.0 mm with a 1.5 mm
corner radius (`btn_w`, `btn_h`, `btn_r`). It stands 0.8 mm out of the wall, runs through a rounded tunnel in the wall (pin plus 0.25 mm
each side, radius 1.75), and has a flange behind the rib (9.0 x 7.5 mm, bigger than the hole) that stops it falling out, and a nose that
reaches to 0.05 mm from the plunger tip. **It goes in from the inside**, head first, before the board is fitted; the flange never has to
pass the hole. (An earlier version had a head bigger than the stem and could not be fitted from either side; `render.sh` now checks that
every position along the path of each pin into its tunnel is clear.)

**Thin rib, long nose, trim to fit.** The rib behind the wall (the reinforcement the flange rests on) was 3.9 mm thick; it is now 2.2 mm
(`y_rib` = 36.4), so the tunnel is 4.4 mm long and the nose between the flange and the switch is about 1.6 mm (flange 0.4 mm thick on `classic`; `equal` has 1.2 mm, grown into a rib slot, see Type 2). The printed pins
(`stl/buttons_print.stl`) have 1.0 mm of extra nose (`btn_stock`): fit the pin, see where the nose meets the plunger, and trim the nose
(blade or sandpaper) until it just clears it. That absorbs about +-0.5 mm of error in the board position and in the printed housing,
instead of the 0.3 mm the first design allowed. The cost is a shorter guide: the pin width is now 1.6 times the guide length, above the
rule of thumb of 1.2 (see below), so the button may bind a little when pressed at its edge. If it does, go to a narrower pin
(`btn_w` about 5.3 mm) or thicken the rib again (lower `y_rib`); the trim range then shrinks. `stl/buttons.stl` is the nominal pin used for
the checks and sections; `renders/button_detail.png` is a close-up section of the middle button.

**Fillet:** 1.5 mm on the pin and 1.75 mm on the hole (so the gap stays even all round), and the front edge of the head is rounded over
by 0.45 mm (`btn_edge`). 1.5 mm is 30% of the short side (5.0 mm): it still reads as a rectangle but is not sharp. Below about 0.6 mm the
rounding is lost at a 0.4 mm nozzle; above about 40% the button turns into a pill. The rounded top corners of the hole also help it print,
because the tunnel is horizontal when the housing is printed face down.

**How big it can be:**
- Width: the switches are 10 mm apart. With 1.2 mm of wall left between two holes (three perimeters) the pin can be at most about
  8.3 mm wide. 7.0 mm leaves 2.0 mm between the holes and 3.0 mm between the heads.
- Height: centred on the plunger, the hole must stay 1.5 mm above the cover seam (Z -7) and 1.35 mm below the chamfer of the front, which
  allows a pin up to about 8.7 mm tall.
- Smooth action: a pin guided over a tunnel only 5.6 mm long can bind if it is pushed off centre; as a rule of thumb the width should not be
  more than about 1.2 times the guide length, so about 7 mm. Hence 7.0 x 5.0 mm is the largest comfortable size here (area 35 mm2, 2.7 times
  the first 5.0 x 2.6 mm); 8 x 8 mm would fit but may bind and leaves heads only 2.0 mm apart. Ergonomic figures are general guidance
  (a fingertip pad is about 8-10 mm wide), not from a standard I read.

Print the pins with the head on the bed and the stem up; the flange then overhangs 0.75 mm each side (same for both flange thicknesses) and 1.0 mm top and bottom. Not
printed or tried: the feel, the travel (a side switch like this moves about 0.25 mm; the pin can move 0.05 mm in and out of its
stops) and whether the flange prints cleanly. `render.sh` checks that the pins do not touch the housing or the board.

## Assembly models

`stl/assembly.stl` (closed), `stl/exploded.stl` (parts pulled apart along Z, the pins slid out sideways) and `stl/cutaway.stl`
(the half with X <= 2 mm, to see the pocket, cell, foam and pins inside) are reference models for looking at on GitHub, not for printing.
They are one colour there; `renders/exploded.png` and `renders/cutaway.png` show the parts in colour.

## Foam, guide pins and cell cushions

(Guide pins: `classic` only. `equal` has `guide_pins = false`, see Type 2.)

How the board sits in the case, and what protects it (all added to the model; none of it printed or tried):

- **Foam seat and strip (`seat_depth`, `foam_t`, `foam_w`).** The underside of the front frame is relieved by 0.3 mm over the whole glass
  outline, which makes a pocket for an adhesive foam strip (closed-cell, for example PORON or EVA, 0.8 mm thick, 0.9 mm wide) stuck
  round the edge of the glass, outside the active area. The gap from the pocket roof to the glass becomes 0.6 mm, so the foam is squeezed
  from 0.8 to 0.6 mm (25 %). The frame never rests on the glass; the foam cushions a knock and keeps dust out. The model draws the
  strip as it sits, squeezed.
- **Guide pins (`pin_pos`, `pin_d`, `pin_hole_d`).** Two pins of 2.0 mm on the cover, 65 mm apart in the wide bottom band, go into 2.4 mm
  holes in two bosses of the housing (0.2 mm each side). The six cover screws have 0.2 mm of slack in their holes, so without the pins
  the cover, and the board hanging on it, could shift sideways relative to the front window. `render.sh` checks that the cover and its
  pins fit the housing.
- **Cell cushions (`pad_l`, `pad_w`, `pad_t`, `pad_x`).** Two foam pads of 10 x 8 mm sit in shallow seats in the pocket floor, 18 mm either
  side of the middle, under the cell, so it cannot hammer the holder contacts when the case is dropped. With a nominal 18 mm cell the
  1.5 mm foam is squeezed to 1.3 mm. With a 18.6 mm cell it would be squeezed to 0.7 mm (about half): use 1.0 mm foam then. The seats
  leave at least 1.0 mm of plastic under them.
- Not added: corner bumpers, and locating ribs for the PCB edge (the board is fixed by the cover screws and, on `classic`, the guide pins).

## Type 2: equal bezel on all four sides (`equal/`)

The original is type 1 (`classic/`, wide bottom bezel with the wordmark) and its geometry is unchanged (volume and bounding box of the housing
and cover compared before and after adding type 2). Type 2 is `equal/equal.scad`, which includes `../case.scad` and sets `equal_bezel = true`.
Its files are in `equal/stl`, `equal/step` and `equal/renders` (`compare_front.png` shows the front of both housings side by side).

**It is possible**, because the PCB is centred in Y: it reaches 2.76 mm past the active area at the bottom as well as at the top, and the top
already works with 9.0 mm. Outline 102.8 x 81.6 mm (type 1 is 102.8 x 97.6). What changes against type 1:
- Bottom bezel 9.0 mm (`bezel_w`), like left, right and top. The buttons stay on the +Y edge.
- The two bottom cover screws move to the edge, like the top ones: 5.6 mm bosses (not 6.4 mm) 0.6 mm from the PCB.
  There is no room left in the middle of a wide band, so they sit where the top screws sit.
- **No guide pins** (`guide_pins = false` in `equal/equal.scad`), for a simple first print: the cover has no pins, and the housing has no pin bosses or
  pin holes. The six screws locate the cover (they have 0.2 mm of slack, so line the cover up by eye or against the board before tightening). Set it
  to true to bring the pins back. `classic` is unchanged (it keeps its pins).
- **No wordmark** (`logo = ""` in `equal/equal.scad`). A 4 mm "FocusType_" fits the 7.9 mm flat bezel but its strokes are only about 0.6-0.8 mm
  (measured on the glyph shapes, not on a print), 1.5 to 2 nozzle widths, and the counters are smaller still, so it was dropped. To bring it back
  remove that line (the text is then size 4, `logo_size` in `case.scad`) and print a test bezel first. The housing is 19.7 mm3 heavier without it.
- **Thicker button flange** (`btn_fl_t = 1.2` in `equal/equal.scad`, 0.4 on `classic`): easier to print and stronger. The extra 0.8 mm grows into the rib, not towards the board: the flange's
  back face stays where it was, so the nose keeps its length and its 1.0 mm of trim stock, and one slot (`btn_slot`, 0.8 mm deep) in the rib face holds all three flanges
  (separate pockets would leave 0.5 mm walls). Host-checked only (clash, clearance, pin path); not printed.
- Everything else (window, walls, pocket, buttons, foam, cover) is shared with type 1.

Checked on the host only (nothing printed): the same clash, 0.45 mm clearance, pin, pin-path and cover-fit checks as type 1, and the
18.6 x 70 mm cell, all pass for type 2 (`./render.sh equal`). The cell pocket is under the PCB and about 32.5 mm from the centre, so it is not
affected by the narrower bottom. `equal/renders/sections_*.png` and `gaps.txt` are drawn for type 2 (the bottom cut is at Y = -38, through the
bosses and pins at the edge); the smallest gaps are the same as in type 1 (the changed area has 0.6 mm to the PCB, like the top).
The PNG views in `equal/renders/` (`assembly_front`, `assembly_back`, `housing_front`, `housing_back`, `cover_back`, `exploded`, `cutaway`) were
drawn without a display by `render_views.py` (VTK, from the STL and the board, cell and foam parts), not by OpenSCAD, so the camera angles differ a
little from OpenSCAD's and the cell's cut face shows some triangulation lines. The section drawings and `compare_front.png` are there too.
Not verified: a real print.

## STEP files

`<type>/step/housing.stp`, `cover.stp` and `buttons_print.stp` (the printed parts) are made from the STL files by `stl2step.py`, for slicers and
CAD tools that want STEP. **They are not real CAD models**: an STL is a triangle mesh, so every triangle is one STEP face (about 6000 in a
housing), holes and corner radii stay polygons, and you cannot pick a flat face or change a dimension. Change the design in `case.scad`
and run the script again. Checked by reading the files back (cadquery): valid solids, volume and bounding box equal to the STL
(`buttons_print` is three solids, one per pin). Not opened in a slicer or a CAD program. The files are large (9 to 14 MB each, about 52 MB
in all) because of the faceting. Merging coplanar triangles (`ShapeUpgrade_UnifySameDomain`) was tried and crashes OpenCASCADE 7.9 on
these meshes. Four zero-area triangles that the STL had are dropped (they are on straight edges; the volume does not change).

## Wordmark

Type 1 (`classic`) only: type 2 (`equal`) has none, see above. The mockup header uses **IBM Plex Sans, weight 700 (Bold), letter-spacing 0.04 em** for "FocusType" (see the mockup's
Home screen). The case uses the same: `logo_font = "IBM Plex Sans:style=Bold"`, `spacing = 1.04`, loaded from
`../third_party/ibm-plex/IBMPlexSans-Bold.ttf` so it does not depend on installed fonts. The underscore is not in the mockup.

## What has actually been checked

- **Done (on the host only, nothing printed or fitted):** the model renders. `./render.sh` runs six checks per type and stops on a failure:
  no intersection between the case and the board dummy (cell 18 x 65), no part of the case closer than 0.45 mm sideways to the board
  (this is what caught the top bosses at 0.24 mm from the PCB and the button rib at 0.34 mm, both now 0.6 mm or more), and a
  worst-case protected cell (18.6 x 70) still fits. The clash check was shown to be able to fail: with `-D clear=-1` it reports a clash.
- **Section drawings** (`sections.py`, written to `renders/`): `sections_front.png` (cuts along the long side seen from the front,
  like the section in the mockup), `sections_side.png` (cuts across it, including the guide pin (`classic` only) and a cell cushion) and `sections_plan.png` (flat cuts at six depths). Grey is the
  case, green the board, orange the cell, pink the foam, and the smallest distances between them are labelled in mm; `gaps.txt` has the same numbers.
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
- `classic` only (`equal` has no engraving): the engraving prints over air when the housing is printed face down (the letters are small bridges). Check it on the
  first print; the alternative is to print it standing up or leave `logo = ""`.
- Pilot hole `pilot_d = 1.8` for M2 thread-forming screws and the screw length (M2 x 6: 2.0 mm cover + about 4 mm in the boss) are an estimate,
  not tested: print one test boss first. The battery is rechargeable and the cover is rarely opened, so threads cut in plastic are acceptable. Prefer PETG or ABS over PLA. Printer tolerances (`clear`, `win_gap`) are guesses: print a
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
- The 5.6 mm bosses leave 1.9 mm of plastic round a 1.8 mm pilot hole (the earlier 3.2 mm heat-set insert hole left only 1.2 mm).
  Do not overtighten: threads cut in plastic strip easily.
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
