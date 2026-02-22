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

  Material vertex_mat;

  // Batching
  float *cube;
  Mesh graph;

  // Instancing
  int transforms_size;
  Matrix *transforms;
  Mesh cube_instance;
  Shader instancing_shader;

public:
  bool mark_solutions;
  bool connect_solutions;

public:
  Renderer()
      : camera(OrbitCamera3D(Vector3(0, 0, 0), CAMERA_DISTANCE)),
        mark_solutions(false), connect_solutions(false), cube(nullptr),
        transforms_size(0), transforms(nullptr) {
    render_target = LoadRenderTexture(GetScreenWidth() / 2.0,
                                      GetScreenHeight() - MENU_HEIGHT);
    klotski_target = LoadRenderTexture(GetScreenWidth() / 2.0,
                                       GetScreenHeight() - MENU_HEIGHT);
    menu_target = LoadRenderTexture(GetScreenWidth(), MENU_HEIGHT);

    vertex_mat = LoadMaterialDefault();
    vertex_mat.maps[MATERIAL_MAP_DIFFUSE].color = VERTEX_COLOR;
  }

  Renderer(const Renderer &copy) = delete;
  Renderer &operator=(const Renderer &copy) = delete;
  Renderer(Renderer &&move) = delete;
  Renderer &operator=(Renderer &&move) = delete;

  ~Renderer() {
    UnloadRenderTexture(render_target);
    UnloadRenderTexture(klotski_target);
    UnloadRenderTexture(menu_target);

    UnloadMaterial(vertex_mat);

    // Batching
    if (cube != nullptr) {
      MemFree(cube);
      UnloadMesh(graph);
    }

    // Instancing
    if (transforms != nullptr) {
      MemFree(transforms);
      UnloadMesh(cube_instance);

      // I think the shader already gets unloaded with the material?
      // UnloadShader(instancing_shader);
    }
  }

public:
  auto UpdateCamera(const MassSpringSystem &mass_springs,
                    const State &current_state) -> void;

  auto UpdateTextureSizes() -> void;

#ifdef BATCHING
  auto AllocateGraphBatching() -> void;

  auto ReallocateGraphBatchingIfNecessary(const MassSpringSystem &mass_springs)
      -> void;
#endif

#ifdef INSTANCING
  auto AllocateGraphInstancing(const MassSpringSystem &mass_springs) -> void;

  auto
  ReallocateGraphInstancingIfNecessary(const MassSpringSystem &mass_springs)
      -> void;
#endif

  auto DrawMassSprings(const MassSpringSystem &mass_springs,
                       const State &current_state,
                       const std::unordered_set<State> &winning_states) -> void;

  auto DrawKlotski(const State &state, int hov_x, int hov_y, int sel_x,
                   int sel_y, int block_add_x, int block_add_y,
                   const WinCondition win_condition) -> void;

  auto DrawMenu(const MassSpringSystem &mass_springs, int current_preset,
                const State &current_state,
                const std::unordered_set<State> &winning_states) -> void;

  auto DrawTextures() -> void;
};

#endif
