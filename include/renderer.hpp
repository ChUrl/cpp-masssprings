#ifndef __RENDERER_HPP_
#define __RENDERER_HPP_

#include <immintrin.h>
#include <raylib.h>
#include <raymath.h>
#include <vector>

#include "klotski.hpp"
#include "mass_springs.hpp"

using Edge3Set = std::vector<std::pair<Vector3, Vector3>>;
using Edge2Set = std::vector<std::pair<Vector2, Vector2>>;
using Vertex2Set =
    std::vector<Vector3>; // Vertex2Set uses Vector3 to retain the z-coordinate
                          // for circle size adaptation

class Renderer {
private:
  int width;
  int height;
  RenderTexture2D render_target;
  RenderTexture2D klotski_target;

public:
  Renderer(int width, int height) : width(width), height(height) {
    render_target = LoadRenderTexture(width, height);
    klotski_target = LoadRenderTexture(width, height);
  }

  Renderer(const Renderer &copy) = delete;
  Renderer &operator=(const Renderer &copy) = delete;
  Renderer(Renderer &&move) = delete;
  Renderer &operator=(Renderer &&move) = delete;

  ~Renderer() {
    UnloadRenderTexture(render_target);
    UnloadRenderTexture(klotski_target);
  }

private:
  auto Rotate(const Vector3 &a, const float cos_angle, const float sin_angle)
      -> Vector3;

  auto Translate(const Vector3 &a, const float distance, const float horizontal,
                 const float vertical) -> Vector3;

  auto Project(const Vector3 &a) -> Vector2;

  auto Map(const Vector2 &a) -> Vector2;

public:
  auto Transform(Edge2Set &edges, Vertex2Set &vertices,
                 const MassSpringSystem &mass_springs, const float angle,
                 const float distance, const float horizontal,
                 const float vertical) -> void;

  auto DrawMassSprings(const Edge2Set &edges, const Vertex2Set &vertices)
      -> void;

  auto DrawKlotski(State &state, int hov_x, int hov_y, int sel_x, int sel_y)
      -> void;

  auto DrawTextures() -> void;
};

#endif
