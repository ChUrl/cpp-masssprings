#ifndef __CONFIG_HPP_
#define __CONFIG_HPP_

#include <raylib.h>

constexpr int WIDTH = 800;
constexpr int HEIGHT = 800;

constexpr float VERTEX_SIZE = 5.0;
constexpr Color VERTEX_COLOR = {27, 188, 104, 255};
constexpr Color EDGE_COLOR = {20, 133, 38, 255};

constexpr float SIM_SPEED = 4.0;
constexpr float CAMERA_DISTANCE = 2.2;

constexpr float DEFAULT_SPRING_CONSTANT = 1.5;
constexpr float DEFAULT_DAMPENING_CONSTANT = 0.1;
constexpr float DEFAULT_REST_LENGTH = 0.5;

#endif
