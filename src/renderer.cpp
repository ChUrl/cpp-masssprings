#include "renderer.hpp"

#include "config.hpp"
#include "mass_springs.hpp"

auto Renderer::Rotate(const Vector3 &a, const float cos_angle,
                      const float sin_angle) -> Vector3 {
  return Vector3(a.x * cos_angle - a.z * sin_angle, a.y,
                 a.x * sin_angle + a.z * cos_angle);
};

auto Renderer::Translate(const Vector3 &a, const float distance) -> Vector3 {
  return Vector3(a.x, a.y, a.z + distance);
};

auto Renderer::Project(const Vector3 &a) -> Vector2 {
  return Vector2(a.x / a.z, a.y / a.z);
}

auto Renderer::Map(const Vector2 &a) -> Vector2 {
  return Vector2((1.0 + a.x) / 2.0 * width, (1.0 - a.y) * height / 2.0);
}

auto Renderer::Transform(Edge2Set &edges, Vertex2Set &vertices,
                         const MassSpringSystem &mass_springs,
                         const float angle, const float distance) -> void {
  const float cos_angle = cos(angle);
  const float sin_angle = sin(angle);

  edges.clear();
  for (const auto &spring : mass_springs.springs) {
    Vector2 a = Map(Project(Translate(
        Rotate(spring.massA.position, cos_angle, sin_angle), distance)));
    Vector2 b = Map(Project(Translate(
        Rotate(spring.massB.position, cos_angle, sin_angle), distance)));

    edges.emplace_back(a, b);
  }

  // This is duplicated work, but easy to read
  vertices.clear();
  for (const auto &mass : mass_springs.masses) {
    Vector3 a =
        Translate(Rotate(mass.position, cos_angle, sin_angle), distance);
    Vector2 b = Map(Project(a));

    vertices.emplace_back(b.x, b.y, a.z);
  }
}

auto Renderer::DrawMassSprings(const Edge2Set &edges,
                               const Vertex2Set &vertices) -> void {
  BeginTextureMode(render_target);
  ClearBackground(RAYWHITE);
  for (const auto &[a, b] : edges) {
    DrawLine(a.x, a.y, b.x, b.y, EDGE_COLOR);
  }
  for (const auto &a : vertices) {
    // Increase the perspective perception by squaring the z-coordinate
    const float size = VERTEX_SIZE / (a.z * a.z);

    DrawRectangle(a.x - size / 2.0, a.y - size / 2.0, size, size, VERTEX_COLOR);
  }
  DrawLine(0, 0, 0, height, BLACK);
  EndTextureMode();

  BeginDrawing();
  DrawTextureRec(render_target.texture,
                 Rectangle(0, 0, (float)width, -(float)height), Vector2(0, 0),
                 WHITE);
  DrawFPS(10, 10);
  EndDrawing();
}
