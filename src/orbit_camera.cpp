#include "orbit_camera.hpp"
#include "config.hpp"
#include "settings.hpp"

#include <raylib.h>
#include <raymath.h>

auto orbit_camera::set_planar(const bool value) -> void
{
    if (value == planar) {
        return;
    }

    if (value) {
        planar_saved_projection = projection;
        planar_saved_angle_x = angle_x;
        planar_saved_angle_y = angle_y;
        projection = CAMERA_ORTHOGRAPHIC;
        angle_x = 0.0f;
        angle_y = 0.0f;
    } else {
        projection = planar_saved_projection;
        angle_x = planar_saved_angle_x;
        angle_y = planar_saved_angle_y;
    }
    planar = value;
}

auto orbit_camera::rotate(const Vector2 last_mouse, const Vector2 mouse) -> void
{
    if (planar) {
        return;
    }

    const auto [dx, dy] = Vector2Subtract(mouse, last_mouse);

    const simulation_settings cfg = get_settings();
    angle_x -= dx * cfg.rot_speed / 200.0f;
    angle_y += dy * cfg.rot_speed / 200.0f;

    angle_y = Clamp(angle_y, -1.5, 1.5); // Prevent flipping
}

auto orbit_camera::pan(const Vector2 last_mouse, const Vector2 mouse) -> void
{
    const auto [dx, dy] = Vector2Subtract(mouse, last_mouse);

    const simulation_settings cfg = get_settings();
    float speed;
    if (IsKeyDown(KEY_LEFT_SHIFT)) {
        speed = distance * cfg.pan_speed / 1000.0f * PAN_MULTIPLIER;
    } else {
        speed = distance * cfg.pan_speed / 1000.0f;
    }

    // The panning needs to happen in camera coordinates, otherwise rotating the
    // camera breaks it
    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    const Vector3 up = Vector3Normalize(Vector3CrossProduct(right, forward));

    const Vector3 offset =
        Vector3Add(Vector3Scale(right, -dx * speed), Vector3Scale(up, dy * speed));

    target = Vector3Add(target, offset);
}

auto orbit_camera::update(const Vector3& current_target, const Vector3& mass_center,
                          const bool lock, const bool mass_center_lock) -> void
{
    const simulation_settings cfg = get_settings();
    set_planar(cfg.planar_graph);

    if (lock) {
        if (mass_center_lock) {
            target = Vector3MoveTowards(target, mass_center,
                                        cfg.camera_smooth_speed * GetFrameTime() *
                                            Vector3Length(Vector3Subtract(target, mass_center)));
        } else {
            target = Vector3MoveTowards(target, current_target,
                                        cfg.camera_smooth_speed * GetFrameTime() *
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

    if (projection == CAMERA_ORTHOGRAPHIC) {
        fov = Clamp(fov, MIN_FOV, MAX_ORTHO_FOV);
    } else {
        fov = Clamp(fov, MIN_FOV, MAX_PERSP_FOV);
    }

    camera.position = Vector3Add(target, Vector3(x, y, z));
    camera.target = target;
    camera.fovy = fov;
    camera.projection = projection;
}
