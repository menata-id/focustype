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
//   housing, cover, board (reference dummy, not printed), assembly, section, check

use <../third_party/ibm-plex/IBMPlexSans-Bold.ttf>   // the wordmark font; not installed system-wide, so load the file

part = "assembly";
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
bat_x = 38.9; bat_y0 = -29.95; bat_y1 = -8.85; bat_depth = 15.2;  // 18650 holder, back of PCB to its top
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
boss_d = 6.0; insert_d = 3.2; insert_depth = 5.0;   // M2 heat-set inserts: check the hole size against the insert datasheet
bay_clear = 0.8; bay_wall = 2.0;
usb_win_w = 13; usb_win_h = 7;   // window for the plug overmould, ASSUMED: measure the plug you will use and add 1 mm each way

rim_z = stand_end;                    // housing rim and cover inner face: the standoffs end here too
cover_z = rim_z - cover_t;            // back face, -9.0
top_z = lcd_front + glass_gap;        // underside of the front frame
bay_floor = -(bat_depth + bay_clear) - bay_wall;   // outer floor of the battery pocket, -18.0

out_x0 = aa_x0 - bezel_w; out_x1 = aa_x1 + bezel_w;
out_y0 = aa_y0 - (bottom_neg ? bottom_w : bezel_w);
out_y1 = aa_y1 + (bottom_neg ? bezel_w : bottom_w);
in_x0 = out_x0 + wall; in_x1 = out_x1 - wall;
in_y0 = out_y0 + wall; in_y1 = out_y1 - wall;
bay_x = bat_x + bay_clear; bay_y0 = bat_y0 - bay_clear; bay_y1 = bat_y1 + bay_clear;

// the 6 cover screws: 2 in the left wall, 2 on the narrow edge, 2 in the wide band
y_narrow = bottom_neg ? out_y1 - boss_d / 2 : out_y0 + boss_d / 2;
y_wide = bottom_neg ? (pcb_y0 + out_y0) / 2 : (pcb_y1 + out_y1) / 2;
screws = [[out_x0 + boss_d / 2 + 0.1, 22], [out_x0 + boss_d / 2 + 0.1, -22],
          [-20, y_narrow], [44, y_narrow], [-20, y_wide], [44, y_wide]];

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
            for (s = screws) translate([s[0], s[1], rim_z]) cylinder(d = boss_d, h = top_z - rim_z + 0.01);
            // rib that carries the button windows to the board edge (the narrow wall is further out than the buttons)
            translate([-14, pcb_y1 + 0.34, rim_z]) cube([28, in_y1 - pcb_y1 - 0.34 + 0.5, top_z - rim_z]);
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

module cover() {
    difference() {
        union() {
            cbox(out_x0, out_x1, out_y0, out_y1, cover_z, rim_z, corner_r, edge_back, 0);
            rbox(-bay_x - bay_wall, bay_x + bay_wall, bay_y0 - bay_wall, bay_y1 + bay_wall, bay_floor, cover_z + 0.5, 1.5);   // battery pocket on the back
        }
        rbox(-bay_x, bay_x, bay_y0, bay_y1, bay_floor + bay_wall, rim_z + 0.5, 1);   // the pocket, open to the inside
        translate([hdr_x0 - 1, hdr_y0 - 1, cover_z - 1]) cube([hdr_x1 - hdr_x0 + 2, hdr_y1 - hdr_y0 + 2, cover_t + 2]);  // header window
        for (s = screws) {                                                               // M2 countersunk screws into the housing inserts
            translate([s[0], s[1], cover_z - 1]) cylinder(d = 2.4, h = cover_t + 2);
            translate([s[0], s[1], cover_z - 0.01]) cylinder(d1 = 4.4, d2 = 2.4, h = 1.1);
        }
    }
}

// Reference dummy of the real board, for looking and for the clash check. Not part of the print.
module board() {
    translate([pcb_x0, pcb_y0, 0]) cube([pcb_x1 - pcb_x0, pcb_y1 - pcb_y0, pcb_t]);
    translate([-glass_x / 2, -glass_y / 2, pcb_t]) cube([glass_x, glass_y, lcd_front - pcb_t]);
    for (sx = [-1, 1], sy = [-1, 1]) translate([sx * stand_xy[0], sy * stand_xy[1], stand_end + 0.01]) cylinder(d = 5.5, h = -stand_end - 0.01);
    translate([-bat_x, bat_y0, -bat_depth]) cube([2 * bat_x, bat_y1 - bat_y0, bat_depth]);
    translate([38.74, usb_y - usb_w / 2, usb_z0]) cube([46.27 - 38.74, usb_w, usb_z1 - usb_z0]);
    translate([sd_x0, sd_y0, sd_z0]) cube([45.22 - sd_x0, sd_y1 - sd_y0, sd_z1 - sd_z0]);
    for (x = btn_x) translate([x - btn_len / 2, btn_y0, btn_z0]) cube([btn_len, btn_y1 - btn_y0, btn_z1 - btn_z0]);
    translate([hdr_x0, hdr_y0, hdr_z0]) cube([hdr_x1 - hdr_x0, hdr_y1 - hdr_y0, -hdr_z0]);
}

module assembly() {
    color("DimGray") housing();
    color("Silver") cover();
    color("SeaGreen") board();
}

if (part == "housing") housing();
else if (part == "cover") cover();
else if (part == "board") board();
else if (part == "section") intersection() { assembly(); translate([-100, -100, -100]) cube([200, 100 + 12, 200]); }
else if (part == "check") intersection() { union() { housing(); cover(); } board(); }   // must be empty
else assembly();
