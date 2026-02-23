#include "camera.hpp"
#include "config.hpp"

#include <raylib.h>
#include <raymath.h>

auto OrbitCamera3D::HandleCameraInput() -> Vector2 {
  Vector2 mouse = GetMousePosition();
  if (mouse.x >= GetScreenWidth() / 2.0 && mouse.y >= MENU_HEIGHT) {
    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
      rotating = true;
      last_mouse = mouse;
    } else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      panning = true;
      target_lock = false;
      last_mouse = mouse;
    }

    // Zoom
    float wheel = GetMouseWheelMove();
    if (IsKeyDown(KEY_LEFT_SHIFT)) {
      distance -= wheel * ZOOM_SPEED * ZOOM_MULTIPLIER;
    } else {
      distance -= wheel * ZOOM_SPEED;
    }
  }

  if (IsMouseButtonReleased(MOUSE_RIGHT_BUTTON)) {
    rotating = false;
  }
  if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
    panning = false;
  }

  if (IsKeyPressed(KEY_L)) {
    target_lock = !target_lock;
  }
  return mouse;
}

auto OrbitCamera3D::Update(const Vector3 &current_target) -> void {
  Vector2 mouse = HandleCameraInput();

  if (rotating) {
    Vector2 dx = Vector2Subtract(mouse, last_mouse);
    last_mouse = mouse;

    angle_x -= dx.x * ROT_SPEED / 200.0;
    angle_y += dx.y * ROT_SPEED / 200.0;

    angle_y = Clamp(angle_y, -1.5, 1.5); // Prevent flipping
  }

  if (panning) {
    Vector2 dx = Vector2Subtract(mouse, last_mouse);
    last_mouse = mouse;

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

  if (target_lock) {
    target = Vector3MoveTowards(
        target, current_target,
        CAMERA_SMOOTH_SPEED * GetFrameTime() *
            Vector3Length(Vector3Subtract(target, current_target)));
  }

  distance = Clamp(distance, MIN_CAMERA_DISTANCE, MAX_CAMERA_DISTANCE);

  // Spherical coordinates
  float x = cos(angle_y) * sin(angle_x) * distance;
  float y = sin(angle_y) * distance;
  float z = cos(angle_y) * cos(angle_x) * distance;

  camera.position = Vector3Add(target, Vector3(x, y, z));
  camera.target = target;
}
