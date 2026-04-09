// JARVIS Device — Parametric 3D Printable Case
// OpenSCAD parametric design for ESP32-S3 + INMP441 + WS2812B room node
// 
// Usage: OpenSCAD or http://OpenSCAD.org (free, cross-platform)
//   1. Open this file
//   2. Adjust parameters below if needed
//   3. Preview (F5) → Export STL (F7)
//
// 3D Print Settings:
//   - Layer height: 0.2mm
//   - Infill: 15-20%
//   - No supports needed (designed for bottom-up print)
//   - Material: PLA or PETG

// ============ PARAMETERS — Adjust these ============

// Case dimensions
case_wall = 2.5;          // Wall thickness in mm
base_h    = 4.0;          // Base plate height
top_h     = 3.0;          // Top plate height
lid_lip   = 1.5;          // How far the lid overlaps the base

// Internal cavity (must fit ESP32-S3 DevKit + components)
inner_w = 28.0;          // Inner width
inner_d = 52.0;           // Inner depth (longer for DevKitC-1)
inner_h = 12.0;           // Inner height

// Component cutouts
esp32_screw_r = 1.75;     // M2 screw radius for mounting holes
esp32_screw_positions = [[6, 6], [6, 44], [20, 6], [20, 44]];

// LED ring dimensions
led_ring_od   = 68.0;     // Outer diameter (65mm ring + clearance)
led_ring_h    = 3.0;      // LED ring height

// Mic hole
mic_hole_d = 8.0;         // Diameter for INMP441 hole

// BME280 sensor (optional)
bme280_w = 12.0;
bme280_d = 12.0;

// USB-C cutout
usbc_w = 10.0;
usbc_h = 4.0;

// Speaker hole (optional)
speaker_d = 36.0;

// Ventilation holes
vent_hole_d = 3.0;
vent_spacing = 8.0;

// ============ CALCULATED VALUES ============
outer_w = inner_w + case_wall * 2;
outer_d = inner_d + case_wall * 2;
outer_h = base_h + inner_h + top_h;
led_ring_r = led_ring_od / 2;
led_clearance = led_ring_r + case_wall + 2; // LED ring sits on top

total_case_d = max(outer_d, led_ring_od + case_wall * 2);

// PCB mounting posts
pcb_thickness = 1.6;
post_r = 2.5;
post_h = 3.0;
post_positions = [
    [esp32_screw_positions[0][0] + case_wall, esp32_screw_positions[0][1] + case_wall],
    [esp32_screw_positions[1][0] + case_wall, esp32_screw_positions[1][1] + case_wall],
    [esp32_screw_positions[2][0] + case_wall, esp32_screw_positions[2][1] + case_wall],
    [esp32_screw_positions[3][0] + case_wall, esp32_screw_positions[3][1] + case_wall],
];

// ============ MODULES ============

module rounded_cube(w, d, h, r) {
    hull() {
        for (x = [r, w - r], y = [r, d - r]) {
            translate([x, y, r])
                cylinder(r=r, h=h - r);
        }
    }
}

module base_case() {
    difference() {
        // Outer shell
        rounded_cube(outer_w, total_case_d, base_h + inner_h, 3);
        
        // Inner cavity
        translate([case_wall, case_wall, base_h])
            rounded_cube(inner_w, inner_d, inner_h + 0.1, 2);
        
        // LED ring recess (on top face)
        translate([(outer_w - led_ring_od)/2, (total_case_d - led_ring_od)/2, 0])
            cylinder(d=led_ring_od - 0.5, h=base_h + 0.1);
        
        // USB-C cutout (right side)
        translate([outer_w - case_wall - usbc_w/2, total_case_d - 8, base_h + inner_h/2 - usbc_h/2])
            hull() {
                translate([0, 0, 0])
                    cylinder(d=usbc_w, h=usbc_h + 1);
                translate([0, 5, 0])
                    cylinder(d=usbc_w, h=usbc_h + 1);
            }
        
        // Ventilation holes (left side)
        for (y = [case_wall + 3 : vent_spacing : total_case_d - 3]) {
            translate([-0.5, y, base_h + inner_h/2])
                rotate([90, 0, 0])
                    cylinder(d=vent_hole_d, h=case_wall + 1);
        }
    }
}

module led_ring_mount() {
    // LED ring recess + screw holes
    translate([(outer_w - led_ring_od)/2 - case_wall, (total_case_d - led_ring_od)/2 - case_wall, base_h + inner_h])
    difference() {
        // Mounting plate for LED ring
        rounded_cube(led_ring_od + case_wall*2, led_ring_od + case_wall*2, top_h + led_ring_h, 3);
        
        // Center hole (LED ring goes through)
        cylinder(d=led_ring_od + 0.5, h=top_h + led_ring_h + 1);
        
        // LED ring mounting holes (3 holes at 120° apart)
        for (angle = [0, 120, 240]) {
            rotate([0, 0, angle])
                translate([led_ring_r - 5, 0, -0.5])
                    cylinder(d=3, h=top_h + 1);
        }
    }
}

module pcb_mounting_posts() {
    for (pos = post_positions) {
        translate([pos[0], pos[1], 0])
            cylinder(r=post_r, h=post_h);
    }
}

module mic_hole() {
    translate([case_wall + inner_w/2, case_wall + 4, -0.1])
        cylinder(d=mic_hole_d, h=base_h + 0.2);
}

module base_assembly() {
    base_case();
    pcb_mounting_posts();
    mic_hole();
}

// ============ LID (snap-fit) ============
module lid() {
    difference() {
        // Lid body
        translate([-1, -1, base_h + inner_h - lid_lip])
            rounded_cube(outer_w + 2, total_case_d + 2, top_h + lid_lip, 3);
        
        // Recess for electronics
        translate([case_wall - 0.5, case_wall - 0.5, base_h + inner_h - lid_lip + case_wall])
            rounded_cube(inner_w + 1, inner_d + 1, top_h + 1, 2);
        
        // Speaker hole (optional, centered in lid)
        translate([outer_w/2, total_case_d/2, -1])
            cylinder(d=speaker_d, h=top_h + lid_lip + 2);
        
        // Mic hole in lid (for top-facing mic)
        translate([outer_w/2, total_case_d - 6, -1])
            cylinder(d=mic_hole_d + 2, h=top_h + lid_lip + 2);
    }
}

// ============ SPEAKER GRILLE (optional lid texture) ============
module speaker_grille(count=20) {
    translate([outer_w/2, total_case_d/2, 0])
    for (x = [-count/2 : count/2], y = [-count/2 : count/2]) {
        if (sqrt(x*x + y*y) < count/2) {
            translate([x * 3, y * 3, 0])
                cylinder(d=1.8, h=top_h + lid_lip + 1);
        }
    }
}

// ============ ASSEMBLY VIEW ============
$fn = 64; // Smoothness

// Toggle to show assembled or exploded view
show_exploded = false;

if (show_exploded) {
    // Exploded view for visualization
    translate([0, 0, 0]) base_assembly();
    translate([0, 0, 25]) led_ring_mount();
    translate([0, 0, 35]) lid();
} else {
    // Assembled case
    translate([0, 0, 0]) base_assembly();
    translate([0, 0, 0]) led_ring_mount();
}

// Speaker grille (comment out to remove)
// translate([0, 0, base_h + inner_h - lid_lip]) speaker_grille();

// ============ INFO ============
echo("JARVIS Device Case Dimensions:");
echo(str("Outer: ", outer_w, " x ", total_case_d, " x ", outer_h, " mm"));
echo(str("Inner cavity: ", inner_w, " x ", inner_d, " x ", inner_h, " mm"));
echo(str("LED ring diameter: ", led_ring_od, " mm"));
echo(str("Volume: ", outer_w * total_case_d * outer_h / 1000, " cm³"));
