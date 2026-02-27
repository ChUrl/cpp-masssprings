#include "camera.hpp"
#include "config.hpp"

#include <raylib.h>
#include <raymath.h>

#ifdef TRACY
#include "tracy.hpp"
#include <tracy/Tracy.hpp>
#endif

auto OrbitCamera3D::Rotate(Vector2 last_mouse, Vector2 mouse) -> void {
  Vector2 dx = Vector2Subtract(mouse, last_mouse);

  angle_x -= dx.x * ROT_SPEED / 200.0;
  angle_y += dx.y * ROT_SPEED / 200.0;

  angle_y = Clamp(angle_y, -1.5, 1.5); // Prevent flipping
}

auto OrbitCamera3D::Pan(Vector2 last_mouse, Vector2 mouse) -> void {
  Vector2 dx = Vector2Subtract(mouse, last_mouse);

  float speed;
  if (IsKeyDown(KEY_LEFT_SHIFT)) {
    speed = distance * PAN_SPEED / 1000.0 * PAN_MULTIPLIER;
  } else {
    speed = distance * PAN_SPEED / 1000.0;
  }

  // The panning needs to happen in camera coordinates, otherwise rotating the
  // camera breaks it
  Vector3 forward =
      Vector3Normalize(Vector3Subtract(camera.target, camera.position));
  Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
  Vector3 up = Vector3Normalize(Vector3CrossProduct(right, forward));

  Vector3 offset = Vector3Add(Vector3Scale(right, -dx.x * speed),
                              Vector3Scale(up, dx.y * speed));

  target = Vector3Add(target, offset);
}

auto OrbitCamera3D::Update(const Vector3 &current_target, bool lock) -> void {
  if (lock) {
    target = Vector3MoveTowards(
        target, current_target,
        CAMERA_SMOOTH_SPEED * GetFrameTime() *
            Vector3Length(Vector3Subtract(target, current_target)));
  }

  distance = Clamp(distance, MIN_CAMERA_DISTANCE, MAX_CAMERA_DISTANCE);
  int actual_distance = distance;
  if (projection == CAMERA_ORTHOGRAPHIC) {
    actual_distance = MAX_CAMERA_DISTANCE;
  }

  // Spherical coordinates
  float x = cos(angle_y) * sin(angle_x) * actual_distance;
  float y = sin(angle_y) * actual_distance;
  float z = cos(angle_y) * cos(angle_x) * actual_distance;

  fov = Clamp(fov, 25.0, 155.0);

  camera.position = Vector3Add(target, Vector3(x, y, z));
  camera.target = target;
  camera.fovy = fov;
  camera.projection = projection;
}
