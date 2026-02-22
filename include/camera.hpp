#ifndef __CAMERA_HPP_
#define __CAMERA_HPP_

#include <raylib.h>
#include <raymath.h>

#include "config.hpp"

class OrbitCamera3D {
  friend class Renderer;

private:
  Camera camera;
  Vector3 target;
  float distance;
  float angle_x;
  float angle_y;
  Vector2 last_mouse;
  bool dragging;
  bool panning;
  bool target_lock;

public:
  OrbitCamera3D()
      : camera({0}), target(Vector3Zero()), distance(CAMERA_DISTANCE),
        angle_x(0.0), angle_y(0.0), last_mouse(Vector2Zero()), dragging(false),
        panning(false), target_lock(true) {
    camera.position = Vector3(0, 0, -1.0 * distance);
    camera.target = target;
    camera.up = Vector3(0, 1.0, 0);
    camera.fovy = CAMERA_FOV;
    camera.projection = CAMERA_PERSPECTIVE;
  }

  ~OrbitCamera3D() {}

public:
  auto Update(const Vector3 &current_target) -> void;
};

#endif
