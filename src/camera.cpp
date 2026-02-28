#include "camera.hpp"
#include "config.hpp"

#include <raylib.h>
#include <raymath.h>

#ifdef TRACY
    #include <tracy/Tracy.hpp>
#endif

auto orbit_camera::rotate(const Vector2 last_mouse, const Vector2 mouse) -> void
{
    const auto [dx, dy] = Vector2Subtract(mouse, last_mouse);

    angle_x -= dx * ROT_SPEED / 200.0f;
    angle_y += dy * ROT_SPEED / 200.0f;

    angle_y = Clamp(angle_y, -1.5, 1.5); // Prevent flipping
}

auto orbit_camera::pan(const Vector2 last_mouse, const Vector2 mouse) -> void
{
    const auto [dx, dy] = Vector2Subtract(mouse, last_mouse);

    float speed;
    if (IsKeyDown(KEY_LEFT_SHIFT)) {
        speed = distance * PAN_SPEED / 1000.0f * PAN_MULTIPLIER;
    } else {
        speed = distance * PAN_SPEED / 1000.0f;
    }

    // The panning needs to happen in camera coordinates, otherwise rotating the
    // camera breaks it
    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    const Vector3 up = Vector3Normalize(Vector3CrossProduct(right, forward));

    const Vector3 offset = Vector3Add(Vector3Scale(right, -dx * speed), Vector3Scale(up, dy * speed));

    target = Vector3Add(target, offset);
}

auto orbit_camera::update(const Vector3& current_target, const Vector3& mass_center, const bool lock,
                          const bool mass_center_lock) -> void
{
    if (lock) {
        if (mass_center_lock) {
            target = Vector3MoveTowards(target, mass_center,
                                        CAMERA_SMOOTH_SPEED * GetFrameTime() *
                                            Vector3Length(Vector3Subtract(target, mass_center)));
        } else {
            target = Vector3MoveTowards(target, current_target,
                                        CAMERA_SMOOTH_SPEED * GetFrameTime() *
                                            Vector3Length(Vector3Subtract(target, current_target)));
        }
    }

    distance = Clamp(distance, MIN_CAMERA_DISTANCE, MAX_CAMERA_DISTANCE);
    float actual_distance = distance;
    if (projection == CAMERA_ORTHOGRAPHIC) {
        actual_distance = MAX_CAMERA_DISTANCE;
    }

    // Spherical coordinates
    const float x = cos(angle_y) * sin(angle_x) * actual_distance;
    const float y = sin(angle_y) * actual_distance;
    const float z = cos(angle_y) * cos(angle_x) * actual_distance;

    fov = Clamp(fov, MIN_FOV, MAX_FOV);

    camera.position = Vector3Add(target, Vector3(x, y, z));
    camera.target = target;
    camera.fovy = fov;
    camera.projection = projection;
}
