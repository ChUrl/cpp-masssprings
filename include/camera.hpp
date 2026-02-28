#ifndef CAMERA_HPP_
#define CAMERA_HPP_

#include "config.hpp"

#include <raylib.h>
#include <raymath.h>

class orbit_camera
{
public:
    Vector3 position = Vector3Zero();
    Vector3 target = Vector3Zero();
    float distance = CAMERA_DISTANCE;
    float fov = CAMERA_FOV;
    CameraProjection projection = CAMERA_PERSPECTIVE;
    float angle_x = 0.0;
    float angle_y = 0.0;

    Camera camera = Camera{Vector3(0, 0, -distance), target, Vector3(0, 1.0, 0), fov, projection};

public:
    auto rotate(Vector2 last_mouse, Vector2 mouse) -> void;

    auto pan(Vector2 last_mouse, Vector2 mouse) -> void;

    auto update(const Vector3& current_target, const Vector3& mass_center, bool lock,
                bool mass_center_lock) -> void;
};

#endif
