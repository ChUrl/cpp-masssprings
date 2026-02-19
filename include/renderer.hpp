#ifndef __RENDERER_HPP_
#define __RENDERER_HPP_

#include <immintrin.h>
#include <raylib.h>
#include <raymath.h>
#include <unordered_set>

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
        angle_y(0.0), last_mouse(Vector2Zero()), dragging(false),
        panning(false), target_lock(true) {
    camera.position = Vector3(0, 0, -1.0 * distance);
    camera.target = target;
    camera.up = Vector3(0, 1.0, 0);
    camera.fovy = CAMERA_FOV;
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
  std::unordered_set<State> winning_states;

public:
  bool mark_solutions;
  bool connect_solutions;

public:
  Renderer()
      : camera(OrbitCamera3D(Vector3(0, 0, 0), CAMERA_DISTANCE)),
        mark_solutions(false), connect_solutions(false) {
    render_target = LoadRenderTexture(GetScreenWidth() / 2.0,
                                      GetScreenHeight() - MENU_HEIGHT);
    klotski_target = LoadRenderTexture(GetScreenWidth() / 2.0,
                                       GetScreenHeight() - MENU_HEIGHT);
    menu_target = LoadRenderTexture(GetScreenWidth(), MENU_HEIGHT);
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
  auto UpdateWinningStates(const MassSpringSystem &masssprings,
                           const WinCondition win_condition) -> void;

  auto AddWinningState(const State &state, const WinCondition win_condition)
      -> void;

  auto UpdateCamera(const MassSpringSystem &masssprings, const State &current)
      -> void;

  auto UpdateTextureSizes() -> void;

  auto DrawMassSprings(const MassSpringSystem &masssprings,
                       const State &current) -> void;

  auto DrawKlotski(const State &state, int hov_x, int hov_y, int sel_x,
                   int sel_y, int block_add_x, int block_add_y) -> void;

  auto DrawMenu(const MassSpringSystem &masssprings, int current_preset,
                const State &current_state) -> void;

  auto DrawTextures() -> void;
};

#endif
