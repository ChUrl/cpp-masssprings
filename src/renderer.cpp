#include "renderer.hpp"
#include "config.hpp"
#include "puzzle.hpp"

#include <algorithm>
#include <format>
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
                   2 * VERTEX_SIZE, TARGET_BLOCK_COLOR);
        }

        std::size_t current_index = state.CurrentMassIndex();
        if (input.connect_solutions && masses.size() > current_index) {
          const Vector3 &current_mass = masses.at(current_index);
          DrawLine3D(winning_mass, current_mass, ORANGE);
        }
      }
    }
  }

  // Mark visited states
  for (const State &_state : state.visited_states) {
    std::size_t visited_index = state.states.at(_state);

    if (masses.size() > visited_index) {
      const Vector3 &visited_mass = masses.at(visited_index);
      DrawCube(visited_mass, VERTEX_SIZE * 1.5, VERTEX_SIZE * 1.5,
               VERTEX_SIZE * 1.5, PURPLE);
    }
  }

  // Mark winning path
  if (input.mark_path) {
    for (const std::size_t &_state : state.winning_path) {
      if (masses.size() > _state) {
        const Vector3 &path_mass = masses.at(_state);
        DrawCube(path_mass, VERTEX_SIZE * 1.75, VERTEX_SIZE * 1.75,
                 VERTEX_SIZE * 1.75, YELLOW);
      }
    }
  }

  // Mark starting state
  std::size_t starting_index = state.states.at(state.starting_state);
  if (masses.size() > starting_index) {
    const Vector3 &starting_mass = masses.at(starting_index);
    DrawCube(starting_mass, VERTEX_SIZE * 2, VERTEX_SIZE * 2, VERTEX_SIZE * 2,
             ORANGE);
  }

  // Mark current state
  std::size_t current_index = state.states.at(state.current_state);
  if (masses.size() > current_index) {
    const Vector3 &current_mass = masses.at(current_index);
    DrawCube(current_mass, VERTEX_SIZE * 2, VERTEX_SIZE * 2, VERTEX_SIZE * 2,
             BLUE);
  }

  // DrawCubeWires(current_mass.position, REPULSION_RANGE, REPULSION_RANGE,
  //               REPULSION_RANGE, BLACK);
  // DrawGrid(100, 1.0);
  // DrawSphere(camera.target, VERTEX_SIZE, ORANGE);
  EndMode3D();

  DrawLine(0, 0, 0, GetScreenHeight() - MENU_HEIGHT, BLACK);
  EndTextureMode();
}

auto Renderer::DrawKlotski() -> void {
#ifdef TRACY
  ZoneScoped;
#endif

  BeginTextureMode(klotski_target);
  ClearBackground(RAYWHITE);

  // Draw Board
  const int board_width = GetScreenWidth() / 2 - 2 * BOARD_PADDING;
  const int board_height = GetScreenHeight() - MENU_HEIGHT - 2 * BOARD_PADDING;
  int block_size = std::min(board_width / state.current_state.width,
                            board_height / state.current_state.height) -
                   2 * BLOCK_PADDING;
  int x_offset = (board_width - (block_size + 2 * BLOCK_PADDING) *
                                    state.current_state.width) /
                 2.0;
  int y_offset = (board_height - (block_size + 2 * BLOCK_PADDING) *
                                     state.current_state.height) /
                 2.0;

  DrawRectangle(0, 0, GetScreenWidth() / 2, GetScreenHeight() - MENU_HEIGHT,
                RAYWHITE);
  DrawRectangle(x_offset, y_offset,
                board_width - 2 * x_offset + 2 * BOARD_PADDING,
                board_height - 2 * y_offset + 2 * BOARD_PADDING,
                state.current_state.IsWon()
                    ? GREEN
                    : (state.current_state.restricted ? DARKGRAY : LIGHTGRAY));
  for (int x = 0; x < state.current_state.width; ++x) {
    for (int y = 0; y < state.current_state.height; ++y) {
      DrawRectangle(x_offset + BOARD_PADDING + x * BLOCK_PADDING * 2 +
                        BLOCK_PADDING + x * block_size,
                    y_offset + BOARD_PADDING + y * BLOCK_PADDING * 2 +
                        BLOCK_PADDING + y * block_size,
                    block_size, block_size, WHITE);
    }
  }

  // Draw Blocks
  for (Block block : state.current_state) {
    Color c = BLOCK_COLOR;
    if (block.Covers(input.sel_x, input.sel_y)) {
      c = HL_BLOCK_COLOR;
    }
    if (block.target) {
      if (block.Covers(input.sel_x, input.sel_y)) {
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

    if (block.Covers(input.hov_x, input.hov_y)) {
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
  if (input.block_add_x >= 0 && input.block_add_y >= 0) {
    DrawCircle(
        x_offset + BOARD_PADDING + input.block_add_x * BLOCK_PADDING * 2 +
            BLOCK_PADDING + input.block_add_x * block_size + block_size / 2,
        y_offset + BOARD_PADDING + input.block_add_y * BLOCK_PADDING * 2 +
            BLOCK_PADDING + input.block_add_y * block_size + block_size / 2,
        block_size / 10.0, Fade(BLACK, 0.5));
  }

  // Draw board goal position
  const Block target = state.current_state.GetTargetBlock();
  if (target.IsValid() && state.current_state.HasWinCondition()) {
    int target_x = state.current_state.target_x;
    int target_y = state.current_state.target_y;
    DrawRectangleLinesEx(
        Rectangle(x_offset + BOARD_PADDING + target_x * BLOCK_PADDING * 2 +
                      BLOCK_PADDING + target_x * block_size,
                  y_offset + BOARD_PADDING + target_y * BLOCK_PADDING * 2 +
                      BLOCK_PADDING + target_y * block_size,
                  target.width * block_size + target.width * 2 * BLOCK_PADDING -
                      2 * BLOCK_PADDING,
                  target.height * block_size +
                      target.height * 2 * BLOCK_PADDING - 2 * BLOCK_PADDING),
        2.0, TARGET_BLOCK_COLOR);
  }

  DrawLine(GetScreenWidth() / 2 - 1, 0, GetScreenWidth() / 2 - 1,
           GetScreenHeight() - MENU_HEIGHT, BLACK);
  EndTextureMode();
}

auto Renderer::DrawMenu(const std::vector<Vector3> &masses) -> void {
#ifdef TRACY
  ZoneScoped;
#endif

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
             btn_height - 2.0 * BUTTON_PAD, WHITE);
  };

  draw_btn(0, 0,
           std::format("States: {}, Transitions: {}, Winning: {}",
                       masses.size(), state.springs.size(),
                       state.winning_states.size()),
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
           std::format("Preset (M/N): {}, {} (F)", state.current_preset,
                       state.current_state.restricted ? "Restricted" : "Free"),
           DARKPURPLE);
  draw_btn(2, 1, std::format("Populate Graph (G), Clear Graph (C)"),
           DARKPURPLE);
  draw_btn(2, 2,
           std::format("Path (U): {} / Mark (I): {} / Connect (O): {}",
                       input.mark_path, input.mark_solutions,
                       input.connect_solutions),
           DARKPURPLE);
  draw_btn(2, 3, std::format("Move along Path (Space)"), DARKPURPLE);

  DrawLine(0, MENU_HEIGHT - 1, GetScreenWidth(), MENU_HEIGHT - 1, BLACK);
  EndTextureMode();
}

auto Renderer::DrawTextures(float ups) -> void {
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
  DrawText(TextFormat("%.0f UPS", ups), GetScreenWidth() / 2 + 120,
           MENU_HEIGHT + 10, 20, ORANGE);
  EndDrawing();
}
