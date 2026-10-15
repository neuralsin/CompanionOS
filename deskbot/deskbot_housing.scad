/* DeskBot Housing v3 — Split-body with snap-fit & personality
 * Color scheme: SteelBlue body + Orange back panel
 * Optimized: thin walls (1.8mm), hollow feet, minimal material
 * Premium > Cost, but cost-conscious where invisible */

// ---- Part ----
part = "both";  // [body, panel, both]

// ---- Body ----
body_w = 100;  body_d = 100;  body_h = 100;
corner_r = 18;
wall = 1.8;             // optimized: premium feel, minimal material
top_wall = 0.8;  // ultra-thin for capacitive touch

// ---- Screen (7cm x 5cm) ----
scr_w = 70;  scr_h = 50;  scr_r = 5;
scr_z_off = 3;
bezel_w = 3;  bezel_d = 1.5;
pcb_extra = 2.5;  pcb_depth = 5;

// ---- Back Panel Split ----
bp_depth = 12;
bp_lip = 4;
lip_gap = 1.2;
bp_tol = 0.3;
split_gap = 0.4;  // visible gap at split line

// ---- ESP32 ----
esp_l = 52;  esp_w = 26;
esp_mount_z = 10;
esp_usb_w = 8;  esp_usb_h = 4;

// ---- LDR ----
ldr_d = 5;  ldr_z = 15;

// ---- Feet ----
foot_d = 10;  foot_h = 3;  foot_inset = 22;

// ---- Ears ----
ear_d = 13;
ear_spread = 24;  // distance from center

// ---- Rendering ----
$fn = 60;

// ---- Derived ----
eps = 0.01;
inner_r = max(corner_r - wall, 2);
scr_cx = body_w / 2;
scr_cz = body_h / 2 + scr_z_off;
split_y = body_d - bp_depth;
lip_r = max(corner_r - lip_gap, 2);

// ESP32 — pulled 3mm from wall to avoid protrusion at curves
esp_xr = body_w - wall - 3;
esp_xl = esp_xr - esp_l;
esp_bz = wall + esp_mount_z;
esp_usb_cz = esp_bz + 4;
esp_cy = body_d * 0.48;

// Snap bump parameters
snap_bump = 0.6;   // protrusion height
snap_w = 8;        // width
snap_d = 2;        // depth along Y

// ===================== PRIMITIVES =====================

module rcube(size, r) {
    hull() {
        for (x = [r, size[0]-r])
            for (y = [r, size[1]-r])
                for (z = [r, size[2]-r])
                    translate([x,y,z]) sphere(r=r);
    }
}

module rrect(w, h, r) {
    offset(r=r) square([w-2*r, h-2*r], center=true);
}

// ===================== SHELL =====================

module outer_shape() { rcube([body_w, body_d, body_h], corner_r); }

module inner_void() {
    translate([wall, wall, wall])
        rcube([body_w-2*wall, body_d-2*wall, body_h-wall-top_wall], inner_r);
}

module closed_shell() {
    difference() { outer_shape(); inner_void(); }
}

// ===================== PERSONALITY =====================

// Cute rounded ear bumps on top
module ears() {
    for (side = [-1, 1])
        translate([body_w/2 + side*ear_spread, body_d/2, body_h + 1])
            scale([1, 0.85, 0.55])
                sphere(d = ear_d);
}

// Decorative cheek circles on front face (subtle indents)
module cheek_details() {
    cheek_d = 7;
    cheek_depth = 0.6;
    cheek_z = scr_cz - 12;

    for (x_off = [-(scr_w/2 + 6), scr_w/2 + 6])
        translate([scr_cx + x_off, -corner_r, cheek_z])
            rotate([-90, 0, 0])
                cylinder(d=cheek_d, h=corner_r + cheek_depth);
}

// Small "nose" dot below screen center
module nose_dot() {
    translate([scr_cx, -corner_r, scr_cz - scr_h/2 - 6])
        rotate([-90, 0, 0])
            cylinder(d=3, h=corner_r + 0.8);
}

// ============== SPLIT BODY: MAIN + BACK PANEL ==============

module main_body_shell() {
    // Front portion of shell (up to split - gap)
    intersection() {
        closed_shell();
        translate([-1, -1, -1])
            cube([body_w+2, split_y - split_gap/2 + 1, body_h+2]);
    }

    // Lip: inset shell extending past split
    difference() {
        intersection() {
            translate([lip_gap, lip_gap, lip_gap])
                rcube([body_w-2*lip_gap, body_d-2*lip_gap, body_h-2*lip_gap], lip_r);
            translate([-1, split_y - split_gap/2, -1])
                cube([body_w+2, bp_lip + split_gap/2, body_h+2]);
        }
        inner_void();
    }

    // Snap retention bumps on the lip (4 points)
    snap_bumps();

    // Ears
    ears();
}

// Snap bumps on the lip outer surface
module snap_bumps() {
    by = split_y + bp_lip - snap_d - 1;

    for (z_frac = [0.35, 0.65]) {
        bz = body_h * z_frac - snap_w/2;

        // Left side bumps
        translate([lip_gap - snap_bump, by, bz])
            cube([snap_bump + 0.3, snap_d, snap_w]);

        // Right side bumps
        translate([body_w - lip_gap - 0.3, by, bz])
            cube([snap_bump + 0.3, snap_d, snap_w]);
    }
}

// Back panel (curved slice of body + lip pocket + snap slots)
module back_panel_shell() {
    difference() {
        // Back portion of shell
        intersection() {
            closed_shell();
            translate([-1, split_y + split_gap/2, -1])
                cube([body_w+2, bp_depth+1, body_h+2]);
        }

        // Lip pocket (enlarged inner void in lip zone)
        intersection() {
            translate([lip_gap+bp_tol, lip_gap+bp_tol, lip_gap+bp_tol])
                rcube([body_w-2*(lip_gap+bp_tol),
                       body_d-2*(lip_gap+bp_tol),
                       body_h-2*(lip_gap+bp_tol)],
                      max(corner_r-lip_gap-bp_tol, 2));
            translate([-1, split_y-eps, -1])
                cube([body_w+2, bp_lip+2*eps, body_h+2]);
        }

        // Snap relief slots (matching the bumps)
        snap_relief();
    }
}

// Snap relief slots in back panel
module snap_relief() {
    tol2 = bp_tol * 2;
    by = split_y + bp_lip - snap_d - 1 - bp_tol;
    rw = snap_w + tol2;
    rd = snap_d + bp_tol;
    rh = snap_bump + bp_tol + 0.3;

    for (z_frac = [0.35, 0.65]) {
        bz = body_h * z_frac - rw/2;

        // Left relief
        translate([lip_gap - snap_bump - bp_tol*2, by, bz])
            cube([rh + bp_tol, rd, rw]);

        // Right relief
        translate([body_w - lip_gap - 0.3 - bp_tol, by, bz])
            cube([rh + bp_tol, rd, rw]);
    }
}

// ============== SCREEN ==============

module screen_cuts() {
    bw = scr_w + 2*bezel_w;
    bh = scr_h + 2*bezel_w;

    // Bezel recess
    translate([scr_cx, -corner_r, scr_cz])
        rotate([-90,0,0])
            linear_extrude(corner_r + bezel_d)
                rrect(bw, bh, scr_r + bezel_w);

    // Screen window (70x50mm through-hole)
    translate([scr_cx, -corner_r, scr_cz])
        rotate([-90,0,0])
            linear_extrude(corner_r + wall + pcb_depth + 1)
                rrect(scr_w, scr_h, scr_r);

    // TFT module PCB pocket
    rw = scr_w + 2*pcb_extra;
    rh = scr_h + 2*pcb_extra;
    translate([scr_cx-rw/2, wall-eps, scr_cz-rh/2])
        cube([rw, pcb_depth+eps, rh]);
}

// ============== USB / LDR ==============

module usb_cut() {
    translate([body_w-corner_r, esp_cy-esp_usb_w/2, esp_usb_cz-esp_usb_h/2])
        cube([corner_r+1, esp_usb_w, esp_usb_h]);
    cw = esp_usb_w+2; ch = esp_usb_h+2;
    translate([body_w-1, esp_cy-cw/2, esp_usb_cz-ch/2])
        cube([1.5, cw, ch]);
}

module ldr_cut() {
    translate([-5, body_d/2, ldr_z])
        rotate([0,90,0]) cylinder(d=ldr_d, h=corner_r+10);
}

// ============== ESP32 CRADLE (fully inside body) ==============

module esp32_cradle() {
    post_d = 4;
    post_h = esp_mount_z;
    rail_h = 2;
    rail_t = 1.2;

    // Clip everything to interior bounds
    intersection() {
        union() {
            // 4 support posts
            for (dx = [3, esp_l-3])
                for (dy = [-esp_w/2+3, esp_w/2-3])
                    translate([esp_xl+dx, esp_cy+dy, wall])
                        cylinder(d=post_d, h=post_h);

            // Front rail
            translate([esp_xl+1, esp_cy-esp_w/2-rail_t, wall])
                cube([esp_l-2, rail_t, post_h+rail_h]);

            // Back rail
            translate([esp_xl+1, esp_cy+esp_w/2, wall])
                cube([esp_l-2, rail_t, post_h+rail_h]);

            // End stop
            translate([esp_xl-rail_t, esp_cy-esp_w/2, wall])
                cube([rail_t, esp_w, post_h+rail_h]);
        }
        // Clip to safe interior (3mm inset from inner walls)
        translate([wall+2, wall+2, wall])
            cube([body_w-2*wall-4, body_d-2*wall-4, body_h-wall-top_wall-2]);
    }
}

// ============== FEET (hollow to save material) ==============

module feet() {
    for (x = [foot_inset, body_w-foot_inset])
        for (y = [foot_inset, body_d-foot_inset])
            translate([x, y, -foot_h])
                difference() {
                    cylinder(d1=foot_d-1, d2=foot_d, h=foot_h);
                    translate([0,0,-eps])
                        cylinder(d=foot_d-3, h=foot_h-1);
                };
}

// ============== ASSEMBLY ==============

module main_body() {
    difference() {
        union() {
            main_body_shell();
            esp32_cradle();
            feet();
        }
        screen_cuts();
        cheek_details();
        nose_dot();
        usb_cut();
        ldr_cut();
    }
}

module back_panel() { back_panel_shell(); }

// ==================== RENDER ====================
// Color: Matte Jet Black exterior + Warm Orange interior/panel

if (part == "body") {
    color([0.12, 0.12, 0.12]) main_body();    // Matte Jet Black
} else if (part == "panel") {
    // Print orientation: outer face on bed
    // Print outer shell in black, inner face in orange
    color([0.95, 0.55, 0.15])                  // Warm Orange
        translate([body_w/2, body_h/2, body_d - split_y])
            rotate([90,0,0])
                translate([-body_w/2, -body_h/2, -body_d])
                    back_panel();
} else {
    // Assembled: black body, orange panel (orange peeks through screen)
    color([0.12, 0.12, 0.12]) main_body();     // Matte Jet Black
    color([0.95, 0.55, 0.15]) back_panel();    // Warm Orange panel
}
