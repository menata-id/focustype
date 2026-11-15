// FocusType case for the Waveshare ESP32-S3-RLCD-4.2 (V2). Replaces the acrylic back plate and stand.
// Units: mm. Read case/ref/README.md first: every board number below comes from the Waveshare STEP/DXF unless marked ASSUMED.
//
// Two printed parts:
//   housing  front frame and walls in one piece (print face down: the front has no seams or screws);
//   cover    back plate with a pocket for the battery; 6 M2 screws go from the back into heat-set inserts in the
//            housing bosses, so nothing shows at the front. The board is not screwed to anything: it is clamped
//            between the front frame (through a thin foam strip on the glass edge) and the cover, which presses on
//            the ends of the board's 4 standoffs. Take the cover off and the battery holder is open.
// Frame: X = long side (USB-C at +X), Y = short side (buttons on the +Y edge), Z = thickness, front of the LCD at +Z.
// Z = 0 is the back face of the PCB. Pick a part with  openscad -D 'part="housing"' case.scad
//   housing, cover, board (board and cell, not printed), assembly, section, section_battery, plan, check, check_gap

use <../third_party/ibm-plex/IBMPlexSans-Bold.ttf>   // the wordmark font; not installed system-wide, so load the file

part = "assembly";
gap_min = 0.45;       // smallest sideways gap allowed between the case and the board (check_gap)
$fn = 32;

// ---- board (from the STEP/DXF; see case/ref/README.md) ----
pcb_x0 = -46.26; pcb_x1 = 46.26;      // 92.52 long
pcb_y0 = -34.56; pcb_y1 = 34.56;      // 69.12 wide
pcb_t  = 1.6;
glass_x = 91.0;  glass_y = 67.6;      // centred on the PCB
lcd_front = 4.35;                     // LCD front face above the PCB back (V1 would be 3.75)
aa_x0 = -44.0; aa_x1 = 40.8;          // active area 84.8 x 63.6; the FPC / USB-C end has the wide margin
aa_y0 = -31.8; aa_y1 = 31.8;
stand_xy = [42.75, 31.05];            // 4 x M2.5 standoffs, 85.5 x 62.1 apart, d 5.5, PCB back to -7.0
stand_end = -7.0;
bat_x = 38.9; bat_y0 = -29.95; bat_y1 = -8.85; bat_depth = 15.2;  // 18650 holder: a frame on the PCB, walls 15.2 mm tall
// The 18650 cell itself (Waveshare says only "standard 18650 rechargeable lithium"; no size, no protection board stated).
// Size from the user: type 18650, a cylinder 18 mm in diameter and 65 mm long. (Real cells with their wrap often measure 18.3-18.6 mm
// and protected ones are longer; bay_clear below leaves 0.8 mm.) The cell lies on the PCB, so it sticks out past the frame rim by
// about cell_d - bat_depth. Measure the cell you actually fit.
cell_d = 18.0; cell_len = 65.0; cell_y = (bat_y0 + bat_y1) / 2;
// Outline of the holder frame (half width across Y, Z) read from sections of the Waveshare STEP; the pocket has to clear it.
frame_prof = [[10.55, -4], [10.4, -6], [10.2, -8], [9.68, -10], [9.15, -12], [8.63, -14], [8.38, -15], [8.3, -15.2]];
// The pocket is sized for the worst case, not the nominal cell: 18.6 mm is what an 18650 with its wrap can measure, plus 0.5 mm
// all round for printing tolerance and for the cell swelling a little over its life.
cell_dmax = 18.6; pocket_clear = 0.5;
usb_y = 0; usb_w = 9.6; usb_z0 = -3.25; usb_z1 = 0.91;            // on the +X edge, centred
sd_x0 = 30.21; sd_y0 = 11.1; sd_y1 = 27.2; sd_z0 = -1.85; sd_z1 = 0.6;  // card enters from +X, contacts toward the LCD
btn_x = [-10, 0, 10]; btn_y0 = 30.71; btn_y1 = 34.36; btn_z0 = -1.8; btn_z1 = 0.4; btn_len = 4.56;  // on the +Y edge
hdr_x0 = -10.6; hdr_x1 = 10.4; hdr_y0 = -4.0; hdr_y1 = 2.6; hdr_z0 = -8.7;  // 2x9 header

// ---- look (from the reference photo) ----
bezel_w  = 9.0;       // active-area edge to the outside, left / right / top. Must stay >= about 8.8: the +X wall needs room for the flex.
bottom_w = 25.0;      // active-area edge to the outside, wide bottom bezel
bottom_neg = true;    // true: the wide bezel is on the -Y edge (battery side), the buttons end up on the narrow top edge. false: the other way.
corner_r = 3.0;       // plan-view corner radius: just takes the edge off, still reads as a rectangle
edge_front = 0.8;     // 45 degree chamfer round the front face (a chamfer prints cleaner than a fillet when printed face down)
edge_back = 1.0;      // same, round the back face of the cover
logo = "FocusType_";  // engraved 0.4 mm into the bottom bezel; "" for none. Face-down printing bridges the engraving: check the print.
logo_font = "IBM Plex Sans:style=Bold";   // the mockup header wordmark: IBM Plex Sans, weight 700, letter-spacing 0.04 em
logo_size = 6;

// ---- build choices ----
wall = 2.2;           // housing wall
plate_t = 1.5;        // front frame thickness
glass_gap = 0.3;      // between the LCD glass and the underside of the front frame (a thin foam strip on the glass edge is better)
win_gap = 0.3;        // window larger than the active area
cover_t = 2.0;
boss_d = 5.6; boss_d_wide = 6.4;   // boss diameters: 5.6 is all that fits between the board and the outside on the narrow edges (about 0.6 mm to the PCB)
insert_d = 3.2; insert_depth = 5.0;   // M2 heat-set inserts: check the hole size against the insert datasheet; the wall round an insert is 1.2 mm on the narrow edges
board_screws = true;  // 4 x M2.5 from the back of the cover into the board's own standoffs, as the Waveshare acrylic plate does
bay_clear = 0.8; bay_wall = 2.0;   // bay_clear: end clearance along the cell; bay_wall: pocket wall
usb_win_w = 13; usb_win_h = 7;   // window for the plug overmould, ASSUMED: measure the plug you will use and add 1 mm each way

rim_z = stand_end;                    // housing rim and cover inner face: the standoffs end here too
cover_z = rim_z - cover_t;            // back face, -9.0
top_z = lcd_front + glass_gap;        // underside of the front frame

out_x0 = aa_x0 - bezel_w; out_x1 = aa_x1 + bezel_w;
out_y0 = aa_y0 - (bottom_neg ? bottom_w : bezel_w);
out_y1 = aa_y1 + (bottom_neg ? bezel_w : bottom_w);
in_x0 = out_x0 + wall; in_x1 = out_x1 - wall;
in_y0 = out_y0 + wall; in_y1 = out_y1 - wall;
bay_x = bat_x + bay_clear;     // half length of the pocket along X: the holder (end contacts) plus clearance

// the 6 cover screws [x, y, boss diameter]: 2 in the left wall, 2 on the narrow edge, 2 in the wide band. None near the board.
y_narrow = bottom_neg ? out_y1 - boss_d / 2 : out_y0 + boss_d / 2;
y_wide = bottom_neg ? (pcb_y0 + out_y0) / 2 : (pcb_y1 + out_y1) / 2;
screws = [[out_x0 + boss_d / 2, 22, boss_d], [out_x0 + boss_d / 2, -22, boss_d],
          [-20, y_narrow, boss_d], [in_x1 - boss_d / 2, y_narrow, boss_d],
          [-20, y_wide, boss_d_wide], [in_x1 - boss_d_wide / 2, y_wide, boss_d_wide]];   // the right ones touch the +X wall

// The +X wall must leave room for the flex that wraps round that edge: about 1.3 mm from the PCB edge (ASSUMED, from the photo).
// That, not the look, sets the smallest equal bezel: 5.46 (PCB beyond the active area) + 1.3 + wall = about 9 mm.
assert(in_x1 - pcb_x1 >= 1.3, "bezel_w is too small: less than 1.3 mm between the PCB edge and the +X wall");

module rbox(x0, x1, y0, y1, z0, z1, r = 0.5) {
    translate([0, 0, z0]) linear_extrude(z1 - z0)
        hull() for (x = [x0 + r, x1 - r], y = [y0 + r, y1 - r]) translate([x, y]) circle(r);
}

// Rounded rectangle in plan, chamfered at the bottom (cb) and top (ct) edges; the corner radius shrinks with the chamfer.
module cbox(x0, x1, y0, y1, z0, z1, r, cb = 0, ct = 0) {
    module slab(z, h, i) translate([0, 0, z]) linear_extrude(h)
        hull() for (x = [x0 + r, x1 - r], y = [y0 + r, y1 - r]) translate([x, y]) circle(max(r - i, 0.01));
    hull() {
        slab(z0, 0.01, cb);
        slab(z0 + cb, 0.01, 0);
        slab(z1 - ct - 0.01, 0.01, 0);
        slab(z1 - 0.01, 0.01, ct);
    }
}

// Rounded window through a wall that faces +X: centre (yc, zc), width w along Y, height h along Z, from x0 for len.
module xwindow(x0, len, yc, zc, w, h, r) {
    hull() for (dy = [-(w / 2 - r), w / 2 - r], dz = [-(h / 2 - r), h / 2 - r])
        translate([x0, yc + dy, zc + dz]) rotate([0, 90, 0]) cylinder(r = r, h = len);
}

module housing() {
    difference() {
        union() {
            difference() {
                cbox(out_x0, out_x1, out_y0, out_y1, rim_z, top_z + plate_t, corner_r, 0, edge_front);
                rbox(in_x0, in_x1, in_y0, in_y1, rim_z - 1, top_z, max(corner_r - wall, 0.5));
            }
            for (s = screws) translate([s[0], s[1], rim_z]) cylinder(d = s[2], h = top_z - rim_z + 0.01);
            // rib that carries the button windows to the board edge (the narrow wall is further out than the buttons)
            translate([-14, pcb_y1 + 0.6, rim_z]) cube([28, in_y1 - pcb_y1 - 0.6 + 0.5, top_z - rim_z]);
        }
        // window: the active area plus a gap, front frame only
        translate([0, 0, top_z - 1]) rbox(aa_x0 - win_gap, aa_x1 + win_gap, aa_y0 - win_gap, aa_y1 + win_gap, 0, plate_t + 2, 0.5);
        for (s = screws) translate([s[0], s[1], rim_z - 1]) cylinder(d = insert_d, h = insert_depth + 1);   // inserts go in from the back
        // USB-C: the socket is only 8.94 x 3.16 and sits flush with the board edge, 3.5 mm behind the outside face, so the
        // plug overmould has to pass through the wall: the window is sized for the overmould, not the socket
        xwindow(in_x1 - 1, wall + 2, usb_y, (usb_z0 + usb_z1) / 2, usb_win_w, usb_win_h, 2.0);
        // microSD: slot for the card to enter and be pinched out
        xwindow(in_x1 - 1, wall + 2, (sd_y0 + sd_y1) / 2, (sd_z0 + sd_z1) / 2, 12.5, 3.3, 1.0);
        // three side buttons: tunnels, to be fitted with printed pins (ASSUMED, see README)
        for (x = btn_x) translate([x - 3.2, pcb_y1 + 0.2, (btn_z0 + btn_z1) / 2 - 1.7]) cube([6.4, out_y1 - pcb_y1, 3.4]);
        if (logo != "")
            translate([(out_x0 + out_x1) / 2, bottom_neg ? (aa_y0 - win_gap + out_y0) / 2 : (aa_y1 + win_gap + out_y1) / 2, top_z + plate_t - 0.4])
                linear_extrude(1) text(logo, size = logo_size, font = logo_font, spacing = 1.04, halign = "center", valign = "center");
    }
}

// Cross-section of the battery pocket (u = across the board, v = Z): a round bottom around the worst-case cell, widening up to
// the outline of the holder frame (plus clearance) so that the frame fits in as well.
module pocket2d() {
    r = cell_dmax / 2 + pocket_clear;
    hull() {
        translate([0, -cell_dmax / 2]) circle(r, $fn = 72);
        for (p = frame_prof, sx = [-1, 1]) translate([sx * p[0], p[1]]) circle(pocket_clear, $fn = 24);   // frame outline + pocket_clear all round
    }
}
// Extrude a 2D section along X, centred on the cell axis (cell_y), length len.
module along_x(len) multmatrix([[0, 0, 1, 0], [1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 0, 1]]) translate([0, 0, -len / 2]) linear_extrude(len) children();

module cover() {
    difference() {
        union() {
            cbox(out_x0, out_x1, out_y0, out_y1, cover_z, rim_z, corner_r, edge_back, 0);
            // battery pocket on the back: the pocket section grown by the wall, and cut off at the cover's inner face
            intersection() {
                translate([0, cell_y, 0]) along_x(2 * (bay_x + bay_wall)) offset(r = bay_wall, $fn = 48) pocket2d();
                translate([-100, -100, -100]) cube([200, 200, 100 + rim_z]);
            }
        }
        translate([0, cell_y, 0]) along_x(2 * bay_x) pocket2d();   // the pocket itself, open to the inside
        translate([hdr_x0 - 1, hdr_y0 - 1, cover_z - 1]) cube([hdr_x1 - hdr_x0 + 2, hdr_y1 - hdr_y0 + 2, cover_t + 2]);  // header window
        if (board_screws) for (sx = [-1, 1], sy = [-1, 1]) {                             // M2.5 countersunk screws into the board's 4 standoffs
            translate([sx * stand_xy[0], sy * stand_xy[1], cover_z - 1]) cylinder(d = 2.9, h = cover_t + 2);
            translate([sx * stand_xy[0], sy * stand_xy[1], cover_z - 0.01]) cylinder(d1 = 5.4, d2 = 2.9, h = 1.5);
            translate([sx * stand_xy[0], sy * stand_xy[1], cover_z - 15]) cylinder(d = 4.4, h = 15);   // driver access where the pocket's end shoulder is in the way
        }
        for (s = screws) {                                                               // M2 countersunk screws into the housing inserts
            translate([s[0], s[1], cover_z - 1]) cylinder(d = 2.4, h = cover_t + 2);
            translate([s[0], s[1], cover_z - 0.01]) cylinder(d1 = 4.4, d2 = 2.4, h = 1.1);
        }
    }
}

// Box grown by g in X and Y only (used to test clearance round the board).
module gcube(p, sz, g) translate([p[0] - g, p[1] - g, p[2]]) cube([sz[0] + 2 * g, sz[1] + 2 * g, sz[2]]);

// The 18650 cell lying on the back of the PCB, axis along X.
module battery(g = 0) {
    translate([-cell_len / 2, cell_y, -cell_d / 2]) rotate([0, 90, 0]) cylinder(d = cell_d + 2 * g, h = cell_len, $fn = 64);
}

// Reference dummy of the real board without the cell. g grows every part sideways by g (clearance check).
module board_parts(g = 0) {
    gcube([pcb_x0, pcb_y0, 0], [pcb_x1 - pcb_x0, pcb_y1 - pcb_y0, pcb_t], g);
    gcube([-glass_x / 2, -glass_y / 2, pcb_t], [glass_x, glass_y, lcd_front - pcb_t], g);
    for (sx = [-1, 1], sy = [-1, 1]) translate([sx * stand_xy[0], sy * stand_xy[1], stand_end + 0.01]) cylinder(d = 5.5 + 2 * g, h = -stand_end - 0.01);
    // holder frame: the real outline (it narrows towards the rim), not a box
    translate([0, cell_y, 0]) along_x(2 * bat_x) hull()
        for (p = concat([[frame_prof[0][0], 0]], frame_prof), sx = [-1, 1]) translate([sx * p[0], p[1]]) circle(0.01 + g, $fn = 8);
    gcube([38.74, usb_y - usb_w / 2, usb_z0], [46.27 - 38.74, usb_w, usb_z1 - usb_z0], g);
    gcube([sd_x0, sd_y0, sd_z0], [45.22 - sd_x0, sd_y1 - sd_y0, sd_z1 - sd_z0], g);
    for (x = btn_x) gcube([x - btn_len / 2, btn_y0, btn_z0], [btn_len, btn_y1 - btn_y0, btn_z1 - btn_z0], g);
    gcube([hdr_x0, hdr_y0, hdr_z0], [hdr_x1 - hdr_x0, hdr_y1 - hdr_y0, -hdr_z0], g);
}

// Board with its cell: for looking and for the clash check. Not part of the print.
module board() { board_parts(); battery(); }

module assembly() {
    color("DimGray") housing();
    color("Silver") cover();
    color("SeaGreen") board_parts();
    color("Orange") battery();
}

if (part == "housing") housing();
else if (part == "cover") cover();
else if (part == "board") board();
else if (part == "board_parts") board_parts();
else if (part == "battery") battery();
else if (part == "section_battery") intersection() { assembly(); translate([-100, -100, -100]) cube([100, 200, 200]); }   // cut across the cell, X <= 0
else if (part == "section") intersection() { assembly(); translate([-100, -100, -100]) cube([200, 100 + 12, 200]); }
else if (part == "check_gap") intersection() { union() { housing(); cover(); } board_parts(gap_min); }   // must be empty: nothing closer than gap_min sideways
else if (part == "plan") intersection() { assembly(); translate([-100, -100, 0.2]) cube([200, 200, 1.0]); }    // slice through the PCB, seen from the front
else if (part == "check") intersection() { union() { housing(); cover(); } board(); }   // must be empty
else assembly();
