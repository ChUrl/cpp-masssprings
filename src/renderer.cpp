#include "renderer.hpp"

#include <algorithm>
#include <cstring>
#include <format>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <unordered_set>

#include "config.hpp"
#include "klotski.hpp"
#include "mass_springs.hpp"

auto OrbitCamera3D::Update(const Mass &current_mass) -> void {
  Vector2 mouse = GetMousePosition();
  if (mouse.x >= GetScreenWidth() / 2.0 && mouse.y >= MENU_HEIGHT) {
    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
      dragging = true;
      last_mouse = mouse;
    } else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      panning = true;
      target_lock = false;
      last_mouse = mouse;
    }
  }

  if (IsMouseButtonReleased(MOUSE_RIGHT_BUTTON)) {
    dragging = false;
  }
  if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
    panning = false;
  }

  if (IsKeyPressed(KEY_L)) {
    target_lock = !target_lock;
  }

  if (dragging) {
    Vector2 dx = Vector2Subtract(mouse, last_mouse);
    last_mouse = mouse;

    angle_x -= dx.x * ROT_SPEED / 200.0;
    angle_y += dx.y * ROT_SPEED / 200.0;

    angle_y = Clamp(angle_y, -1.5, 1.5); // Prevent flipping
  }

  if (panning) {
    Vector2 dx = Vector2Subtract(mouse, last_mouse);
    last_mouse = mouse;

    // float speed = PAN_SPEED;
    float speed;
    if (IsKeyDown(KEY_LEFT_SHIFT)) {
      speed = distance * PAN_SPEED / 1000.0 * PAN_MULTIPLIER;
    } else {
      speed = distance * PAN_SPEED / 1000.0;
    }
    Vector3 forward =
        Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    Vector3 up = Vector3Normalize(Vector3CrossProduct(right, forward));

    Vector3 offset = Vector3Add(Vector3Scale(right, -dx.x * speed),
                                Vector3Scale(up, dx.y * speed));

    target = Vector3Add(target, offset);
  }

  if (target_lock) {
    target = current_mass.position;
  }

  if (mouse.x >= GetScreenWidth() / 2.0 && mouse.y >= MENU_HEIGHT) {
    float wheel = GetMouseWheelMove();
    if (IsKeyDown(KEY_LEFT_SHIFT)) {
      distance -= wheel * ZOOM_SPEED * ZOOM_MULTIPLIER;
    } else {
      distance -= wheel * ZOOM_SPEED;
    }
  }
  distance = Clamp(distance, MIN_CAMERA_DISTANCE, MAX_CAMERA_DISTANCE);

  // Spherical coordinates
  float x = cos(angle_y) * sin(angle_x) * distance;
  float y = sin(angle_y) * distance;
  float z = cos(angle_y) * cos(angle_x) * distance;

  camera.position = Vector3Add(target, Vector3(x, y, z));
  camera.target = target;
}

auto Renderer::UpdateCamera(const MassSpringSystem &masssprings,
                            const State &current) -> void {
  const Mass &c = masssprings.masses.at(current);
  camera.Update(c);
}

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

#ifdef BATCHING
auto Renderer::AllocateGraphBatching() -> void {
  int vertices = 36;
  int max_masses = 100000;

  graph = {0};
  graph.vertexCount = max_masses * vertices;
  graph.triangleCount = max_masses * 12;
  graph.vertices = (float *)MemAlloc(graph.vertexCount * 3 * sizeof(float));

  memset(graph.vertices, 0, graph.vertexCount * 3 * sizeof(float));

  UploadMesh(&graph, true);

  Mesh indexed_cube = GenMeshCube(VERTEX_SIZE, VERTEX_SIZE, VERTEX_SIZE);
  cube = (float *)MemAlloc(indexed_cube.triangleCount * 3 * 3 * sizeof(float));
  for (int i = 0; i < indexed_cube.triangleCount * 3; ++i) {
    int idx = indexed_cube.indices[i];
    cube[i * 3 + 0] = indexed_cube.vertices[idx * 3 + 0];
    cube[i * 3 + 1] = indexed_cube.vertices[idx * 3 + 1];
    cube[i * 3 + 2] = indexed_cube.vertices[idx * 3 + 2];
  }
  UnloadMesh(indexed_cube);

  std::cout << "Allocated graph mesh." << std::endl;
}

auto Renderer::ReallocateGraphBatchingIfNecessary(
    const MassSpringSystem &mass_springs) -> void {
  if (graph.vertexCount / 3 > mass_springs.masses.size()) {
    return;
  }

  int vertices = 36;
  int max_masses = mass_springs.masses.size() * 2;

  UnloadMesh(graph);

  graph = {0};
  graph.vertexCount = max_masses * vertices;
  graph.triangleCount = max_masses * 12;
  graph.vertices = (float *)MemAlloc(graph.vertexCount * 3 * sizeof(float));

  memset(graph.vertices, 0, graph.vertexCount * 3 * sizeof(float));

  UploadMesh(&graph, true);

  std::cout << "Reallocated graph mesh." << std::endl;
}
#endif

#ifdef INSTANCING
auto Renderer::AllocateGraphInstancing(const MassSpringSystem &mass_springs)
    -> void {
  cube_instance = GenMeshCube(VERTEX_SIZE, VERTEX_SIZE, VERTEX_SIZE);

  instancing_shader =
      LoadShader("instancing_vertex.glsl", "instancing_fragment.glsl");
  instancing_shader.locs[SHADER_LOC_MATRIX_MVP] =
      GetShaderLocation(instancing_shader, "mvp");
  instancing_shader.locs[SHADER_LOC_VECTOR_VIEW] =
      GetShaderLocation(instancing_shader, "viewPos");

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
#endif

auto Renderer::DrawMassSprings(const MassSpringSystem &mass_springs,
                               const State &current_state,
                               const std::unordered_set<State> &winning_states)
    -> void {
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
#ifdef INSTANCING
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
  DrawMeshInstanced(cube_instance, vertex_mat, transforms,
                    mass_springs.masses.size());
#endif

  // Draw masses (batched)
#ifdef BATCHING
  if (cube == nullptr) {
    AllocateGraphBatching();
  }
  ReallocateGraphBatchingIfNecessary(mass_springs);

  int vertices = 36;
  int current_size = mass_springs.masses.size();
  int i = 0;
  for (const auto &[state, mass] : mass_springs.masses) {
    for (int v = 0; v < vertices; ++v) {
      int dst = (i * vertices + v) * 3;
      int src = v * 3;
      graph.vertices[dst + 0] = cube[src + 0] + mass.position.x;
      graph.vertices[dst + 1] = cube[src + 1] + mass.position.y;
      graph.vertices[dst + 2] = cube[src + 2] + mass.position.z;
    }
    ++i;
  }
  UpdateMeshBuffer(graph, 0, graph.vertices,
                   current_size * vertices * 3 * sizeof(float), 0);

  // Temporarily reduce the vertex count to the used part (we overallocate)
  int full_size = graph.vertexCount;
  graph.vertexCount = current_size * vertices;
  graph.triangleCount = current_size * 12;

  DrawMesh(graph, vertex_mat, MatrixIdentity());

  // Restore the vertex count
  graph.vertexCount = full_size;
  graph.triangleCount = full_size / 3;
#endif

  // Mark current state
  const Mass &current_mass = mass_springs.GetMass(current_state);
  DrawCube(current_mass.position, VERTEX_SIZE * 2, VERTEX_SIZE * 2,
           VERTEX_SIZE * 2, RED);

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

  // DrawGrid(10, 1.0);
  // DrawSphere(camera.target, VERTEX_SIZE, ORANGE);
  EndMode3D();

  DrawLine(0, 0, 0, GetScreenHeight() - MENU_HEIGHT, BLACK);
  EndTextureMode();
}

auto Renderer::DrawKlotski(const State &state, int hov_x, int hov_y, int sel_x,
                           int sel_y, int block_add_x, int block_add_y,
                           const WinCondition win_condition) -> void {
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

auto Renderer::DrawMenu(const MassSpringSystem &masssprings, int current_preset,
                        const State &current_state,
                        const std::unordered_set<State> &winning_states)
    -> void {
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
                       masssprings.masses.size(), masssprings.springs.size(),
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
}
