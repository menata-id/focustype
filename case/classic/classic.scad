// FocusType case, type 1 (classic): wide bottom bezel (25 mm) with the wordmark; left, right and top 9 mm.
// The model is in ../case.scad, shared with type 2 (../equal/equal.scad). Pick a part with
//   openscad -D 'part="housing"' classic.scad
include <../case.scad>
equal_bezel = false;   // the default in case.scad; set again here so the type is explicit
