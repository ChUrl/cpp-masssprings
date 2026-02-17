#include "renderer.hpp"

#include <raylib.h>

#include "config.hpp"
#include "mass_springs.hpp"

auto OrbitCamera3D::Update() -> void {
  Vector2 mouse = GetMousePosition();

  if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    dragging = true;
    last_mouse = mouse;
  }

  if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
    dragging = false;
  }

  if (dragging) {
    Vector2 dx = Vector2Subtract(mouse, last_mouse);
    last_mouse = mouse;

    angle_x -= dx.x * 0.005;
    angle_y += dx.y * 0.005;

    angle_y = Clamp(angle_y, -1.5, 1.5); // Prevent flipping
  }

  float wheel = GetMouseWheelMove();
  distance -= wheel * 2.0;
  distance = Clamp(distance, 2.0, 50.0);

  float x = cos(angle_y) * sin(angle_x) * distance;
  float y = sin(angle_y) * distance;
  float z = cos(angle_y) * cos(angle_x) * distance;

  camera.position = Vector3Add(target, Vector3(x, y, z));
  camera.target = target;
}

auto Renderer::UpdateCamera() -> void { camera.Update(); }

auto Renderer::DrawMassSprings(const MassSpringSystem &masssprings) -> void {
  BeginTextureMode(render_target);
  ClearBackground(RAYWHITE);

  BeginMode3D(camera.camera);

  // Draw springs
  for (const auto &spring : masssprings.springs) {
    const Mass a = spring.massA;
    const Mass b = spring.massB;

    DrawLine3D(a.position, b.position, EDGE_COLOR);
  }

  // Draw masses
  for (const auto &[state, mass] : masssprings.masses) {
    DrawCube(mass.position, VERTEX_SIZE, VERTEX_SIZE, VERTEX_SIZE,
             VERTEX_COLOR);
  }

  DrawGrid(10, 1.0);

  EndMode3D();

  DrawLine(0, 0, 0, height, BLACK);

  EndTextureMode();
}

auto Renderer::DrawKlotski(State &state, int hov_x, int hov_y, int sel_x,
                           int sel_y) -> void {
  BeginTextureMode(klotski_target);
  ClearBackground(RAYWHITE);

  // Draw Board
  const int board_width = width - 2 * BOARD_PADDING;
  const int board_height = height - 2 * BOARD_PADDING;
  float block_size;
  float x_offset = 0.0;
  float y_offset = 0.0;
  if (state.width > state.height) {
    block_size =
        static_cast<float>(board_width) / state.width - 2 * BLOCK_PADDING;
    y_offset = (board_height - block_size * state.height -
                BLOCK_PADDING * 2 * state.height) /
               2.0;
  } else {
    block_size =
        static_cast<float>(board_height) / state.height - 2 * BLOCK_PADDING;
    x_offset = (board_width - block_size * state.width -
                BLOCK_PADDING * 2 * state.width) /
               2.0;
  }

  DrawRectangle(0, 0, width, height, RAYWHITE);
  DrawRectangle(x_offset, y_offset,
                board_width - 2 * x_offset + 2 * BOARD_PADDING,
                board_height - 2 * y_offset + 2 * BOARD_PADDING, LIGHTGRAY);
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

  DrawLine(width - 1, 0, width - 1, height, BLACK);
  EndTextureMode();
}

auto Renderer::DrawTextures() -> void {
  BeginDrawing();
  DrawTextureRec(klotski_target.texture,
                 Rectangle(0, 0, (float)width, -(float)height), Vector2(0, 0),
                 WHITE);
  DrawTextureRec(render_target.texture,
                 Rectangle(0, 0, (float)width, -(float)height),
                 Vector2(width, 0), WHITE);
  DrawFPS(width + 10, 10);
  EndDrawing();
}
