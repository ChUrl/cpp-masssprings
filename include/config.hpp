#ifndef __CONFIG_HPP_
#define __CONFIG_HPP_

#include <raylib.h>

constexpr int WIDTH = 800;
constexpr int HEIGHT = 800;

constexpr float VERTEX_SIZE = 0.1;
constexpr Color VERTEX_COLOR = GREEN;
constexpr Color EDGE_COLOR = DARKGREEN;

constexpr float SIM_SPEED = 4.0;
constexpr float ROTATION_SPEED = 1.0;
constexpr float CAMERA_DISTANCE = 2.2;
constexpr float CULLING_TOLERANCE = 0.1; // percentage

constexpr float DEFAULT_SPRING_CONSTANT = 1.5;
constexpr float DEFAULT_DAMPENING_CONSTANT = 0.1;
constexpr float DEFAULT_REST_LENGTH = 0.5;
constexpr float DEFAULT_REPULSION_FORCE = 0.01;

constexpr int BOARD_PADDING = 5;
constexpr int BLOCK_PADDING = 5;
constexpr Color BLOCK_COLOR = DARKGREEN;
constexpr Color HL_BLOCK_COLOR = GREEN;
constexpr Color TARGET_BLOCK_COLOR = RED;
constexpr Color HL_TARGET_BLOCK_COLOR = ORANGE;

#endif
