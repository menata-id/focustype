#!/bin/sh
# Usage: ./render.sh [classic|equal]   (no argument: both types)
# For one type: PNG views into <type>/renders/, STL into <type>/stl/, the clash and clearance checks, and the section drawings.
# Needs openscad (2021.01 is enough). The PNG views need a display: run under xvfb-run if there is none (they are skipped otherwise).
set -e
cd "$(dirname "$0")"
if [ $# -eq 0 ]; then "$0" classic && "$0" equal; exit $?; fi
T=$1
case "$T" in classic|equal) ;; *) echo "usage: $0 [classic|equal]"; exit 2;; esac
SCAD="$T/$T.scad"
mkdir -p "$T/renders" "$T/stl"
TMP=$(mktemp -d)
SIZE=1200,900
R() { # name part camera extra...
    name=$1; part=$2; cam=$3; shift 3
    openscad -o "$T/renders/$name.png" --imgsize=$SIZE --projection=p --camera="$cam" -D "part=\"$part\"" "$@" "$SCAD" 2>/dev/null
}
if [ -n "$DISPLAY" ] || command -v xvfb-run >/dev/null 2>&1; then
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
else
    echo "renders: PNG views skipped (no display and no xvfb-run)"
fi
for p in assembly exploded cutaway housing cover buttons buttons_print; do openscad -o "$T/stl/$p.stl" -D "part=\"$p\"" "$SCAD" 2>/dev/null; done

# Each check must come out empty (OpenSCAD says "empty" when the intersection is nothing). Extra arguments are passed on.
chk() { # label part extra...
    label=$1; part=$2; shift 2
    out=$(openscad -o "$TMP/chk.stl" -D "part=\"$part\"" "$@" "$SCAD" 2>&1 || true)
    if echo "$out" | grep -qi "empty"; then echo "[$T] check OK: $label"; else echo "[$T] check FAIL: $label"; echo "$out" | tail -5; exit 1; fi
}
chk "no clash between the case and the board (cell 18 x 65)" check
chk "nothing closer than 0.45 mm sideways to the board" check_gap
chk "the button pins do not touch the case" check_pins
chk "the button pins can be pushed in from the inside" check_pin_path
chk "the cover and its guide pins fit the housing" check_fit
chk "also with a cell of 18.6 x 70" check -D cell_d=18.6 -D cell_len=70

# Section drawings with the gaps measured (needs the venv with trimesh, networkx, shapely, matplotlib).
if [ -x .venv/bin/python ]; then
    .venv/bin/python sections.py "$T" > /dev/null && echo "[$T] sections: $T/renders/sections_front.png, sections_side.png, sections_plan.png, gaps.txt"
else
    echo "sections: skipped (no .venv with trimesh, networkx, shapely, matplotlib)"
fi
rm -rf "$TMP"
