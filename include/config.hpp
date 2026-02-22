#ifndef __CONFIG_HPP_
#define __CONFIG_HPP_

#include <raylib.h>

#define PRINT_TIMINGS
// #define WEB // Disables multithreading
#define BARNES_HUT // Use octree BH instead of uniform grid

// Window
constexpr int INITIAL_WIDTH = 800;
constexpr int INITIAL_HEIGHT = 800;
constexpr int MENU_HEIGHT = 200;

// Menu
constexpr int MENU_PAD = 5;
constexpr int BUTTON_PAD = 20;
constexpr int MENU_ROWS = 3;
constexpr int MENU_COLS = 3;

// Camera Controls
constexpr float CAMERA_FOV = 120.0;
constexpr float CAMERA_DISTANCE = 20.0;
constexpr float MIN_CAMERA_DISTANCE = 2.0;
constexpr float MAX_CAMERA_DISTANCE = 2000.0;
constexpr float ZOOM_SPEED = 2.5;
constexpr float ZOOM_MULTIPLIER = 4.0;
constexpr float PAN_SPEED = 2.0;
constexpr float PAN_MULTIPLIER = 10.0;
constexpr float ROT_SPEED = 1.0;

// Physics Engine
constexpr float SIM_SPEED = 4.0;          // How large each update should be
constexpr float TIMESTEP = 1.0 / 60;      // Do 60 physics updates per second
constexpr float MASS = 1.0;               // Mass spring system
constexpr float SPRING_CONSTANT = 5.0;    // Mass spring system
constexpr float DAMPENING_CONSTANT = 1.0; // Mass spring system
constexpr float REST_LENGTH = 2.0;        // Mass spring system
constexpr float VERLET_DAMPENING = 0.05;  // [0, 1]
constexpr float BH_FORCE = 2.0;           // BH: [1.0, 3.0]
constexpr float THETA = 1.0;              // Barnes-Hut [0.5, ~]
constexpr float SOFTENING = 0.01;         // Barnes-Hut [0.01, 1.0]
constexpr float GRID_FORCE = 0.02;        // Grid: [0.0, ~0.05]
constexpr float REPULSION_RANGE = 5.0 * REST_LENGTH; // Grid
constexpr int REPULSION_GRID_REFRESH = 5;            // Grid rebuild freq

// Graph Drawing
constexpr float VERTEX_SIZE = 0.5;
constexpr Color VERTEX_COLOR = GREEN;
constexpr Color EDGE_COLOR = DARKGREEN;
constexpr int DRAW_VERTICES_LIMIT = 1000000;

// Klotski Drawing
constexpr int BOARD_PADDING = 5;
constexpr int BLOCK_PADDING = 5;
constexpr Color BLOCK_COLOR = DARKGREEN;
constexpr Color HL_BLOCK_COLOR = GREEN;
constexpr Color TARGET_BLOCK_COLOR = RED;
constexpr Color HL_TARGET_BLOCK_COLOR = ORANGE;

#endif
