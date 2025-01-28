/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "Munk2D Documentation", "index.html", [
    [ "Munk2D", "index.html", "index" ],
    [ "Manual", "guide.html", [
      [ "Intro", "guide.html#autotoc_md0", [
        [ "Why a C Library?", "guide.html#autotoc_md1", null ],
        [ "Limitations of a C API", "guide.html#autotoc_md2", null ]
      ] ],
      [ "Get it, Compile it", "guide.html#autotoc_md3", [
        [ "Hello Chipmunk (World)", "guide.html#autotoc_md8", null ],
        [ "Support", "guide.html#autotoc_md9", null ],
        [ "Contact", "guide.html#autotoc_md10", null ],
        [ "License", "guide.html#autotoc_md11", null ],
        [ "Links", "guide.html#autotoc_md12", null ]
      ] ],
      [ "Chipmunk2D Basics", "guide.html#autotoc_md13", [
        [ "Overview", "guide.html#autotoc_md14", null ],
        [ "Memory Management the Chipmunk way", "guide.html#autotoc_md15", null ],
        [ "Basic Types", "guide.html#autotoc_md16", null ],
        [ "Math the Chipmunk way", "guide.html#autotoc_md17", null ]
      ] ],
      [ "Chipmunk Vectors: <tt>cpVect</tt>", "guide.html#autotoc_md18", [
        [ "Struct Definition, Constants and Constructors", "guide.html#autotoc_md19", null ],
        [ "Operations", "guide.html#autotoc_md20", null ]
      ] ],
      [ "Chipmunk Axis Aligned Bounding Boxes: <tt>cpBB</tt>", "guide.html#autotoc_md21", [
        [ "Struct Definition and Constructors", "guide.html#autotoc_md22", null ],
        [ "Operations", "guide.html#autotoc_md23", null ]
      ] ],
      [ "Chipmunk Rigid Bodies: <tt>cpBody</tt>", "guide.html#autotoc_md24", [
        [ "Dynamic, Kinematic, and Static Bodies", "guide.html#autotoc_md25", null ],
        [ "Movement, Teleportation, and Velocity", "guide.html#autotoc_md26", null ],
        [ "Memory Management Functions", "guide.html#autotoc_md27", null ],
        [ "Creating Dynamic Bodies", "guide.html#autotoc_md28", null ],
        [ "Properties", "guide.html#autotoc_md29", null ],
        [ "Moment of Inertia and Area Helper Functions", "guide.html#autotoc_md30", null ],
        [ "Coordinate Conversion Functions", "guide.html#autotoc_md31", null ],
        [ "Velocity Conversion Functions", "guide.html#autotoc_md32", null ],
        [ "Applying Forces and Torques", "guide.html#autotoc_md33", null ],
        [ "Sleeping Functions", "guide.html#autotoc_md34", null ],
        [ "Iterators", "guide.html#autotoc_md35", null ],
        [ "Integration Callbacks", "guide.html#autotoc_md36", null ]
      ] ],
      [ "Chipmunk Collision Shapes: <tt>cpShape</tt>", "guide.html#autotoc_md37", [
        [ "Properties", "guide.html#autotoc_md38", null ],
        [ "Fast Collision Filtering using cpShapeFilter", "guide.html#autotoc_md39", null ],
        [ "Memory Management Functions", "guide.html#autotoc_md42", null ],
        [ "Misc Functions", "guide.html#autotoc_md43", null ],
        [ "Working With Circle Shapes", "guide.html#autotoc_md44", null ],
        [ "Working With Segment Shapes", "guide.html#autotoc_md45", null ],
        [ "Working With Polygon Shapes", "guide.html#autotoc_md46", [
          [ "Boxes", "guide.html#autotoc_md47", null ],
          [ "Poly Shape Helper Functions", "guide.html#autotoc_md48", null ],
          [ "Convex Hull Helper Functions", "guide.html#autotoc_md49", null ]
        ] ],
        [ "Modifying cpShapes", "guide.html#autotoc_md50", null ],
        [ "Notes", "guide.html#autotoc_md51", null ]
      ] ],
      [ "Chipmunk Spaces: <tt>cpSpace</tt>", "guide.html#autotoc_md52", [
        [ "What Are Iterations, and Why Should I Care?", "guide.html#autotoc_md53", null ],
        [ "Sleeping", "guide.html#autotoc_md54", null ],
        [ "Properties", "guide.html#autotoc_md55", null ],
        [ "Memory Management Functions", "guide.html#autotoc_md56", null ],
        [ "Operations", "guide.html#autotoc_md57", null ],
        [ "Spatial Indexing", "guide.html#autotoc_md58", null ],
        [ "Iterators", "guide.html#autotoc_md59", null ],
        [ "Simulating the Space", "guide.html#autotoc_md60", null ],
        [ "Enabling and Tuning the Spatial Hash", "guide.html#autotoc_md61", null ],
        [ "Notes", "guide.html#autotoc_md62", null ]
      ] ],
      [ "Chipmunk Constraints: <tt>cpConstraint</tt>", "guide.html#autotoc_md63", [
        [ "What constraints are and what they are not", "guide.html#autotoc_md64", null ],
        [ "Properties", "guide.html#autotoc_md65", null ],
        [ "Error correction by Feedback", "guide.html#autotoc_md66", null ],
        [ "Constraints and Collision Shapes", "guide.html#autotoc_md67", null ],
        [ "Video Tour of Current Joint Types. (Requires connection to YouTube)", "guide.html#autotoc_md68", null ],
        [ "Shared Memory Management Functions", "guide.html#autotoc_md69", null ]
      ] ],
      [ "Constraint Types", "guide.html#autotoc_md70", [
        [ "Pin Joints", "guide.html#autotoc_md71", [
          [ "Properties", "guide.html#autotoc_md72", null ]
        ] ],
        [ "Slide Joints", "guide.html#autotoc_md73", [
          [ "Properties", "guide.html#autotoc_md74", null ]
        ] ],
        [ "Pivot Joints", "guide.html#autotoc_md75", [
          [ "Properties", "guide.html#autotoc_md76", null ]
        ] ],
        [ "Groove Joint", "guide.html#autotoc_md77", [
          [ "Properties", "guide.html#autotoc_md78", null ]
        ] ],
        [ "Damped Spring", "guide.html#autotoc_md79", [
          [ "Properties", "guide.html#autotoc_md80", null ]
        ] ],
        [ "Damped Rotary Spring", "guide.html#autotoc_md81", [
          [ "Properties", "guide.html#autotoc_md82", null ]
        ] ],
        [ "Rotary Limit Joint", "guide.html#autotoc_md83", [
          [ "Properties", "guide.html#autotoc_md84", null ]
        ] ],
        [ "Ratchet Joint", "guide.html#autotoc_md85", [
          [ "Properties", "guide.html#autotoc_md86", null ]
        ] ],
        [ "Gear Joint", "guide.html#autotoc_md87", [
          [ "Properties", "guide.html#autotoc_md88", null ]
        ] ],
        [ "Simple Motor", "guide.html#autotoc_md89", [
          [ "Properties", "guide.html#autotoc_md90", null ]
        ] ],
        [ "Notes", "guide.html#autotoc_md91", null ]
      ] ],
      [ "Overview of Collision Detection in Chipmunk", "guide.html#autotoc_md92", [
        [ "Spatial Indexing", "guide.html#autotoc_md93", null ],
        [ "Fast Collision Filtering", "guide.html#autotoc_md94", null ],
        [ "Constraint Based Filtering", "guide.html#autotoc_md95", null ],
        [ "Primitive Shape to Shape Collision Detection", "guide.html#autotoc_md96", null ],
        [ "Collision Handler Filtering", "guide.html#autotoc_md97", null ]
      ] ],
      [ "Collision Callbacks", "guide.html#autotoc_md98", [
        [ "Collision Handlers", "guide.html#autotoc_md99", null ],
        [ "Collision Handler API", "guide.html#autotoc_md100", null ],
        [ "Post-Step Callbacks", "guide.html#autotoc_md101", null ],
        [ "Examples", "guide.html#autotoc_md102", null ]
      ] ],
      [ "Chipmunk Collision Pairs: <tt>cpArbiter</tt>", "guide.html#autotoc_md103", [
        [ "Memory Management", "guide.html#autotoc_md104", null ],
        [ "Properties", "guide.html#autotoc_md105", null ],
        [ "Contact Point Sets", "guide.html#autotoc_md106", null ],
        [ "Helper Functions", "guide.html#autotoc_md107", null ]
      ] ],
      [ "Queries", "guide.html#autotoc_md108", [
        [ "Nearest Point Queries", "guide.html#autotoc_md109", null ],
        [ "Segment Queries", "guide.html#autotoc_md110", null ],
        [ "AABB Queries", "guide.html#autotoc_md111", null ],
        [ "Shape Queries", "guide.html#autotoc_md112", null ],
        [ "Blocks", "guide.html#autotoc_md113", null ],
        [ "Examples", "guide.html#autotoc_md114", null ]
      ] ]
    ] ],
    [ "Changelog", "md__2home_2runner_2work_2_munk2_d_2_munk2_d_2_c_h_a_n_g_e_l_o_g.html", [
      [ "UPCOMING 8.0 (Munk2D Fork):", "md__2home_2runner_2work_2_munk2_d_2_munk2_d_2_c_h_a_n_g_e_l_o_g.html#autotoc_md126", null ],
      [ "7.0.3", "md__2home_2runner_2work_2_munk2_d_2_munk2_d_2_c_h_a_n_g_e_l_o_g.html#autotoc_md127", null ],
      [ "7.0.2", "md__2home_2runner_2work_2_munk2_d_2_munk2_d_2_c_h_a_n_g_e_l_o_g.html#autotoc_md128", null ],
      [ "7.0.1", "md__2home_2runner_2work_2_munk2_d_2_munk2_d_2_c_h_a_n_g_e_l_o_g.html#autotoc_md129", null ],
      [ "7.0.0", "md__2home_2runner_2work_2_munk2_d_2_munk2_d_2_c_h_a_n_g_e_l_o_g.html#autotoc_md130", null ],
      [ "6.2.2", "md__2home_2runner_2work_2_munk2_d_2_munk2_d_2_c_h_a_n_g_e_l_o_g.html#autotoc_md131", null ],
      [ "6.2.1", "md__2home_2runner_2work_2_munk2_d_2_munk2_d_2_c_h_a_n_g_e_l_o_g.html#autotoc_md132", null ],
      [ "6.2.0", "md__2home_2runner_2work_2_munk2_d_2_munk2_d_2_c_h_a_n_g_e_l_o_g.html#autotoc_md133", null ],
      [ "6.1.5", "md__2home_2runner_2work_2_munk2_d_2_munk2_d_2_c_h_a_n_g_e_l_o_g.html#autotoc_md134", null ],
      [ "6.1.4", "md__2home_2runner_2work_2_munk2_d_2_munk2_d_2_c_h_a_n_g_e_l_o_g.html#autotoc_md135", null ],
      [ "6.1.3", "md__2home_2runner_2work_2_munk2_d_2_munk2_d_2_c_h_a_n_g_e_l_o_g.html#autotoc_md136", null ],
      [ "6.1.2", "md__2home_2runner_2work_2_munk2_d_2_munk2_d_2_c_h_a_n_g_e_l_o_g.html#autotoc_md137", null ],
      [ "6.1.1", "md__2home_2runner_2work_2_munk2_d_2_munk2_d_2_c_h_a_n_g_e_l_o_g.html#autotoc_md138", null ],
      [ "6.1.0", "md__2home_2runner_2work_2_munk2_d_2_munk2_d_2_c_h_a_n_g_e_l_o_g.html#autotoc_md139", null ],
      [ "6.0.3", "md__2home_2runner_2work_2_munk2_d_2_munk2_d_2_c_h_a_n_g_e_l_o_g.html#autotoc_md140", null ],
      [ "6.0.2", "md__2home_2runner_2work_2_munk2_d_2_munk2_d_2_c_h_a_n_g_e_l_o_g.html#autotoc_md141", null ],
      [ "6.0.1", "md__2home_2runner_2work_2_munk2_d_2_munk2_d_2_c_h_a_n_g_e_l_o_g.html#autotoc_md142", null ],
      [ "6.0.0", "md__2home_2runner_2work_2_munk2_d_2_munk2_d_2_c_h_a_n_g_e_l_o_g.html#autotoc_md143", null ],
      [ "5.3.5", "md__2home_2runner_2work_2_munk2_d_2_munk2_d_2_c_h_a_n_g_e_l_o_g.html#autotoc_md144", null ],
      [ "5.3.4", "md__2home_2runner_2work_2_munk2_d_2_munk2_d_2_c_h_a_n_g_e_l_o_g.html#autotoc_md145", null ],
      [ "5.3.3", "md__2home_2runner_2work_2_munk2_d_2_munk2_d_2_c_h_a_n_g_e_l_o_g.html#autotoc_md146", null ],
      [ "5.3.2", "md__2home_2runner_2work_2_munk2_d_2_munk2_d_2_c_h_a_n_g_e_l_o_g.html#autotoc_md147", null ],
      [ "5.3.1", "md__2home_2runner_2work_2_munk2_d_2_munk2_d_2_c_h_a_n_g_e_l_o_g.html#autotoc_md148", null ],
      [ "5.3.0", "md__2home_2runner_2work_2_munk2_d_2_munk2_d_2_c_h_a_n_g_e_l_o_g.html#autotoc_md149", null ],
      [ "5.2.0", "md__2home_2runner_2work_2_munk2_d_2_munk2_d_2_c_h_a_n_g_e_l_o_g.html#autotoc_md150", null ],
      [ "5.1.0", "md__2home_2runner_2work_2_munk2_d_2_munk2_d_2_c_h_a_n_g_e_l_o_g.html#autotoc_md151", null ],
      [ "5.0.0", "md__2home_2runner_2work_2_munk2_d_2_munk2_d_2_c_h_a_n_g_e_l_o_g.html#autotoc_md152", null ]
    ] ],
    [ "LICENSE", "md__2home_2runner_2work_2_munk2_d_2_munk2_d_2_l_i_c_e_n_s_e.html", null ],
    [ "Topics", "topics.html", "topics" ],
    [ "Data Structures", "annotated.html", [
      [ "Data Structures", "annotated.html", "annotated_dup" ],
      [ "Data Structure Index", "classes.html", null ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Data Fields", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Functions", "functions_func.html", null ],
        [ "Variables", "functions_vars.html", null ],
        [ "Typedefs", "functions_type.html", null ],
        [ "Properties", "functions_prop.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"_chipmunk_auto_geometry_8h_source.html",
"group__cp_damped_spring.html#ga5a663e7d5e0c88f26719af1d543c1db8",
"group__cp_vect.html#ga0fd46c6ef6dcca0dc7b6c6cef8008758",
"interface_chipmunk_c_g_context_sampler.html#a60d99d43cd8cf5104fd3112ea71f4fcd",
"structcp_circle_shape.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';