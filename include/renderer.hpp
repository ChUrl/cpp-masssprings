#ifndef __RENDERER_HPP_
#define __RENDERER_HPP_

#include "camera.hpp"
#include "config.hpp"
#include "input.hpp"
#include "state.hpp"

#include <raylib.h>
#include <raymath.h>

class Renderer {
private:
  const StateManager &state;
  const InputHandler &input;

  const OrbitCamera3D &camera;
  RenderTexture render_target;
  RenderTexture klotski_target;
  RenderTexture menu_target;

  // Instancing
  Material vertex_mat;
  std::size_t transforms_size;
  Matrix *transforms;
  Mesh cube_instance;
  Shader instancing_shader;

public:
  Renderer(const OrbitCamera3D &_camera, const StateManager &_state,
           const InputHandler &_input)
      : state(_state), input(_input), camera(_camera), transforms_size(0),
        transforms(nullptr) {
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
  auto AllocateGraphInstancing(std::size_t size) -> void;

  auto ReallocateGraphInstancingIfNecessary(std::size_t size) -> void;

public:
  auto UpdateTextureSizes() -> void;

  auto DrawMassSprings(const std::vector<Vector3> &masses) -> void;

  auto DrawKlotski() -> void;

  auto DrawMenu(const std::vector<Vector3> &masses) -> void;

  auto DrawTextures(float ups) -> void;
};

#endif
