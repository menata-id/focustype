#!/bin/sh
# Render PNG views into case/renders/, export STL into case/stl/, and run the clash check.
# Needs openscad (2021.01 is enough). Headless: run under xvfb-run if there is no display.
set -e
cd "$(dirname "$0")"
mkdir -p renders stl
SIZE=1200,900
R() { # name part camera extra...
    name=$1; part=$2; cam=$3; shift 3
    openscad -o "renders/$name.png" --imgsize=$SIZE --projection=p --camera="$cam" -D "part=\"$part\"" "$@" case.scad 2>/dev/null
}
R assembly_front assembly 0,0,0,55,0,25,380
R assembly_back  assembly 0,0,-5,125,0,205,380
R cover_back     cover    0,0,-5,125,0,205,380
R housing_back   housing  0,0,0,125,0,205,380
R housing_front  housing  0,0,0,50,0,20,380
R board          board    0,0,-5,125,0,205,330
R section        section  0,12,-5,90,0,180,300
R section_battery section_battery -10,-19,-8,90,0,90,300
R board_back     board    0,0,-8,125,0,205,300
R plan           plan     0,-8,0,0,0,0,250
R exploded       exploded 0,0,10,70,0,25,520
R cutaway        cutaway  -10,-10,-2,65,0,60,330
R buttons_close  assembly 0,36,-1,75,0,200,90
for p in assembly exploded cutaway; do openscad -o "stl/$p.stl" -D "part=\"$p\"" case.scad 2>/dev/null; done
for p in housing cover buttons buttons_print; do openscad -o "stl/$p.stl" -D "part=\"$p\"" case.scad 2>/dev/null; done

# Clash check: the case must not intersect the board dummy. An empty result means no clash.
out=$(openscad -o /tmp/focustype_check.stl -D 'part="check"' case.scad 2>&1 || true)
if echo "$out" | grep -qi "empty"; then
    echo "check: OK, no clash between the case and the board (cell 18 x 65)"
else
    echo "check: FAIL, the case touches the board:"; echo "$out" | tail -5; exit 1
fi
# Clearance: no part of the case closer than 0.45 mm sideways to the board (bosses, rib, pocket).
out=$(openscad -o /tmp/focustype_gap.stl -D 'part="check_gap"' case.scad 2>&1 || true)
if echo "$out" | grep -qi "empty"; then
    echo "check: OK, nothing closer than 0.45 mm to the board"
else
    echo "check: FAIL, something is closer than 0.45 mm to the board:"; echo "$out" | tail -5; exit 1
fi
# The pins must not touch the housing or the cover (they slide in their tunnels).
out=$(openscad -o /tmp/focustype_check3.stl -D 'part="check_pins"' case.scad 2>&1 || true)
if echo "$out" | grep -qi "empty"; then
    echo "check: OK, the button pins do not touch the case"
else
    echo "check: FAIL, a button pin touches the case:"; echo "$out" | tail -5; exit 1
fi
# A pin must be able to go in from the inside: every position along its path to the tunnel is clear of the case.
out=$(openscad -o /tmp/focustype_check5.stl -D 'part="check_pin_path"' case.scad 2>&1 || true)
if echo "$out" | grep -qi "empty"; then
    echo "check: OK, the button pins can be pushed in from the inside"
else
    echo "check: FAIL, a button pin is blocked on its way in:"; echo "$out" | tail -5; exit 1
fi
# The cover and its guide pins must fit the housing (the faces at the rim only touch).
out=$(openscad -o /tmp/focustype_check4.stl -D 'part="check_fit"' case.scad 2>&1 || true)
if echo "$out" | grep -qi "empty"; then
    echo "check: OK, the cover and its guide pins fit the housing"
else
    echo "check: FAIL, the cover and the housing overlap:"; echo "$out" | tail -5; exit 1
fi
# Worst case: a protected cell, 18.6 mm x 70 mm, must also fit the pocket.
out=$(openscad -o /tmp/focustype_check2.stl -D 'part="check"' -D cell_d=18.6 -D cell_len=70 case.scad 2>&1 || true)
if echo "$out" | grep -qi "empty"; then
    echo "check: OK, also with a cell of 18.6 x 70"
else
    echo "check: FAIL, a cell of 18.6 x 70 does not fit:"; echo "$out" | tail -5; exit 1
fi

# Section drawings with the gaps measured (needs the venv with trimesh, networkx, shapely, matplotlib).
if [ -x .venv/bin/python ]; then
    .venv/bin/python sections.py > /dev/null && echo "sections: renders/sections_front.png, sections_side.png, sections_plan.png, gaps.txt"
else
    echo "sections: skipped (no .venv with trimesh, networkx, shapely, matplotlib)"
fi
