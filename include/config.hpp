#ifndef __CONFIG_HPP_
#define __CONFIG_HPP_

#include <raylib.h>

// Window
constexpr int WIDTH = 1300;
constexpr int HEIGHT = 1300;

// Camera Controls
constexpr float SIM_SPEED = 4.0;
constexpr float CAMERA_DISTANCE = 250.0;
constexpr float MIN_CAMERA_DISTANCE = 2.0;
constexpr float MAX_CAMERA_DISTANCE = 2000.0;
constexpr float ZOOM_SPEED = 10.0;
constexpr float PAN_SPEED = 2.0;
constexpr float ROT_SPEED = 1.0;

// Physics Engine
constexpr float SPRING_CONSTANT = 1.5;
constexpr float DAMPENING_CONSTANT = 0.8;
constexpr float REST_LENGTH = 1.0;
constexpr float REPULSION_FORCE = 0.1;
constexpr float REPULSION_RANGE = 5.0 * REST_LENGTH;
constexpr float VERLET_DAMPENING = 0.01; // [0, 1]

// Graph Drawing
constexpr float VERTEX_SIZE = 0.05;
constexpr Color VERTEX_COLOR = GREEN;
constexpr Color EDGE_COLOR = DARKGREEN;

// Klotski Drawing
constexpr int BOARD_PADDING = 5;
constexpr int BLOCK_PADDING = 5;
constexpr Color BLOCK_COLOR = DARKGREEN;
constexpr Color HL_BLOCK_COLOR = GREEN;
constexpr Color TARGET_BLOCK_COLOR = RED;
constexpr Color HL_TARGET_BLOCK_COLOR = ORANGE;

#endif
