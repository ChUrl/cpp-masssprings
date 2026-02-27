#ifndef __CAMERA_HPP_
#define __CAMERA_HPP_

#include "config.hpp"

#include <raylib.h>
#include <raymath.h>

class OrbitCamera3D {
public:
  Vector3 position = Vector3Zero();
  Vector3 target = Vector3Zero();
  float distance = CAMERA_DISTANCE;
  float fov = CAMERA_FOV;
  CameraProjection projection = CAMERA_PERSPECTIVE;
  float angle_x = 0.0;
  float angle_y = 0.0;

  Camera camera = Camera{Vector3(0, 0, -distance), target, Vector3(0, 1.0, 0),
                         fov, projection};

public:
  auto Rotate(Vector2 last_mouse, Vector2 mouse) -> void;

  auto Pan(Vector2 last_mouse, Vector2 mouse) -> void;

  auto Update(const Vector3 &current_target, bool lock) -> void;
};

#endif
