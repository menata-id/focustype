// FocusType case, type 2 (equal): the bezel is the same on all four sides (bezel_w, 9 mm), the bottom included.
// The model is in ../case.scad, shared with type 1 (../classic/classic.scad). Pick a part with
//   openscad -D 'part="housing"' equal.scad
include <../case.scad>
equal_bezel = true;   // the last assignment wins in OpenSCAD, so this overrides the default in case.scad
logo = "";            // no wordmark on this type: the 7.9 mm flat bezel leaves a 4 mm text only 0.6-0.8 mm strokes, too fine to rely on printing
