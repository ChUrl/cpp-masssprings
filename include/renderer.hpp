#ifndef __RENDERER_HPP_
#define __RENDERER_HPP_

#include <immintrin.h>
#include <raylib.h>
#include <raymath.h>
#include <vector>

#include "mass_springs.hpp"

using Edge2Set = std::vector<std::pair<Vector2, Vector2>>;
using Edge3Set = std::vector<std::pair<Vector3, Vector3>>;

class Renderer {
private:
  int width;
  int height;
  RenderTexture2D render_target;

public:
  Renderer(int width, int height) : width(width), height(height) {
    render_target = LoadRenderTexture(width, height);
  }

  Renderer(const Renderer &copy) = delete;
  Renderer &operator=(const Renderer &copy) = delete;
  Renderer(Renderer &&move) = delete;
  Renderer &operator=(Renderer &&move) = delete;

  ~Renderer() { UnloadRenderTexture(render_target); }

private:
  auto Rotate(const Vector3 &a, const float cos_angle, const float sin_angle)
      -> Vector3;

  auto Translate(const Vector3 &a, const float distance) -> Vector3;

  auto Project(const Vector3 &a) -> Vector2;

  auto Map(const Vector2 &a) -> Vector2;

public:
  auto Transform(Edge2Set &edges, const MassSpringSystem &mass_springs,
                 const float angle, const float distance) -> void;

  auto Draw(const Edge2Set &edges) -> void;
};

#endif
