#include "renderer.hpp"

#include <raylib.h>

#include "config.hpp"

auto Renderer::Rotate(const Vector3 &a, const float cos_angle,
                      const float sin_angle) -> Vector3 {
  return Vector3(a.x * cos_angle - a.z * sin_angle, a.y,
                 a.x * sin_angle + a.z * cos_angle);
};

auto Renderer::Translate(const Vector3 &a, const float distance,
                         const float horizontal, const float vertical)
    -> Vector3 {
  return Vector3(a.x + horizontal, a.y + vertical, a.z + distance);
};

auto Renderer::Project(const Vector3 &a) -> Vector2 {
  return Vector2(a.x / a.z, a.y / a.z);
}

auto Renderer::Map(const Vector2 &a) -> Vector2 {
  return Vector2((1.0 + a.x) / 2.0 * width, (1.0 - a.y) * height / 2.0);
}

auto Renderer::Transform(Edge2Set &edges, Vertex2Set &vertices,
                         const MassSpringSystem &mass_springs,
                         const float angle, const float distance,
                         const float horizontal, const float vertical) -> void {
  const float cos_angle = cos(angle);
  const float sin_angle = sin(angle);

  edges.clear();
  for (const auto &spring : mass_springs.springs) {
    const Mass &massA = spring.massA;
    const Mass &massB = spring.massB;

    // Stuff behind the camera
    if (massA.position.z + distance <= 0.1) {
      continue;
    }
    if (massB.position.z + distance <= 0.1) {
      continue;
    }

    Vector2 a =
        Map(Project(Translate(Rotate(massA.position, cos_angle, sin_angle),
                              distance, horizontal, vertical)));
    Vector2 b =
        Map(Project(Translate(Rotate(massB.position, cos_angle, sin_angle),
                              distance, horizontal, vertical)));

    // Stuff outside the viewport
    if (!CheckCollisionPointRec(
            a, Rectangle(-1.0 * width * CULLING_TOLERANCE,
                         -1.0 * height * CULLING_TOLERANCE,
                         width + width * CULLING_TOLERANCE * 2.0,
                         height + height * CULLING_TOLERANCE * 2.0))) {
      continue;
    }
    if (!CheckCollisionPointRec(
            b, Rectangle(-1.0 * width * CULLING_TOLERANCE,
                         -1.0 * height * CULLING_TOLERANCE,
                         width + width * CULLING_TOLERANCE * 2.0,
                         height + height * CULLING_TOLERANCE * 2.0))) {
      continue;
    }

    edges.emplace_back(a, b);
  }

  // This is duplicated work, but easy to read
  vertices.clear();
  for (const auto &[state, mass] : mass_springs.masses) {

    // Stuff behind the camera
    if (mass.position.z + distance <= 0.1) {
      continue;
    }

    Vector3 a = Translate(Rotate(mass.position, cos_angle, sin_angle), distance,
                          horizontal, vertical);
    Vector2 b = Map(Project(a));

    // Stuff outside the viewport
    if (!CheckCollisionPointRec(
            b, Rectangle(-1.0 * width * CULLING_TOLERANCE,
                         -1.0 * height * CULLING_TOLERANCE,
                         width + width * CULLING_TOLERANCE * 2.0,
                         height + height * CULLING_TOLERANCE * 2.0))) {
      continue;
    }

    vertices.emplace_back(b.x, b.y, a.z);
  }
}

auto Renderer::DrawMassSprings(const Edge2Set &edges,
                               const Vertex2Set &vertices) -> void {
  BeginTextureMode(render_target);
  ClearBackground(RAYWHITE);

  // Draw springs
  for (const auto &[a, b] : edges) {
    DrawLine(a.x, a.y, b.x, b.y, EDGE_COLOR);
  }

  // Draw masses
  for (const auto &a : vertices) {
    // Increase the perspective perception by squaring the z-coordinate
    const float size = Clamp(VERTEX_SIZE / (a.z * a.z), 0.1, 100.0);

    DrawRectangle(a.x - size / 2.0, a.y - size / 2.0, size, size, VERTEX_COLOR);
  }

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
