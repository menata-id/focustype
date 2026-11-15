// FocusType case for the Waveshare ESP32-S3-RLCD-4.2 (V2). Replaces the acrylic back plate and stand.
// Units: mm. Read case/ref/README.md first: every number below comes from the Waveshare STEP/DXF unless marked ASSUMED.
//
// Frame: X = long side (USB-C at +X), Y = short side (buttons on the +Y edge), Z = thickness, front of the LCD at +Z.
// Z = 0 is the back face of the PCB. Parts: pick with  openscad -D 'part="shell"' case.scad
//   shell, bezel, lid, board (reference dummy, not printed), assembly, section, check

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

// ---- case choices ----
clear = 0.3;          // gap around the PCB
fpc_extra = 1.0;      // extra room at +X: the LCD flex wraps round that edge (photo, ASSUMED 1 mm is enough)
wall = 3.0;           // thick enough for the M2 self-tapping bezel screws
floor_t = 2.0;
floor_z = stand_end - floor_t;        // outer face of the back, -9.0
top_z = lcd_front + clear;            // top of the walls, bezel sits on it
bezel_t = 1.5;
win_gap = 0.3;        // bezel window larger than the active area
bay_clear = 0.8; bay_wall = 2.0; bay_floor = -17.0;   // battery bay under the back, open at the bottom, closed by the lid

cav_x0 = pcb_x0 - clear; cav_x1 = pcb_x1 + clear + fpc_extra;
cav_y0 = pcb_y0 - clear; cav_y1 = pcb_y1 + clear;
out_x0 = cav_x0 - wall; out_x1 = cav_x1 + wall;
out_y0 = cav_y0 - wall; out_y1 = cav_y1 + wall;
corner_r = 3.0;
pilots = [for (sx = [-1, 1], sy = [-1, 1]) [sx * 25, sy * (cav_y1 + wall / 2)]];  // bezel screws, M2 self-tapping
bay_x = bat_x + bay_clear; bay_y0 = bat_y0 - bay_clear; bay_y1 = bat_y1 + bay_clear;

module rbox(x0, x1, y0, y1, z0, z1, r = 0.5) {
    translate([0, 0, z0]) linear_extrude(z1 - z0)
        hull() for (x = [x0 + r, x1 - r], y = [y0 + r, y1 - r]) translate([x, y]) circle(r);
}

module shell() {
    difference() {
        union() {
            rbox(out_x0, out_x1, out_y0, out_y1, floor_z, top_z, corner_r);
            rbox(-bay_x - bay_wall, bay_x + bay_wall, bay_y0 - bay_wall, bay_y1 + bay_wall, bay_floor, floor_z + 0.5, 1.5);
        }
        rbox(cav_x0, cav_x1, cav_y0, cav_y1, stand_end, top_z + 1, 0.5);                 // the PCB pocket
        rbox(-bay_x, bay_x, bay_y0, bay_y1, bay_floor - 1, stand_end + 0.5, 1);       // the battery bay
        for (sx = [-1, 1], sy = [-1, 1]) {                                               // screws into the standoffs
            translate([sx * stand_xy[0], sy * stand_xy[1], floor_z - 1]) cylinder(d = 2.9, h = floor_t + 2);
            translate([sx * stand_xy[0], sy * stand_xy[1], floor_z - 0.01]) cylinder(d1 = 5.4, d2 = 2.9, h = 1.5);
            translate([sx * stand_xy[0], sy * stand_xy[1], bay_floor - 1]) cylinder(d = 6.5, h = -bay_floor + floor_z + 0.9 - 1);  // driver access through the bay wall
        }
        translate([hdr_x0 - 1, hdr_y0 - 1, floor_z - 1]) cube([hdr_x1 - hdr_x0 + 2, hdr_y1 - hdr_y0 + 2, floor_t + 2]);  // header window
        // USB-C: a plug overmould is bigger than the socket, so 13 x 7 (ASSUMED)
        translate([cav_x1 - 1, usb_y - 6.5, (usb_z0 + usb_z1) / 2 - 3.5]) cube([wall + 2, 13, 7]);
        // microSD: slot for the card to enter and be pinched out
        translate([cav_x1 - 1, (sd_y0 + sd_y1) / 2 - 6.25, (sd_z0 + sd_z1) / 2 - 1.65]) cube([wall + 2, 12.5, 3.3]);
        // three side buttons
        for (x = btn_x) translate([x - 3.2, cav_y1 - 1, (btn_z0 + btn_z1) / 2 - 1.7]) cube([6.4, wall + 2, 3.4]);
        for (p = pilots) translate([p[0], p[1], top_z - 8]) cylinder(d = 1.8, h = 9);
    }
}

module bezel() {
    difference() {
        rbox(out_x0, out_x1, out_y0, out_y1, top_z, top_z + bezel_t, corner_r);
        translate([0, 0, top_z - 1]) rbox(aa_x0 - win_gap, aa_x1 + win_gap, aa_y0 - win_gap, aa_y1 + win_gap, 0, bezel_t + 2, 0.5);
        for (p = pilots) {
            translate([p[0], p[1], top_z - 1]) cylinder(d = 2.2, h = bezel_t + 2);
            translate([p[0], p[1], top_z + bezel_t - 1.0]) cylinder(d1 = 2.2, d2 = 4.2, h = 1.01);
        }
    }
}

// Battery lid: friction fit plug (0.2 mm clearance), thumb notch. ASSUMED, not tested; may need a screw.
module lid() {
    translate([0, 0, bay_floor - 1.5]) {
        rbox(-bay_x - bay_wall, bay_x + bay_wall, bay_y0 - bay_wall, bay_y1 + bay_wall, 0, 1.5, 1.5);
        translate([0, 0, 1.5]) rbox(-bay_x + 0.2, bay_x - 0.2, bay_y0 + 0.2, bay_y1 - 0.2, 0, 1.2, 1);
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
    color("DimGray") shell();
    color("Silver") bezel();
    color("SlateGray") lid();
    color("SeaGreen") board();
}

if (part == "shell") shell();
else if (part == "bezel") bezel();
else if (part == "lid") lid();
else if (part == "board") board();
else if (part == "section") intersection() { assembly(); translate([-100, -100, -100]) cube([200, 100 + 12, 200]); }
else if (part == "check") intersection() { union() { shell(); bezel(); lid(); } board(); }   // must be empty
else assembly();
