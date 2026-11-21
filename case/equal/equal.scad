// FocusType case, type 2 (equal): the bezel is the same on all four sides (bezel_w, 9 mm), the bottom included.
// The model is in ../case.scad, shared with type 1 (../classic/classic.scad). Pick a part with
//   openscad -D 'part="housing"' equal.scad
include <../case.scad>
equal_bezel = true;   // the last assignment wins in OpenSCAD, so this overrides the default in case.scad
