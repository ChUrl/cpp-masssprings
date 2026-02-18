#ifndef __RENDERER_HPP_
#define __RENDERER_HPP_

#include <immintrin.h>
#include <raylib.h>
#include <raymath.h>

#include "config.hpp"
#include "klotski.hpp"
#include "mass_springs.hpp"

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
  OrbitCamera3D(Vector3 target, float distance)
      : camera({0}), target(target), distance(distance), angle_x(0.0),
        angle_y(0.3), last_mouse(Vector2Zero()), dragging(false),
        panning(false), target_lock(true) {
    camera.position = Vector3(0, 0, -1.0 * distance);
    camera.target = target;
    camera.up = Vector3(0, 1.0, 0);
    camera.fovy = 90.0;
    camera.projection = CAMERA_PERSPECTIVE;
  }

  ~OrbitCamera3D() {}

public:
  auto Update(const Mass &current_mass) -> void;
};

class Renderer {
private:
  OrbitCamera3D camera;
  RenderTexture render_target;
  RenderTexture klotski_target;
  RenderTexture menu_target;

public:
  Renderer() : camera(OrbitCamera3D(Vector3(0, 0, 0), CAMERA_DISTANCE)) {
    render_target = LoadRenderTexture(WIDTH, HEIGHT);
    klotski_target = LoadRenderTexture(WIDTH, HEIGHT);
    menu_target = LoadRenderTexture(WIDTH * 2, MENU_HEIGHT);
  }

  Renderer(const Renderer &copy) = delete;
  Renderer &operator=(const Renderer &copy) = delete;
  Renderer(Renderer &&move) = delete;
  Renderer &operator=(Renderer &&move) = delete;

  ~Renderer() {
    UnloadRenderTexture(render_target);
    UnloadRenderTexture(klotski_target);
    UnloadRenderTexture(menu_target);
  }

public:
  auto UpdateCamera(const MassSpringSystem &masssprings, const State &current)
      -> void;

  auto DrawMassSprings(const MassSpringSystem &masssprings,
                       const State &current) -> void;

  auto DrawKlotski(const State &state, int hov_x, int hov_y, int sel_x,
                   int sel_y) -> void;

  auto DrawMenu(const MassSpringSystem &masssprings) -> void;

  auto DrawTextures() -> void;
};

#endif
