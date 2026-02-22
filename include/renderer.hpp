#ifndef __RENDERER_HPP_
#define __RENDERER_HPP_

#include <raylib.h>
#include <raymath.h>
#include <unordered_set>

#include "camera.hpp"
#include "config.hpp"
#include "physics.hpp"
#include "puzzle.hpp"

class Renderer {
private:
  const OrbitCamera3D &camera;
  RenderTexture render_target;
  RenderTexture klotski_target;
  RenderTexture menu_target;

  // Instancing
  Material vertex_mat;
  int transforms_size;
  Matrix *transforms;
  Mesh cube_instance;
  Shader instancing_shader;

public:
  bool mark_solutions;
  bool connect_solutions;

public:
  Renderer(const OrbitCamera3D &camera)
      : camera(camera), mark_solutions(false), connect_solutions(false),
        transforms_size(0), transforms(nullptr) {
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

    // Instancing
    if (transforms != nullptr) {
      UnloadMaterial(vertex_mat);
      MemFree(transforms);
      UnloadMesh(cube_instance);

      // I think the shader already gets unloaded with the material?
      // UnloadShader(instancing_shader);
    }
  }

private:
  auto AllocateGraphInstancing(const MassSpringSystem &mass_springs) -> void;

  auto
  ReallocateGraphInstancingIfNecessary(const MassSpringSystem &mass_springs)
      -> void;

public:
  auto UpdateTextureSizes() -> void;

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
