// =====================================================================
//  Shuffle Alarm Clock - enclosure (OpenSCAD)
//
//  Parts (set `part` below, then F6 render + export STL):
//    shell          main body: front, top, sides, back (print UPSIDE DOWN)
//    base           bottom plate the electronics stick to (print flat)
//    snooze_bracket holds the 2 snooze switches under the snooze bar
//    off_bracket    holds the OFF switch
//    lcd_tab        clamp for the screen (print 2)
//    speaker_cup    sealed back box for the speaker
//    assembly       everything together with dummy parts, for checking fit
//
//  All sizes are in mm. Everything you might need to tweak is at the top.
// =====================================================================

part = "assembly"; // [assembly, shell, base, snooze_bracket, off_bracket, lcd_tab, speaker_cup]

/* [Case] */
W      = 144;   // width
D      = 84;    // depth at the base
H      = 74;    // height
tilt   = 15;    // how far the front face leans back (degrees)
rtilt  = 8;     // how far the back leans forward (degrees)
r      = 14;    // outside corner rounding
t      = 2.4;   // wall thickness
base_t = 3;     // bottom plate thickness
fit    = 0.3;   // gap between bottom plate and shell

/* [Screen: 2inch LCD Module] */
lcd_x       = 44;    // centre of the screen, from the left side
lcd_zc      = 40;    // centre height on the front face
lcd_board   = [58.5, 35];
lcd_active  = [40.8, 30.6];  // visible picture area
lcd_act_off = 1.4;   // picture sits this far toward the cable end (+ = right)
lcd_stack   = 4.5;   // glass front to back of the circuit board
lcd_ridge   = 1.0;   // spacer ring that the glass rests on
lcd_clear   = 0.4;

/* [Speaker: 40 mm, 4 ohm 5 W] */
spk_x        = 108;
spk_zc       = 40;
spk_frame    = 40;
spk_holes    = 32;   // centre-to-centre of the speaker's screw holes
spk_depth    = 19.5;
spk_ring_h   = 4.5;  // spacer between the front wall and the speaker
spk_ring_w   = 50;
cup_depth    = 30;   // depth of the sealed box behind the speaker
cup_screw    = 23;   // cup screw positions (+/- from centre)

/* [Buttons: 6x6 tactile] */
sw_body     = [6.4, 6.6];  // pocket size (switch + clearance)
sw_body_h   = 3.5;   // height of the square body
sw_total_h  = 5.0;   // body + plunger. MEASURE YOURS and set this
sw_gap      = 0.3;   // free play before the switch clicks
boss_len    = 6;     // ceiling posts the brackets screw to
br_t        = 1.2;   // bracket roof thickness above the switch
slot        = 1.2;   // width of the cut around each flex button

// flex buttons cut into the top: [x0, y0, x1, y1]
snooze_pad  = [22, 33, 98, 52];    // hinge along the back edge
off_pad     = [106, 43, 124, 60];  // hinge along the front edge
snooze_sw   = [[42, 37], [78, 37]];
off_sw      = [[115, 56.5]];
snooze_bosses = [[16, 40], [102, 40]];
off_bosses    = [[101.5, 56], [128, 56]];
snooze_bar  = [12, 106, 33, 45];   // bracket outline x0, x1, y0, y1
off_bar     = [97, 131, 50, 61];

/* [Circuit boards on the base: x, y, width, depth] */
esp_rect   = [12, 14.5, 28, 64];   // USB end toward the back
sd_rect    = [47, 38, 25, 42];     // card slot toward the back
rtc_rect   = [46, 14, 38.5, 22];
pwr_rect   = [77, 50.5, 17.5, 28]; // USB-C toward the back
amp_rect   = [99, 59.5, 19.5, 19.5];
cap_pos    = [122, 52];
cap_d      = 10.5;

/* [Base screws] */
base_screws = [[5.4, 24], [5.4, 64], [W - 5.4, 24], [W - 5.4, 64]];
pad_h       = 3;    // raised pads on the base around each screw

$fn = 48;

// ---------------------------------------------------------------------
tn = tan(tilt);
cs = cos(tilt);
sn = sin(tilt);
function yc(z) = r / cs + z * tn;                       // front corner sphere centres
function yr(z) = D - r / cos(rtilt) - z * tan(rtilt);   // back corner sphere centres
z_ceil   = H - t;                  // underside of the top
z_br_top = z_ceil - boss_len;      // top of the switch brackets
br_h     = br_t + sw_body_h;       // bracket thickness
z_plunger = z_br_top - br_t + (sw_total_h - sw_body_h);
nub_len  = z_ceil - (z_plunger + sw_gap);
boss_z   = base_t + pad_h;         // shell screw bosses start here

// ---------- body shapes ----------
module body_hull(rad, zbot) {
  hull() for (x = [r, W - r]) {
    translate([x, yc(zbot), zbot]) sphere(rad);
    translate([x, yr(zbot), zbot]) sphere(rad);
    translate([x, yc(H - r), H - r]) sphere(rad);
    translate([x, yr(H - r), H - r]) sphere(rad);
  }
}
module outer_solid() {
  intersection() {
    body_hull(r, 0);
    translate([-1, -1, 0]) cube([W + 2, D + 2, H + 2]);
  }
}
module inner_solid(extra = 0) { body_hull(r - t - extra, -8); }

// Local frame on the front face: u = across, v = up the face, w = outward.
// w = 0 is the outside surface, w = -t the inside surface.
module on_front(x, zc) {
  multmatrix([[1, 0, 0, x], [0, sn, -cs, zc * tn], [0, cs, sn, zc], [0, 0, 0, 1]]) children();
}
module box_c(size, w0, w1) {  // box centred in u/v, spanning w0..w1
  translate([-size[0] / 2, -size[1] / 2, w0]) cube([size[0], size[1], w1 - w0]);
}
module rrect(size, rad, h) {  // rounded rectangle, centred
  linear_extrude(h) offset(rad) square([size[0] - 2 * rad, size[1] - 2 * rad], center = true);
}

// ---------- screen ----------
lcd_win   = [lcd_active[0] + 1.6, lcd_active[1] + 1.6];
lcd_inner = [lcd_board[0] + 2 * lcd_clear, lcd_board[1] + 2 * lcd_clear];
lcd_depth = lcd_ridge + lcd_stack;
lcd_tab_v = lcd_inner[1] / 2 + 1.6 + 3.0;

module lcd_features() {
  on_front(lcd_x, lcd_zc) {
    // ridge the glass rests on
    translate([lcd_act_off, 0, 0]) difference() {
      box_c([lcd_win[0] + 6, lcd_win[1] + 6], -t - lcd_ridge, -t + 0.5);
      box_c(lcd_win, -t - lcd_ridge - 1, 0);
    }
    // long walls + corner stops (short ends left open for the cable/header)
    difference() {
      box_c([lcd_inner[0] + 3.2, lcd_inner[1] + 3.2], -t - lcd_depth, -t + 0.5);
      box_c(lcd_inner, -t - lcd_depth - 1, 0);
      box_c([lcd_inner[0] + 4, lcd_inner[1] - 12], -t - lcd_depth - 1, 0);
    }
    // posts for the two clamp tabs
    for (s = [-1, 1]) translate([0, s * lcd_tab_v, -t - lcd_depth]) cylinder(d = 7, h = lcd_depth + 0.5);
  }
}
module lcd_cuts() {
  on_front(lcd_x, lcd_zc) {
    translate([lcd_act_off, 0, 0]) hull() {   // window, chamfered outward
      box_c(lcd_win, -t - 0.01, -t);
      box_c([lcd_win[0] + 2 * t, lcd_win[1] + 2 * t], 0, 0.01);
    }
    translate([lcd_act_off, 0, 0]) box_c(lcd_win, -t - lcd_ridge - 1, -t + 0.01);
    for (s = [-1, 1]) translate([0, s * lcd_tab_v, -t - lcd_depth - 0.1]) cylinder(d = 1.8, h = lcd_depth - 0.3, $fn = 16);
  }
}

// ---------- speaker ----------
module speaker_features() {
  on_front(spk_x, spk_zc) box_c([spk_ring_w, spk_ring_w], -t - spk_ring_h, -t + 0.5);
}
module speaker_cuts() {
  on_front(spk_x, spk_zc) {
    translate([0, 0, -t - spk_ring_h - 0.1]) cylinder(d = 36, h = spk_ring_h + 0.1);
    // grille
    for (i = [-6 : 6], j = [-6 : 6]) {
      px = i * 3.4 + (j % 2) * 1.7;
      py = j * 2.95;
      if (sqrt(px * px + py * py) < 16.5) translate([px, py, -t - 0.2]) cylinder(d = 2.2, h = t + 0.4, $fn = 14);
    }
    // screw holes: speaker (inside) + cup (corners)
    for (sx = [-1, 1], sy = [-1, 1]) {
      translate([sx * spk_holes / 2, sy * spk_holes / 2, -t - spk_ring_h - 0.1]) cylinder(d = 1.8, h = spk_ring_h + 1.3, $fn = 16);
      translate([sx * cup_screw, sy * cup_screw, -t - spk_ring_h - 0.1]) cylinder(d = 1.8, h = spk_ring_h + 1.3, $fn = 16);
    }
  }
}

// ---------- top buttons ----------
module pad_cuts(p, hinge_back) {
  x0 = p[0]; y0p = p[1]; x1 = p[2]; y1 = p[3];
  translate([0, 0, z_ceil - 1]) linear_extrude(t + 2) {
    if (hinge_back) {
      translate([x0 - slot, y0p - slot]) square([x1 - x0 + 2 * slot, slot]);
      translate([x0 - slot, y0p - slot]) square([slot, y1 - y0p + slot]);
      translate([x1, y0p - slot]) square([slot, y1 - y0p + slot]);
    } else {
      translate([x0 - slot, y1]) square([x1 - x0 + 2 * slot, slot]);
      translate([x0 - slot, y0p]) square([slot, y1 - y0p + slot]);
      translate([x1, y0p]) square([slot, y1 - y0p + slot]);
    }
  }
  // thinner hinge so it flexes easily
  hy = hinge_back ? y1 : y0p;
  translate([x0, hy - 1.5, z_ceil - 1]) cube([x1 - x0, 3, 2]);
}
module top_features() {
  for (b = concat(snooze_bosses, off_bosses)) translate([b[0], b[1], z_br_top]) cylinder(d = 5.5, h = boss_len + 1);
  for (s = concat(snooze_sw, off_sw)) translate([s[0], s[1], z_ceil - nub_len]) cylinder(d = 4, h = nub_len + 0.5);
}
module top_cuts() {
  pad_cuts(snooze_pad, true);
  pad_cuts(off_pad, false);
  for (b = concat(snooze_bosses, off_bosses)) translate([b[0], b[1], z_br_top - 0.1]) cylinder(d = 1.8, h = boss_len, $fn = 16);
  translate([0, 0, H - 0.6]) linear_extrude(1) {
    translate([(snooze_pad[0] + snooze_pad[2]) / 2, (snooze_pad[1] + snooze_pad[3]) / 2 - 3])
      text("SNOOZE", size = 7, font = "Liberation Sans:style=Bold", halign = "center", valign = "center");
    translate([(off_pad[0] + off_pad[2]) / 2, (off_pad[1] + off_pad[3]) / 2 + 2])
      text("OFF", size = 5.5, font = "Liberation Sans:style=Bold", halign = "center", valign = "center");
  }
}

// ---------- base screw bosses (with a 45 degree gusset so it prints upside down) ----------
module base_bosses() {
  for (p = base_screws) {
    wx = p[0] < W / 2 ? t - 0.5 : W - t;
    translate([p[0], p[1], boss_z]) cylinder(d = 7, h = 10);
    hull() {
      translate([p[0], p[1], boss_z + 10]) cylinder(d = 7, h = 0.01);
      translate([wx, p[1] - 3.5, boss_z + 17]) cube([0.5, 7, 0.01]);
    }
  }
}
module base_boss_holes() {
  for (p = base_screws) translate([p[0], p[1], boss_z - 0.1]) cylinder(d = 2.7, h = 10, $fn = 20);
}

// ---------- back openings ----------
// Local frame on the back face (u to the left as seen from behind, v up, w outward)
module on_rear(x, z) {
  rs = sin(rtilt); rc = cos(rtilt);
  multmatrix([[-1, 0, 0, x], [0, -rs, rc, D - z * tan(rtilt)], [0, rc, rs, z], [0, 0, 0, 1]]) children();
}
module notch(x0, x1, ztop) {
  translate([x0, D - t - 4, -1]) cube([x1 - x0, t + 6, ztop + 1 - 1.5]);
  hull() for (x = [x0 + 1.5, x1 - 1.5]) translate([x, D - t - 4, ztop - 1.5]) rotate([-90, 0, 0]) cylinder(r = 1.5, h = t + 6, $fn = 16);
}
module rear_label(txt, x, z) {
  on_rear(x, z) translate([0, 0, -0.6]) linear_extrude(1)
    text(txt, size = 4, font = "Liberation Sans:style=Bold", halign = "center", valign = "center");
}
module rear_cuts() {
  ex = esp_rect[0];
  notch(ex + 2, ex + 26, 10);                                   // ESP32 USB ports
  px = pwr_rect[0] + pwr_rect[2] / 2;
  notch(px - 6, px + 6, 10);                                    // power USB-C
  notch(sd_rect[0] + 4, sd_rect[0] + 21, 9.5);                  // SD card
  rear_label("POWER", px, 14.5);
  rear_label("SD", sd_rect[0] + 12.5, 14);
}

// ---------- SHELL ----------
module shell() {
  difference() {
    union() {
      difference() { outer_solid(); inner_solid(); }
      intersection() {
        outer_solid();
        union() { lcd_features(); speaker_features(); top_features(); base_bosses(); }
      }
    }
    lcd_cuts();
    speaker_cuts();
    top_cuts();
    base_boss_holes();
    rear_cuts();
  }
}

// ---------- BASE ----------
module corner_stops(rc, h = 3) {   // little L-shaped corners that locate a board
  x = rc[0] - 0.6; y = rc[1] - 0.6; w = rc[2] + 1.2; d = rc[3] + 1.2; L = 5; th = 1.2;
  for (cx = [0, 1], cy = [0, 1]) {
    ox = x + cx * w; oy = y + cy * d;
    translate([cx ? ox : ox - th, cy ? oy - L : oy, base_t]) cube([th, L, h]);
    translate([cx ? ox - L : ox - th, cy ? oy : oy - th, base_t]) cube([L + th, th, h]);
  }
}
module base_plate() {
  difference() {
    intersection() {
      inner_solid(fit);
      union() {
        translate([-1, -1, 0]) cube([W + 2, D + 2, base_t]);
        for (p = base_screws) translate([p[0], p[1], 0]) cylinder(d = 9, h = base_t + pad_h);
        for (rc = [esp_rect, sd_rect, rtc_rect, pwr_rect, amp_rect]) corner_stops(rc);
        translate([cap_pos[0], cap_pos[1], base_t]) difference() {
          cylinder(d = cap_d + 2.4, h = 3);
          translate([0, 0, -1]) cylinder(d = cap_d, h = 5);
        }
      }
    }
    for (p = base_screws) {
      translate([p[0], p[1], -1]) cylinder(d = 3.4, h = 20, $fn = 20);
      translate([p[0], p[1], -1]) cylinder(d = 6.2, h = 3.2 + 1, $fn = 24);  // screw head sits inside
    }
    // spots for stick-on rubber feet
    for (p = [[20, 18], [W - 20, 18], [20, D - 18], [W - 20, D - 18]])
      translate([p[0], p[1], -1]) cylinder(d = 10.5, h = 1.5);
    // vents under the ESP32
    for (i = [0 : 4]) translate([esp_rect[0] + 6, esp_rect[1] + 12 + i * 9, -1]) rrect([16, 3], 1.4, base_t + 2);
  }
}

// ---------- BRACKETS ----------
module bracket(x0, x1, yy0, yy1, bosses, switches) {
  difference() {
    translate([x0, yy0, z_br_top - br_h]) rrect_box(x1 - x0, yy1 - yy0, br_h);
    for (b = bosses) translate([b[0], b[1], z_br_top - br_h - 1]) cylinder(d = 2.4, h = br_h + 2, $fn = 16);
    for (s = switches) {
      translate([s[0] - sw_body[0] / 2, s[1] - sw_body[1] / 2, z_br_top - br_h - 1]) cube([sw_body[0], sw_body[1], sw_body_h + 1]);
      translate([s[0], s[1], z_br_top - br_t - 1]) cylinder(d = 4.2, h = br_t + 2, $fn = 24);
    }
  }
}
module rrect_box(w, d, h) { translate([w / 2, d / 2, 0]) rrect([w, d], 2, h); }
module snooze_bracket() { bracket(snooze_bar[0], snooze_bar[1], snooze_bar[2], snooze_bar[3], snooze_bosses, snooze_sw); }
module off_bracket()    { bracket(off_bar[0], off_bar[1], off_bar[2], off_bar[3], off_bosses, off_sw); }

// ---------- LCD TAB ----------
module lcd_tab() {
  difference() {
    translate([-5, -4.5, 0]) rrect_box(10, 13, 2);
    translate([0, 0, -1]) cylinder(d = 2.4, h = 4, $fn = 16);
  }
}

// ---------- SPEAKER CUP (drawn in the speaker's local frame) ----------
cup_in  = spk_frame + 2;
cup_out = cup_in + 4;
module speaker_cup_local() {
  difference() {
    union() {
      box_c([cup_out, cup_out], -cup_depth, 0);
      for (sx = [-1, 1], sy = [-1, 1]) translate([sx * cup_screw, sy * cup_screw, -2.5]) cylinder(d = 7, h = 2.5);
    }
    box_c([cup_in, cup_in], -cup_depth + 2, 0.1);
    for (sx = [-1, 1], sy = [-1, 1]) translate([sx * cup_screw, sy * cup_screw, -3]) cylinder(d = 2.4, h = 4, $fn = 16);
    translate([cup_in / 2 - 4, 0, -cup_depth - 1]) cylinder(d = 3, h = 4, $fn = 16);  // speaker wires
  }
}

// ---------- dummy parts for the assembly view ----------
module dummy_board(rc, h, col) { color(col) translate([rc[0], rc[1], base_t + 1]) cube([rc[2], rc[3], h]); }
module assembly(show_shell = true) {
  if (show_shell) color("SlateGray", 0.55) shell();
  color("DimGray") base_plate();
  color("Orange") snooze_bracket();
  color("Orange") off_bracket();
  on_front(lcd_x, lcd_zc) {
    color("RoyalBlue") box_c(lcd_board, -t - lcd_depth, -t - lcd_ridge);
    for (s = [-1, 1]) color("Orange") translate([0, s * lcd_tab_v, -t - lcd_depth - 2]) rotate([0, 0, s > 0 ? 180 : 0]) lcd_tab();
  }
  on_front(spk_x, spk_zc) {
    color("Black") box_c([spk_frame, spk_frame], -t - spk_ring_h - 1, -t - spk_ring_h);
    color("Black") translate([0, 0, -t - spk_ring_h - spk_depth]) cylinder(d = 32, h = spk_depth - 1);
    color("Orange") translate([0, 0, -t - spk_ring_h]) speaker_cup_local();
  }
  dummy_board(esp_rect, 6, "Black");
  dummy_board(sd_rect, 4, "Blue");
  dummy_board(rtc_rect, 9, "MediumBlue");
  dummy_board(pwr_rect, 5, "SteelBlue");
  dummy_board(amp_rect, 11, "Navy");
  color("DarkSlateGray") translate([cap_pos[0], cap_pos[1], base_t]) cylinder(d = cap_d - 0.5, h = 16);
}

// ---------- pick what to show ----------
if (part == "assembly") assembly();
else if (part == "shell") rotate([180, 0, 0]) translate([0, -D, -H]) shell();  // upside down, ready to print
else if (part == "base") base_plate();
else if (part == "snooze_bracket") translate([-snooze_bar[0], -snooze_bar[2], -(z_br_top - br_h)]) snooze_bracket();
else if (part == "off_bracket") translate([-off_bar[0], -off_bar[2], -(z_br_top - br_h)]) off_bracket();
else if (part == "lcd_tab") lcd_tab();
else if (part == "speaker_cup") rotate([180, 0, 0]) speaker_cup_local();  // open side down

// ---------- checking views (not for printing) ----------
sec = 44;
if (part == "section_x") intersection() { assembly(); translate([sec, -10, -10]) cube([200, 200, 200]); }
if (part == "inside") assembly(false);
if (part == "section_z") intersection() { assembly(); translate([-10, -10, -10]) cube([200, 200, sec]); }
