#include "renderer.hpp"
#include "config.hpp"
#include "physics.hpp"
#include "puzzle.hpp"
#include "tracy.hpp"

#include <algorithm>
#include <format>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <tracy/Tracy.hpp>
#include <unordered_set>

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

auto Renderer::AllocateGraphInstancing(const MassSpringSystem &mass_springs)
    -> void {
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

  transforms = (Matrix *)MemAlloc(mass_springs.masses.size() * sizeof(Matrix));
  transforms_size = mass_springs.masses.size();
}

auto Renderer::ReallocateGraphInstancingIfNecessary(
    const MassSpringSystem &mass_springs) -> void {
  if (transforms_size != mass_springs.masses.size()) {
    transforms = (Matrix *)MemRealloc(transforms, mass_springs.masses.size() *
                                                      sizeof(Matrix));
    transforms_size = mass_springs.masses.size();
  }
}

auto Renderer::DrawMassSprings(const MassSpringSystem &mass_springs,
                               const State &current_state,
                               const State &starting_state,
                               const std::unordered_set<State> &winning_states,
                               const std::unordered_set<State> &visited_states)
    -> void {
  ZoneScoped;

  // Prepare cube instancing
  if (mass_springs.masses.size() < DRAW_VERTICES_LIMIT) {
    if (transforms == nullptr) {
      AllocateGraphInstancing(mass_springs);
    }
    ReallocateGraphInstancingIfNecessary(mass_springs);

    int i = 0;
    for (const auto &[state, mass] : mass_springs.masses) {
      transforms[i] =
          MatrixTranslate(mass.position.x, mass.position.y, mass.position.z);
      ++i;
    }
  }

  BeginTextureMode(render_target);
  ClearBackground(RAYWHITE);

  BeginMode3D(camera.camera);

  // Draw springs (batched)
  rlBegin(RL_LINES);
  for (const auto &[states, spring] : mass_springs.springs) {
    rlColor4ub(EDGE_COLOR.r, EDGE_COLOR.g, EDGE_COLOR.b, EDGE_COLOR.a);
    rlVertex3f(spring.massA.position.x, spring.massA.position.y,
               spring.massA.position.z);
    rlVertex3f(spring.massB.position.x, spring.massB.position.y,
               spring.massB.position.z);
  }
  rlEnd();

  // Draw masses (instanced)
  if (mass_springs.masses.size() < DRAW_VERTICES_LIMIT) {
    // NOTE: I don't know if drawing all this inside a shader would make it much
    //       faster...
    //       The amount of data sent to the GPU would be reduced (just positions
    //       instead of matrices), but is this noticable for < 100000 cubes?
    DrawMeshInstanced(cube_instance, vertex_mat, transforms,
                      mass_springs.masses.size());
  }

  // Mark winning states
  if (mark_solutions || connect_solutions) {
    for (const auto &state : winning_states) {
      const Mass &winning_mass = mass_springs.GetMass(state);
      if (mark_solutions) {
        DrawCube(winning_mass.position, 2 * VERTEX_SIZE, 2 * VERTEX_SIZE,
                 2 * VERTEX_SIZE, BLUE);
      }

      if (connect_solutions) {
        DrawLine3D(winning_mass.position,
                   mass_springs.GetMass(current_state).position, PURPLE);
      }
    }
  }

  // Mark visited states
  for (const auto &state : visited_states) {
    const Mass &visited_mass = mass_springs.GetMass(state);

    DrawCube(visited_mass.position, VERTEX_SIZE * 1.5, VERTEX_SIZE * 1.5,
             VERTEX_SIZE * 1.5, PURPLE);
  }

  // Mark starting state
  const Mass &starting_mass = mass_springs.GetMass(starting_state);
  DrawCube(starting_mass.position, VERTEX_SIZE * 2, VERTEX_SIZE * 2,
           VERTEX_SIZE * 2, ORANGE);

  // Mark current state
  const Mass &current_mass = mass_springs.GetMass(current_state);
  DrawCube(current_mass.position, VERTEX_SIZE * 2, VERTEX_SIZE * 2,
           VERTEX_SIZE * 2, RED);

  // DrawCubeWires(current_mass.position, REPULSION_RANGE, REPULSION_RANGE,
  //               REPULSION_RANGE, BLACK);
  // DrawGrid(100, 1.0);
  // DrawSphere(camera.target, VERTEX_SIZE, ORANGE);
  EndMode3D();

  DrawLine(0, 0, 0, GetScreenHeight() - MENU_HEIGHT, BLACK);
  EndTextureMode();
}

auto Renderer::DrawKlotski(const State &state, int hov_x, int hov_y, int sel_x,
                           int sel_y, int block_add_x, int block_add_y,
                           const WinCondition win_condition) -> void {
  ZoneScoped;

  BeginTextureMode(klotski_target);
  ClearBackground(RAYWHITE);

  // Draw Board
  const int board_width = GetScreenWidth() / 2 - 2 * BOARD_PADDING;
  const int board_height = GetScreenHeight() - MENU_HEIGHT - 2 * BOARD_PADDING;
  int block_size =
      std::min(board_width / state.width, board_height / state.height) -
      2 * BLOCK_PADDING;
  int x_offset =
      (board_width - (block_size + 2 * BLOCK_PADDING) * state.width) / 2.0;
  int y_offset =
      (board_height - (block_size + 2 * BLOCK_PADDING) * state.height) / 2.0;

  DrawRectangle(0, 0, GetScreenWidth() / 2, GetScreenHeight() - MENU_HEIGHT,
                RAYWHITE);
  DrawRectangle(
      x_offset, y_offset, board_width - 2 * x_offset + 2 * BOARD_PADDING,
      board_height - 2 * y_offset + 2 * BOARD_PADDING,
      win_condition(state) ? GREEN : (state.restricted ? DARKGRAY : LIGHTGRAY));
  for (int x = 0; x < state.width; ++x) {
    for (int y = 0; y < state.height; ++y) {
      DrawRectangle(x_offset + BOARD_PADDING + x * BLOCK_PADDING * 2 +
                        BLOCK_PADDING + x * block_size,
                    y_offset + BOARD_PADDING + y * BLOCK_PADDING * 2 +
                        BLOCK_PADDING + y * block_size,
                    block_size, block_size, WHITE);
    }
  }

  // Draw Blocks
  for (Block block : state) {
    Color c = BLOCK_COLOR;
    if (block.Covers(sel_x, sel_y)) {
      c = HL_BLOCK_COLOR;
    }
    if (block.target) {
      if (block.Covers(sel_x, sel_y)) {
        c = HL_TARGET_BLOCK_COLOR;
      } else {
        c = TARGET_BLOCK_COLOR;
      }
    }
    DrawRectangle(x_offset + BOARD_PADDING + block.x * BLOCK_PADDING * 2 +
                      BLOCK_PADDING + block.x * block_size,
                  y_offset + BOARD_PADDING + block.y * BLOCK_PADDING * 2 +
                      BLOCK_PADDING + block.y * block_size,
                  block.width * block_size + block.width * 2 * BLOCK_PADDING -
                      2 * BLOCK_PADDING,
                  block.height * block_size + block.height * 2 * BLOCK_PADDING -
                      2 * BLOCK_PADDING,
                  c);

    if (block.Covers(hov_x, hov_y)) {
      DrawRectangleLinesEx(
          Rectangle(x_offset + BOARD_PADDING + block.x * BLOCK_PADDING * 2 +
                        BLOCK_PADDING + block.x * block_size,
                    y_offset + BOARD_PADDING + block.y * BLOCK_PADDING * 2 +
                        BLOCK_PADDING + block.y * block_size,
                    block.width * block_size + block.width * 2 * BLOCK_PADDING -
                        2 * BLOCK_PADDING,
                    block.height * block_size +
                        block.height * 2 * BLOCK_PADDING - 2 * BLOCK_PADDING),
          2.0, BLACK);
    }
  }

  // Draw editing starting position
  if (block_add_x >= 0 && block_add_y >= 0) {
    DrawCircle(x_offset + BOARD_PADDING + block_add_x * BLOCK_PADDING * 2 +
                   BLOCK_PADDING + block_add_x * block_size + block_size / 2,
               y_offset + BOARD_PADDING + block_add_y * BLOCK_PADDING * 2 +
                   BLOCK_PADDING + block_add_y * block_size + block_size / 2,
               block_size / 10.0, Fade(BLACK, 0.5));
  }

  DrawLine(GetScreenWidth() / 2 - 1, 0, GetScreenWidth() / 2 - 1,
           GetScreenHeight() - MENU_HEIGHT, BLACK);
  EndTextureMode();
}

auto Renderer::DrawMenu(const MassSpringSystem &mass_springs,
                        int current_preset, const State &current_state,
                        const std::unordered_set<State> &winning_states)
    -> void {
  ZoneScoped;

  BeginTextureMode(menu_target);
  ClearBackground(RAYWHITE);

  float btn_width =
      static_cast<float>(GetScreenWidth() - (MENU_COLS * MENU_PAD + MENU_PAD)) /
      MENU_COLS;
  float btn_height =
      static_cast<float>(MENU_HEIGHT - (MENU_ROWS * MENU_PAD + MENU_PAD)) /
      MENU_ROWS;

  auto draw_btn = [&](int x, int y, std::string text, Color color) {
    int posx = MENU_PAD + x * (MENU_PAD + btn_width);
    int posy = MENU_PAD + y * (MENU_PAD + btn_height);
    DrawRectangle(posx, posy, btn_width, btn_height, Fade(color, 0.6));
    DrawRectangleLines(posx, posy, btn_width, btn_height, color);
    DrawText(text.data(), posx + BUTTON_PAD, posy + BUTTON_PAD,
             btn_height - 2 * BUTTON_PAD, WHITE);
  };

  draw_btn(0, 0,
           std::format("States: {}, Transitions: {}, Winning: {}",
                       mass_springs.masses.size(), mass_springs.springs.size(),
                       winning_states.size()),
           DARKGREEN);
  draw_btn(
      0, 1,
      std::format("Camera Distance (SHIFT for Fast Zoom): {}", camera.distance),
      DARKGREEN);
  draw_btn(
      0, 2,
      std::format("Lock Camera to Current State (L): {}", camera.target_lock),
      DARKGREEN);

  draw_btn(1, 0, std::format("Reset State (R)"), DARKBLUE);
  draw_btn(1, 1, std::format("Add/Remove Col/Row (Arrow Keys)"), DARKBLUE);
  draw_btn(1, 2, std::format("Print Board State to Console (P)"), DARKBLUE);

  draw_btn(2, 0,
           std::format("Preset (M/N): {}, {} (F)", current_preset,
                       current_state.restricted ? "Restricted" : "Free"),
           DARKPURPLE);
  draw_btn(2, 1, std::format("Populate Graph (G), Clear Graph (C)"),
           DARKPURPLE);
  draw_btn(2, 2,
           std::format("Mark (I): {} / Connect (O): {}", mark_solutions,
                       connect_solutions),
           DARKPURPLE);

  DrawLine(0, MENU_HEIGHT - 1, GetScreenWidth(), MENU_HEIGHT - 1, BLACK);
  EndTextureMode();
}

auto Renderer::DrawTextures() -> void {
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
  DrawFPS(GetScreenWidth() / 2 + 10, MENU_HEIGHT + 10);
  EndDrawing();
  FrameMark;
}
