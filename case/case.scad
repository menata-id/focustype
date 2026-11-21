// FocusType case for the Waveshare ESP32-S3-RLCD-4.2 (V2). Replaces the acrylic back plate and stand.
// Units: mm. Read case/ref/README.md first: every board number below comes from the Waveshare STEP/DXF unless marked ASSUMED.
//
// Two printed parts:
//   housing  front frame and walls in one piece (print face down: the front has no seams or screws);
//   cover    back plate with a pocket for the battery; 6 M2 thread-forming screws go from the back into pilot holes in the
//            housing bosses, so nothing shows at the front. The board is not screwed to anything: it is clamped
//            between the front frame (through a thin foam strip on the glass edge) and the cover, which presses on
//            the ends of the board's 4 standoffs. Take the cover off and the battery holder is open.
// Frame: X = long side (USB-C at +X), Y = short side (buttons on the +Y edge), Z = thickness, front of the LCD at +Z.
// Z = 0 is the back face of the PCB. Pick a part with  openscad -D 'part="housing"' case.scad
//   assembly, exploded, cutaway (reference models, not printed); housing, cover, buttons (3 pins), board (board and cell, not printed), assembly, section, section_battery, plan, check, check_gap

use <../third_party/ibm-plex/IBMPlexSans-Bold.ttf>   // the wordmark font; not installed system-wide, so load the file

part = "assembly";
equal_bezel = false;  // false: the original case (wide bottom bezel, 25 mm). true: the second type, the bottom bezel as wide as the others (bezel_w, 9 mm)
gap_min = 0.45;       // smallest sideways gap allowed between the case and the board (check_gap)
$fn = 32;

// ---- board (from the STEP/DXF; see case/ref/README.md) ----
pcb_x0 = -46.26; pcb_x1 = 46.26;      // 92.52 long
pcb_y0 = -34.56; pcb_y1 = 34.56;      // 69.12 wide
pcb_t  = 1.6;
pcb_notch_x = 15; pcb_notch_d = 1.0;   // the +Y edge is cut back 1.0 mm between X -15 and 15, where the three switches sit (Waveshare STEP; visible in the photos)
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
// The three side switches (BOOT, PWR, KEY): body 4.55 wide to Y 33.3, then a plunger 2.0 x 1.0 mm centred on Z -0.9 whose tip is at Y 34.36 (STEP).
btn_zc = -0.9; btn_tip_y = 34.36; btn_body_y1 = 33.3;
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
logo_size = equal_bezel ? 4.0 : 6;   // the equal-bezel type has 7.9 mm of flat bezel, not 22 mm: the wordmark is smaller

// ---- build choices ----
wall = 2.2;           // housing wall
plate_t = 1.5;        // front frame thickness
glass_gap = 0.3;      // between the LCD glass and the underside of the front frame (a thin foam strip on the glass edge is better)
win_gap = 0.3;        // window larger than the active area
cover_t = 2.0;
boss_d = 5.6; boss_d_wide = 6.4;   // boss diameters: 5.6 is all that fits between the board and the outside on the narrow edges (about 0.6 mm to the PCB)
pilot_d = 1.8; pilot_depth = 5.0;   // M2 thread-forming screws (M2 x 6) go straight into the plastic, no inserts; the wall round a hole is 1.9 mm on the narrow edges. Print a test boss: 1.8 is a typical value, adjust to the printer and filament
foam_t = 0.8; foam_w = 0.9;   // adhesive foam strip round the glass (closed-cell, e.g. PORON or EVA): 0.8 mm thick, 0.9 mm wide
seat_depth = 0.3;             // the front frame is relieved by this over the glass: the gap to the glass becomes glass_gap + seat_depth = 0.6 mm, so the foam is squeezed from 0.8 to 0.6 mm (25 %)
pin_d = 2.0; pin_h = 2.5;     // two guide pins on the cover ...
pin_hole_d = 2.4; pin_hole_depth = 3.0; pin_boss_d = 5.0;   // ... into holes in two bosses of the housing (0.2 mm each side)
pad_l = 10; pad_w = 8; pad_t = 1.5; pad_seat = 0.2; pad_x = [-18, 18];   // two foam cushions under the cell, 10 x 8 x 1.5 mm, in shallow seats in the pocket floor
board_screws = true;  // 4 x M2.5 from the back of the cover into the board's own standoffs, as the Waveshare acrylic plate does
bay_clear = 0.8; bay_wall = 2.0;   // bay_clear: end clearance along the cell; bay_wall: pocket wall
// microSD window: the card is 11.0 x 15.0 x 1.0 mm (SD Association). The holder is a thin metal shell 1.85 mm tall (Z -1.85..0 in the STEP), so the
// card lies at Z -0.93, centred on Y 19.15. Window = card + 0.3 mm each side in width, and the holder's own height + 0.35 mm.
sd_win_w = 11.6; sd_win_h = 2.2; sd_win_zc = -0.93;
usb_win_w = 13; usb_win_h = 7;   // USB-C plug overmould: the USB-IF compliance document allows up to 12.35 x 6.5 mm, so a standard plug fits with
                                 // 0.3 mm each side in width and 0.25 mm in height. A smaller plug could use a smaller window.

rim_z = stand_end;                    // housing rim and cover inner face: the standoffs end here too
cover_z = rim_z - cover_t;            // back face, -9.0
top_z = lcd_front + glass_gap;        // underside of the front frame

out_x0 = aa_x0 - bezel_w; out_x1 = aa_x1 + bezel_w;
bottom_eff = equal_bezel ? bezel_w : bottom_w;
out_y0 = aa_y0 - (bottom_neg ? bottom_eff : bezel_w);
out_y1 = aa_y1 + (bottom_neg ? bezel_w : bottom_eff);
in_x0 = out_x0 + wall; in_x1 = out_x1 - wall;
in_y0 = out_y0 + wall; in_y1 = out_y1 - wall;
bay_x = bat_x + bay_clear;     // half length of the pocket along X: the holder (end contacts) plus clearance

// the 6 cover screws [x, y, boss diameter]: 2 in the left wall, 2 on the narrow edge, 2 in the wide band. None near the board.
y_narrow = bottom_neg ? out_y1 - boss_d / 2 : out_y0 + boss_d / 2;
y_wide = equal_bezel ? (bottom_neg ? out_y0 + boss_d / 2 : out_y1 - boss_d / 2)     // equal bezel: the bottom bosses sit like the top ones, 0.6 mm from the PCB
                     : (bottom_neg ? (pcb_y0 + out_y0) / 2 : (pcb_y1 + out_y1) / 2);
boss_b = equal_bezel ? boss_d : boss_d_wide;   // boss diameter in the bottom band
screws = [[out_x0 + boss_d / 2, 22, boss_d], [out_x0 + boss_d / 2, -22, boss_d],
          [-20, y_narrow, boss_d], [in_x1 - boss_d / 2, y_narrow, boss_d],
          [-20, y_wide, boss_b], [in_x1 - boss_b / 2, y_wide, boss_b]];   // the right ones touch the +X wall
pin_pos = [[-35, y_wide], [30, y_wide]];      // guide pins: in the wide band, 65 mm apart, away from the board and from the screws
pocket_floor_z = -(cell_dmax + pocket_clear);  // lowest inner point of the battery pocket, -19.1
pad_seat_z = pocket_floor_z - pad_seat;

// The +X wall must leave room for the flex that wraps round that edge: about 1.3 mm from the PCB edge (ASSUMED, from the photo).
// That, not the look, sets the smallest equal bezel: 5.46 (PCB beyond the active area) + 1.3 + wall = about 9 mm.
// ---- the three buttons: printed pins that carry a press from the outside to the switch plunger ----
btn_w = 7.0; btn_h = 5.0;      // pin cross-section: the same all along, head to flange, so it goes in from the inside
btn_r = 1.5;                   // corner radius of the pin: about 40% of the short side, so it still reads as a rectangle but is not sharp
btn_clear = 0.25;              // sliding clearance each side; the hole is the pin + this, with the radius grown to match (1.3)
btn_edge = 0.45;               // round-over of the front edge of the head
btn_head_out = 0.8;            // how far the head stands out of the wall
btn_fl_w = 9.0; btn_fl_h = 7.5; btn_fl_t = 0.40;   // flange behind the rib: stops the pin from falling out
btn_nose_w = 3.0; btn_nose_h = 1.6;// nose that meets the plunger tip
y_rib = 36.4;                  // face of the (thinned) rib the flange rests on: the rib is 2.2 mm thick, was 3.9, so the nose is 1.5 mm long and can be trimmed to fit
btn_stock = 1.0;               // extra nose length on the printed pins, to be trimmed off when fitting (see buttons_print)
y_stop = y_rib - 0.05;         // the flange stops 0.05 mm short of it, so the pin never touches the housing

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

// Rounded rectangle section w x h with corner radius r, centred.
module rr2d(w, h, r) hull() for (sx = [-1, 1], sz = [-1, 1]) translate([sx * (w / 2 - r), sz * (h / 2 - r)]) circle(r, $fn = 32);
// Bar along +Y from y0 to y1 with a rounded-rectangle section (X and Z), centred on x = 0, z = 0.
module ybar(y0, y1, w, h, r) translate([0, y0, 0]) rotate([-90, 0, 0]) linear_extrude(y1 - y0) rr2d(w, h, r);
// Round-cornered window through a wall that faces +Y.
module ywindow(y0, len, xc, zc, w, h, r) translate([xc, 0, zc]) ybar(y0, y0 + len, w, h, r);

// One printed button: head outside the wall, stem through the tunnel, flange stopping against the rib, nose touching the plunger.
module button_pin(xc, stock = 0) {
    y_head = out_y1 + btn_head_out;           // front of the head
    translate([xc, 0, btn_zc]) {
        ybar(y_stop, y_head - btn_edge, btn_w, btn_h, btn_r);                      // head and stem: one section, so it goes in from the inside
        hull() {                                                                   // the front edge of the head, rounded over
            ybar(y_head - btn_edge - 0.01, y_head - btn_edge, btn_w, btn_h, btn_r);
            ybar(y_head - 0.01, y_head, btn_w - 2 * btn_edge, btn_h - 2 * btn_edge, max(btn_r - btn_edge, 0.1));
        }
        ybar(y_stop - btn_fl_t, y_stop + 0.01, btn_fl_w, btn_fl_h, btn_r);         // flange behind the rib: bigger than the hole, but it never has to pass through it
        ybar(btn_tip_y + 0.05 - stock, y_stop - btn_fl_t + 0.01, btn_nose_w, btn_nose_h, 0.4);   // nose on the plunger
    }
}
module buttons(stock = 0) for (x = btn_x) button_pin(x, stock);

module housing() {
    difference() {
        union() {
            difference() {
                cbox(out_x0, out_x1, out_y0, out_y1, rim_z, top_z + plate_t, corner_r, 0, edge_front);
                rbox(in_x0, in_x1, in_y0, in_y1, rim_z - 1, top_z, max(corner_r - wall, 0.5));
            }
            for (s = screws) translate([s[0], s[1], rim_z]) cylinder(d = s[2], h = top_z - rim_z + 0.01);
            for (p = pin_pos) translate([p[0], p[1], rim_z]) cylinder(d = pin_boss_d, h = top_z - rim_z + 0.01);
            // rib that carries the button windows to the board edge (the narrow wall is further out than the buttons)
            translate([-16, y_rib, rim_z]) cube([32, in_y1 - y_rib + 0.5, top_z - rim_z]);
        }
        // window: the active area plus a gap, front frame only
        translate([0, 0, top_z - 1]) rbox(aa_x0 - win_gap, aa_x1 + win_gap, aa_y0 - win_gap, aa_y1 + win_gap, 0, plate_t + 2, 0.5);
        // foam seat: the underside of the front frame is relieved over the whole glass outline, so the foam strip has a pocket
        translate([0, 0, top_z - 1]) rbox(-glass_x / 2 - 0.3, glass_x / 2 + 0.3, -glass_y / 2 - 0.3, glass_y / 2 + 0.3, 0, 1 + seat_depth, 0.5);
        for (p = pin_pos) translate([p[0], p[1], rim_z - 1]) cylinder(d = pin_hole_d, h = pin_hole_depth + 1);   // holes for the cover's guide pins
        for (s = screws) translate([s[0], s[1], rim_z - 1]) cylinder(d = pilot_d, h = pilot_depth + 1);   // pilot holes, open to the back
        // USB-C: the socket is only 8.94 x 3.16 and sits flush with the board edge, 3.5 mm behind the outside face, so the
        // plug overmould has to pass through the wall: the window is sized for the overmould, not the socket
        xwindow(in_x1 - 1, wall + 2, usb_y, (usb_z0 + usb_z1) / 2, usb_win_w, usb_win_h, 2.0);
        // microSD: slot for the card to enter and be pinched out
        xwindow(in_x1 - 1, wall + 2, (sd_y0 + sd_y1) / 2, sd_win_zc, sd_win_w, sd_win_h, 0.9);
        // three side buttons: rounded tunnels for the printed pins
        for (x = btn_x) ywindow(y_rib - 0.3, out_y1 - y_rib + 1.0, x, btn_zc, btn_w + 2 * btn_clear, btn_h + 2 * btn_clear, btn_r + btn_clear);
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
            for (p = pin_pos) translate([p[0], p[1], rim_z - 0.01]) {                  // guide pins, with a chamfered tip
                cylinder(d = pin_d, h = pin_h - 0.5 + 0.01);
                translate([0, 0, pin_h - 0.5]) cylinder(d1 = pin_d, d2 = pin_d - 0.8, h = 0.5);
            }
            // battery pocket on the back: the pocket section grown by the wall, and cut off at the cover's inner face
            intersection() {
                translate([0, cell_y, 0]) along_x(2 * (bay_x + bay_wall)) offset(r = bay_wall, $fn = 48) pocket2d();
                translate([-100, -100, -100]) cube([200, 200, 100 + rim_z]);
            }
        }
        translate([0, cell_y, 0]) along_x(2 * bay_x) pocket2d();   // the pocket itself, open to the inside
        for (x = pad_x) translate([x - pad_l / 2 - 0.2, cell_y - pad_w / 2 - 0.2, pad_seat_z]) cube([pad_l + 0.4, pad_w + 0.4, 3]);   // seats for the cell cushions
        translate([hdr_x0 - 1, hdr_y0 - 1, cover_z - 1]) cube([hdr_x1 - hdr_x0 + 2, hdr_y1 - hdr_y0 + 2, cover_t + 2]);  // header window
        if (board_screws) for (sx = [-1, 1], sy = [-1, 1]) {                             // M2.5 countersunk screws into the board's 4 standoffs
            translate([sx * stand_xy[0], sy * stand_xy[1], cover_z - 1]) cylinder(d = 2.9, h = cover_t + 2);
            translate([sx * stand_xy[0], sy * stand_xy[1], cover_z - 0.01]) cylinder(d1 = 5.4, d2 = 2.9, h = 1.5);
            translate([sx * stand_xy[0], sy * stand_xy[1], cover_z - 15]) cylinder(d = 4.4, h = 15);   // driver access where the pocket's end shoulder is in the way
        }
        for (s = screws) {                                                               // M2 countersunk screws into the housing pilot holes
            translate([s[0], s[1], cover_z - 1]) cylinder(d = 2.4, h = cover_t + 2);
            translate([s[0], s[1], cover_z - 0.01]) cylinder(d1 = 4.4, d2 = 2.4, h = 1.1);
        }
    }
}

// Foam strip round the glass, as it sits squeezed between the glass and the relieved front frame (0.6 mm thick).
module foam_gasket() {
    z0 = lcd_front; z1 = top_z + seat_depth;
    difference() {
        rbox(-glass_x / 2 + 0.2, glass_x / 2 - 0.2, -glass_y / 2 + 0.2, glass_y / 2 - 0.2, z0, z1, 0.5);
        translate([0, 0, z0 - 1]) rbox(-glass_x / 2 + 0.2 + foam_w, glass_x / 2 - 0.2 - foam_w, -glass_y / 2 + 0.2 + foam_w, glass_y / 2 - 0.2 - foam_w, 0, z1 - z0 + 2, 0.3);
    }
}
// Two cushions under the cell, squeezed between the seat in the pocket floor and the cell (nominal 18 mm cell: 1.5 mm foam to 1.3 mm).
module cell_pads() for (x = pad_x) translate([x - pad_l / 2, cell_y - pad_w / 2, pad_seat_z]) cube([pad_l, pad_w, -cell_d - pad_seat_z]);
module foam() { foam_gasket(); cell_pads(); }

// Box grown by g in X and Y only (used to test clearance round the board).
module gcube(p, sz, g) translate([p[0] - g, p[1] - g, p[2]]) cube([sz[0] + 2 * g, sz[1] + 2 * g, sz[2]]);

// The 18650 cell lying on the back of the PCB, axis along X.
module battery(g = 0) {
    translate([-cell_len / 2, cell_y, -cell_d / 2]) rotate([0, 90, 0]) cylinder(d = cell_d + 2 * g, h = cell_len, $fn = 64);
}

// Reference dummy of the real board without the cell. g grows every part sideways by g (clearance check).
module board_parts(g = 0) {
    gcube([pcb_x0, pcb_y0, 0], [-pcb_notch_x - pcb_x0, pcb_y1 - pcb_y0, pcb_t], g);          // PCB, left of the notch
    gcube([-pcb_notch_x, pcb_y0, 0], [2 * pcb_notch_x, pcb_y1 - pcb_notch_d - pcb_y0, pcb_t], g);                       // between: cut back
    gcube([pcb_notch_x, pcb_y0, 0], [pcb_x1 - pcb_notch_x, pcb_y1 - pcb_y0, pcb_t], g);                               // right of the notch
    gcube([-glass_x / 2, -glass_y / 2, pcb_t], [glass_x, glass_y, lcd_front - pcb_t], g);
    for (sx = [-1, 1], sy = [-1, 1]) translate([sx * stand_xy[0], sy * stand_xy[1], stand_end + 0.01]) cylinder(d = 5.5 + 2 * g, h = -stand_end - 0.01);
    // holder frame: the real outline (it narrows towards the rim), not a box
    translate([0, cell_y, 0]) along_x(2 * bat_x) hull()
        for (p = concat([[frame_prof[0][0], 0]], frame_prof), sx = [-1, 1]) translate([sx * p[0], p[1]]) circle(0.01 + g, $fn = 8);
    gcube([38.74, usb_y - usb_w / 2, usb_z0], [46.27 - 38.74, usb_w, usb_z1 - usb_z0], g);
    gcube([sd_x0, sd_y0, sd_z0], [45.22 - sd_x0, sd_y1 - sd_y0, sd_z1 - sd_z0], g);
    for (x = btn_x) {                                                                   // switch body and its plunger
        gcube([x - 2.275, btn_y0, btn_z0], [4.55, btn_body_y1 - btn_y0, btn_z1 - btn_z0], g);
        gcube([x - 1.0, btn_body_y1, btn_zc - 0.5], [2.0, btn_tip_y - btn_body_y1, 1.0], g);
    }
    gcube([hdr_x0, hdr_y0, hdr_z0], [hdr_x1 - hdr_x0, hdr_y1 - hdr_y0, -hdr_z0], g);
}

// Board with its cell: for looking and for the clash check. Not part of the print.
module board() { board_parts(); battery(); }

module assembly() {
    color("DimGray") housing();
    color("Silver") cover();
    color("Tomato") buttons();
    color("HotPink") foam();
    color("SeaGreen") board_parts();
    color("Orange") battery();
}

// Parts pulled apart along Z (front up) so each can be seen; the button pins slide out sideways.
module exploded() {
    color("DimGray") translate([0, 0, 40]) housing();
    color("HotPink") translate([0, 0, 22]) foam_gasket();
    color("Tomato") translate([0, 22, 40]) buttons();
    color("SeaGreen") board_parts();
    color("Orange") translate([0, 0, -14]) battery();
    color("HotPink") translate([0, 0, -26]) cell_pads();
    color("Silver") translate([0, 0, -38]) cover();
}

if (part == "housing") housing();
else if (part == "cover") cover();
else if (part == "buttons") buttons();
else if (part == "buttons_print") buttons(btn_stock);   // the pins as printed: nose 1.0 mm long, trimmed to fit
else if (part == "foam_gasket") foam_gasket();
else if (part == "cell_pads") cell_pads();
else if (part == "check_fit") intersection() { housing(); translate([0, 0, -0.01]) cover(); }   // must be empty: the cover and its guide pins fit the housing (cover 0.01 mm lower, so the rim faces that touch do not count)
else if (part == "check_pin_path") intersection() { union() { housing(); cover(); } union() for (d = [0, 0.5, 1, 2, 3, 4, 5, 6, 8, 10]) translate([0, -d, 0]) buttons(); }   // must be empty: a pin can be pushed in from the inside, along its own axis
else if (part == "check_pins") intersection() { buttons(); union() { housing(); cover(); } }   // must be empty: the pins do not touch the case
else if (part == "exploded") exploded();
else if (part == "cutaway") intersection() { assembly(); translate([-100, -100, -100]) cube([102, 200, 200]); }   // the half with X <= 2, to see inside
else if (part == "board") board();
else if (part == "board_parts") board_parts();
else if (part == "battery") battery();
else if (part == "section_battery") intersection() { assembly(); translate([-100, -100, -100]) cube([100, 200, 200]); }   // cut across the cell, X <= 0
else if (part == "section") intersection() { assembly(); translate([-100, -100, -100]) cube([200, 100 + 12, 200]); }
else if (part == "check_gap") intersection() { union() { housing(); cover(); } board_parts(gap_min); }   // must be empty: nothing closer than gap_min sideways
else if (part == "plan") intersection() { assembly(); translate([-100, -100, 0.2]) cube([200, 200, 1.0]); }    // slice through the PCB, seen from the front
else if (part == "check") intersection() { union() { housing(); cover(); buttons(); } board(); }   // must be empty
else assembly();
