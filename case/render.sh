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
R assembly_front assembly 0,0,0,55,0,25,330
R assembly_back  assembly 0,0,-5,125,0,205,330
R shell_back     shell    0,0,-5,125,0,205,330
R shell_front    shell    0,0,0,55,0,25,330
R bezel          bezel    0,0,0,55,0,25,330
R board          board    0,0,-5,125,0,205,330
R section        section  0,12,-5,90,0,180,260
for p in shell bezel lid; do openscad -o "stl/$p.stl" -D "part=\"$p\"" case.scad 2>/dev/null; done

# Clash check: the case must not intersect the board dummy. An empty result means no clash.
out=$(openscad -o /tmp/focustype_check.stl -D 'part="check"' case.scad 2>&1 || true)
if echo "$out" | grep -qi "empty"; then
    echo "check: OK, no clash between the case and the board"
else
    echo "check: FAIL, the case touches the board:"; echo "$out" | tail -5; exit 1
fi
