// FocusType case, type 2: the bezel is the same on all four sides (bezel_w, 9 mm), the bottom included.
// The original case (wide bottom bezel with the wordmark) stays in case.scad and its stl/ files, unchanged.
// This file only switches case.scad to its equal-bezel mode; every other parameter is shared. Pick a part with
//   openscad -D 'part="housing"' case_equal.scad
include <case.scad>
equal_bezel = true;   // the last assignment wins in OpenSCAD, so this overrides the default in case.scad
