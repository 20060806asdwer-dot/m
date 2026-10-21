// 【25 结构草模：盒体、磁芯、电子模块与装配占位】
// Layout concept only. Units: mm. Verify purchased components before printing.
// Animation: OpenSCAD View > Animate, FPS 20, Steps 100.
// No manufacturing holes, isolation, sealing or tolerance verification is implied.
$fn = 64;
explode = 0; // 0..1. Override manually, or replace with $t for animation.
core_open = 0; // degrees, 0..75
case_l = 110;
case_w = 75;
case_h = 45;
wall = 3;
core_R = 45;
core_r = 8;

module block(p, s, c) { color(c) translate(p) cube(s, center=true); }
module ring_half(upper=true) {
    intersection() {
        rotate([90,0,0]) rotate_extrude() translate([core_R,0]) circle(core_r);
        translate([0,0,upper ? 55 : -55]) cube([120,40,110], center=true);
    }
}
module core() {
    color("SlateGray") translate([-54,0,65]) ring_half(false);
    color("LightSlateGray")
        translate([-99,0,65]) rotate([0,-core_open,0]) translate([45,0,0]) ring_half(true);
    // Winding envelope is schematic. Turn count is determined by experiments.
    color("Peru") translate([-54,0,20]) cube([24,21,12], center=true);
}
module case_base() {
    color([.68,.81,.72,.4]) translate([43,0,27]) difference() {
        cube([case_l,case_w,case_h], center=true);
        translate([0,0,wall]) cube([case_l-2*wall,case_w-2*wall,case_h], center=true);
    }
}
module electronics() {
    block([10,-17,25+explode*23],[32,30,2],"SeaGreen");
    block([7,-20,29+explode*23],[8,8,5],"DarkSlateGray");
    color("DarkSlateGray") translate([18,20,15+explode*24]) cylinder(h=25,r=10);
    color("Silver") translate([18,20,40+explode*24]) cylinder(h=1,r=9);
    block([58,-13,32+explode*44],[44,24,2],"SteelBlue");
    block([56,-13,35+explode*44],[11,11,3],"DarkSlateGray");
    block([84,-23,34+explode*44],[14,12,2],"SeaGreen");
    block([67,23,37+explode*60],[36,21,2],"SeaGreen");
    block([67,23,40+explode*60],[24,15,4],"Silver");
    color("DarkSlateGray") translate([87,24,38+explode*60]) cylinder(h=48,r=2);
}
module lid() {
    block([43,0,52+explode*79],[case_l,case_w,5],[.8,.88,.82,.55]);
}
core(); case_base(); electronics(); lid();
// Primary conductor only. Isolated low-voltage AC source and return are omitted.
color("SaddleBrown") translate([-54,0,65]) rotate([90,0,0]) cylinder(h=180,r=4,center=true);
