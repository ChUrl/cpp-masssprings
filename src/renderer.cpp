#include "renderer.hpp"

#include <raylib.h>
#include <raymath.h>

#include "config.hpp"
#include "mass_springs.hpp"

auto OrbitCamera3D::Update() -> void {
  Vector2 mouse = GetMousePosition();

  if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
    dragging = true;
    last_mouse = mouse;
  } else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    panning = true;
    last_mouse = mouse;
  }

  if (IsMouseButtonReleased(MOUSE_RIGHT_BUTTON)) {
    dragging = false;
  }
  if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
    panning = false;
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

    float speed = distance * PAN_SPEED / 1000.0;
    Vector3 forward =
        Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    Vector3 up = Vector3Normalize(Vector3CrossProduct(right, forward));

    Vector3 offset = Vector3Add(Vector3Scale(right, -dx.x * speed),
                                Vector3Scale(up, dx.y * speed));

    target = Vector3Add(target, offset);
  }

  float wheel = GetMouseWheelMove();
  distance -= wheel * ZOOM_SPEED;
  distance = Clamp(distance, MIN_CAMERA_DISTANCE, MAX_CAMERA_DISTANCE);

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
  for (const auto &[states, spring] : masssprings.springs) {
    const Mass a = spring.massA;
    const Mass b = spring.massB;

    DrawLine3D(a.position, b.position, EDGE_COLOR);
  }

  // Draw masses (high performance impact)
  if (masssprings.masses.size() <= 5000) {
    for (const auto &[state, mass] : masssprings.masses) {
      DrawCube(mass.position, VERTEX_SIZE, VERTEX_SIZE, VERTEX_SIZE,
               VERTEX_COLOR);
    }
  }

  // DrawGrid(10, 1.0);

  EndMode3D();

  DrawLine(0, 0, 0, height, BLACK);

  EndTextureMode();
}

auto Renderer::DrawKlotski(const State &state, int hov_x, int hov_y, int sel_x,
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
