#include "renderer.hpp"
#include "config.hpp"
#include "puzzle.hpp"

#include <algorithm>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#ifdef TRACY
#include "tracy.hpp"
#include <tracy/Tracy.hpp>
#endif

#ifdef BATCHING
#include <cstring>
#endif

auto Renderer::UpdateTextureSizes() -> void {
  if (!IsWindowResized()) {
    return;
  }

  UnloadRenderTexture(render_target);
  UnloadRenderTexture(klotski_target);
  UnloadRenderTexture(menu_target);

  int width = GetScreenWidth() / 2.0;
  int height = GetScreenHeight() - MENU_HEIGHT;

  render_target = LoadRenderTexture(width, height);
  klotski_target = LoadRenderTexture(width, height);
  menu_target = LoadRenderTexture(width * 2, MENU_HEIGHT);
}

auto Renderer::AllocateGraphInstancing(std::size_t size) -> void {
  cube_instance = GenMeshCube(VERTEX_SIZE, VERTEX_SIZE, VERTEX_SIZE);

  instancing_shader = LoadShader("shader/instancing_vertex.glsl",
                                 "shader/instancing_fragment.glsl");
  instancing_shader.locs[SHADER_LOC_MATRIX_MVP] =
      GetShaderLocation(instancing_shader, "mvp");
  instancing_shader.locs[SHADER_LOC_VECTOR_VIEW] =
      GetShaderLocation(instancing_shader, "viewPos");

  vertex_mat = LoadMaterialDefault();
  vertex_mat.maps[MATERIAL_MAP_DIFFUSE].color = VERTEX_COLOR;
  vertex_mat.shader = instancing_shader;

  transforms = (Matrix *)MemAlloc(size * sizeof(Matrix));
  transforms_size = size;
}

auto Renderer::ReallocateGraphInstancingIfNecessary(std::size_t size) -> void {
  if (transforms_size != size) {
    transforms = (Matrix *)MemRealloc(transforms, size * sizeof(Matrix));
    transforms_size = size;
  }
}

auto Renderer::DrawMassSprings(const std::vector<Vector3> &masses) -> void {
#ifdef TRACY
  ZoneScoped;
#endif

  // Prepare cube instancing
  {
#ifdef TRACY
    ZoneNamedN(prepare_masses, "PrepareMasses", true);
#endif
    if (masses.size() < DRAW_VERTICES_LIMIT) {
      if (transforms == nullptr) {
        AllocateGraphInstancing(masses.size());
      }
      ReallocateGraphInstancingIfNecessary(masses.size());

      int i = 0;
      for (const Vector3 &mass : masses) {
        transforms[i] = MatrixTranslate(mass.x, mass.y, mass.z);
        ++i;
      }
    }
  }

  BeginTextureMode(render_target);
  ClearBackground(RAYWHITE);

  BeginMode3D(camera.camera);

  // Draw springs (batched)
  {
#ifdef TRACY
    ZoneNamedN(draw_springs, "DrawSprings", true);
#endif
    rlBegin(RL_LINES);
    for (const auto &[from, to] : state.springs) {
      if (masses.size() > from && masses.size() > to) {
        const Vector3 &a = masses.at(from);
        const Vector3 &b = masses.at(to);
        rlColor4ub(EDGE_COLOR.r, EDGE_COLOR.g, EDGE_COLOR.b, EDGE_COLOR.a);
        rlVertex3f(a.x, a.y, a.z);
        rlVertex3f(b.x, b.y, b.z);
      }
    }
    rlEnd();
  }

  // Draw masses (instanced)
  {
#ifdef TRACY
    ZoneNamedN(draw_masses, "DrawMasses", true);
#endif
    if (masses.size() < DRAW_VERTICES_LIMIT) {
      // NOTE: I don't know if drawing all this inside a shader would make it
      //       much faster... The amount of data sent to the GPU would be
      //       reduced (just positions instead of matrices), but is this
      //       noticable for < 100000 cubes?
      DrawMeshInstanced(cube_instance, vertex_mat, transforms, masses.size());
    }
  }

  // Mark winning states
  if (input.mark_solutions || input.connect_solutions) {
    for (const State &_state : state.winning_states) {

      std::size_t winning_index = state.states.at(_state);
      if (masses.size() > winning_index) {

        const Vector3 &winning_mass = masses.at(winning_index);
        if (input.mark_solutions) {
          DrawCube(winning_mass, 2 * VERTEX_SIZE, 2 * VERTEX_SIZE,
                   2 * VERTEX_SIZE, VERTEX_TARGET_COLOR);
        }

        std::size_t current_index = state.CurrentMassIndex();
        if (input.connect_solutions && masses.size() > current_index) {
          const Vector3 &current_mass = masses.at(current_index);
          DrawLine3D(winning_mass, current_mass, Fade(TARGET_BLOCK_COLOR, 0.5));
        }
      }
    }
  }

  // Mark visited states
  for (const auto &[_state, visits] : state.visited_states) {
    std::size_t visited_index = state.states.at(_state);

    if (masses.size() > visited_index) {
      const Vector3 &visited_mass = masses.at(visited_index);
      DrawCube(visited_mass, VERTEX_SIZE * 1.5, VERTEX_SIZE * 1.5,
               VERTEX_SIZE * 1.5, VERTEX_VISITED_COLOR);
    }
  }

  // Mark winning path
  if (input.mark_path) {
    for (const std::size_t &_state : state.winning_path) {
      if (masses.size() > _state) {
        const Vector3 &path_mass = masses.at(_state);
        DrawCube(path_mass, VERTEX_SIZE * 1.75, VERTEX_SIZE * 1.75,
                 VERTEX_SIZE * 1.75, VERTEX_PATH_COLOR);
      }
    }
  }

  // Mark starting state
  std::size_t starting_index = state.states.at(state.starting_state);
  if (masses.size() > starting_index) {
    const Vector3 &starting_mass = masses.at(starting_index);
    DrawCube(starting_mass, VERTEX_SIZE * 2, VERTEX_SIZE * 2, VERTEX_SIZE * 2,
             VERTEX_START_COLOR);
  }

  // Mark current state
  std::size_t current_index = state.states.at(state.current_state);
  if (masses.size() > current_index) {
    const Vector3 &current_mass = masses.at(current_index);
    DrawCube(current_mass, VERTEX_SIZE * 2, VERTEX_SIZE * 2, VERTEX_SIZE * 2,
             VERTEX_CURRENT_COLOR);
  }

  EndMode3D();
  EndTextureMode();
}

auto Renderer::DrawKlotski() -> void {
#ifdef TRACY
  ZoneScoped;
#endif

  BeginTextureMode(klotski_target);
  ClearBackground(RAYWHITE);

  gui.DrawPuzzleBoard();

  EndTextureMode();
}

auto Renderer::DrawMenu(const std::vector<Vector3> &masses) -> void {
#ifdef TRACY
  ZoneScoped;
#endif

  BeginTextureMode(menu_target);
  ClearBackground(RAYWHITE);

  gui.DrawMainMenu();

  EndTextureMode();
}

auto Renderer::DrawTextures(int fps, int ups) -> void {
  BeginDrawing();

  DrawTextureRec(menu_target.texture,
                 Rectangle(0, 0, menu_target.texture.width,
                           -1 * menu_target.texture.height),
                 Vector2(0, 0), WHITE);
  DrawTextureRec(klotski_target.texture,
                 Rectangle(0, 0, klotski_target.texture.width,
                           -1 * klotski_target.texture.height),
                 Vector2(0, MENU_HEIGHT), WHITE);
  DrawTextureRec(render_target.texture,
                 Rectangle(0, 0, render_target.texture.width,
                           -1 * render_target.texture.height),
                 Vector2(GetScreenWidth() / 2.0, MENU_HEIGHT), WHITE);

  // Draw borders
  DrawRectangleLinesEx(Rectangle(0, 0, GetScreenWidth(), MENU_HEIGHT), 1.0,
                       BLACK);
  DrawRectangleLinesEx(Rectangle(0, MENU_HEIGHT, GetScreenWidth() / 2.0,
                                 GetScreenHeight() - MENU_HEIGHT),
                       1.0, BLACK);
  DrawRectangleLinesEx(Rectangle(GetScreenWidth() / 2.0, MENU_HEIGHT,
                                 GetScreenWidth() / 2.0,
                                 GetScreenHeight() - MENU_HEIGHT),
                       1.0, BLACK);

  gui.DrawGraphOverlay(fps, ups);
  gui.DrawSavePresetPopup();
  gui.Update();

  EndDrawing();
}
