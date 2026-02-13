#define TINYOBJLOADER_IMPLEMENTATION

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <raylib-cpp.hpp>
#include <raylib.h>
#include <tiny_obj_loader.h>
#include <vector>

constexpr int WIDTH = 800;
constexpr int HEIGHT = 800;
constexpr float SPEED = 1.0;
constexpr float CAMERA_DISTANCE = 2.2;

using Edge2Set = std::vector<std::pair<Vector2, Vector2>>;
using Edge3Set = std::vector<std::pair<Vector3, Vector3>>;

// constexpr Color EDGE_COLOR = {27, 188, 104, 255};
constexpr Color EDGE_COLOR = {20, 133, 38, 255};

auto parse_obj_file(Edge3Set &result, const std::string_view path) -> void {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warn;
  std::string err;

  std::cout << "Parsing \"" << path << "\"..." << std::endl;

  bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                              path.data(), NULL, false);

  if (!err.empty()) {
    std::cerr << err << std::endl;
  }

  if (!ret) {
    exit(1);
  }

  int faces = 0;

  // Loop over shapes
  for (size_t s = 0; s < shapes.size(); ++s) {
    // Loop over faces (polygon)
    size_t index_offset = 0;
    for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); ++f) {
      int fv = shapes[s].mesh.num_face_vertices[f];

      // std::cout << "Found face:" << std::endl;
      faces++;

      // Loop over vertices in the face
      for (size_t v = 0; v < fv; v++) {
        // Access to vertex
        tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
        tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
        tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
        tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];

        // Access to previous vertex
        tinyobj::index_t p_idx =
            shapes[s].mesh.indices[index_offset + (v + 1) % fv];
        tinyobj::real_t p_vx = attrib.vertices[3 * p_idx.vertex_index + 0];
        tinyobj::real_t p_vy = attrib.vertices[3 * p_idx.vertex_index + 1];
        tinyobj::real_t p_vz = attrib.vertices[3 * p_idx.vertex_index + 2];

        // std::cout << "Found edge: (" << vx << ", " << vy << ", " << vz
        //           << ") -> (" << p_vx << ", " << p_vy << ", " << p_vz << ")"
        //           << std::endl;

        result.emplace_back(Vector3(vx, vy, vz), Vector3(p_vx, p_vy, p_vz));
      }
      index_offset += fv;
    }
  }
  std::cout << "Found " << faces << " faces and " << result.size() << " edges."
            << std::endl;
}

auto prune_edges(Edge3Set &result, const Edge3Set &edges) -> void {
  auto eq = [](const float a, const float b) -> bool {
    return fabs(a - b) <= 0.001;
  };

  auto v3eq = [&](const Vector3 a, const Vector3 b) -> bool {
    return eq(a.x, b.x) && eq(a.y, b.y) && eq(a.z, b.z);
  };

  for (const auto &edge : edges) {
    if (std::find_if(result.begin(), result.end(), [&](const auto &e) -> bool {
          return (v3eq(edge.first, e.first) && v3eq(edge.second, e.second)) ||
                 (v3eq(edge.first, e.second) && v3eq(edge.second, e.first));
        }) != result.end()) {

      // We found the edge already in the result vector
      continue;
    }

    // We didn't find the edge in the result vector
    result.emplace_back(edge);
  }

  std::cout << "Found " << edges.size() - result.size() << " duplicate edges."
            << std::endl;
}

auto to_viewport(Edge2Set &result, const Edge2Set &edges) -> void {
  for (const auto &[a, b] : edges) {
    result.emplace_back(
        Vector2((a.x + 1.0) / 2.0 * WIDTH,
                (1.0 - (a.y + 1.0)) / 2.0 * HEIGHT + HEIGHT / 2.0),
        Vector2((b.x + 1.0) / 2.0 * WIDTH,
                (1.0 - (b.y + 1.0)) / 2.0 * HEIGHT + HEIGHT / 2.0));
  }
}

auto to_imageplane(Edge2Set &result, const Edge3Set &edges) -> void {
  for (const auto &[a, b] : edges) {
    result.emplace_back(Vector2(a.x / a.z, a.y / a.z),
                        Vector2(b.x / b.z, b.y / b.z));
  }
}

auto translate_forward(Edge3Set &result, const Edge3Set &edges,
                       const float distance) -> void {
  for (const auto &[a, b] : edges) {
    result.emplace_back(Vector3(a.x, a.y, a.z + distance),
                        Vector3(b.x, b.y, b.z + distance));
  }
}

auto rotate_upwards(Edge3Set &result, const Edge3Set &edges,
                    const float abstime) -> void {
  for (const auto &[a, b] : edges) {
    result.emplace_back(Vector3(a.x * cos(abstime) - a.z * sin(abstime), a.y,
                                a.x * sin(abstime) + a.z * cos(abstime)),
                        Vector3(b.x * cos(abstime) - b.z * sin(abstime), b.y,
                                b.x * sin(abstime) + b.z * cos(abstime)));
  }
}

auto draw(const Edge2Set &edges) -> void {
  for (const auto &[a, b] : edges) {
    DrawLine(a.x, a.y, b.x, b.y, EDGE_COLOR);
  }
}

auto main(int argc, char *argv[]) -> int {
  if (argc < 2) {
    std::cout << "Missing .obj file." << std::endl;
    return 1;
  }

  // SetTargetFPS(60);
  SetConfigFlags(FLAG_VSYNC_HINT);
  // SetConfigFlags(FLAG_MSAA_4X_HINT);

  raylib::Window window(WIDTH, HEIGHT, "ObjRender");

  Edge3Set edges;
  parse_obj_file(edges, argv[1]);

  Edge3Set pruned;
  prune_edges(pruned, edges);

  // TODO: Replace this with combined matrix transform
  Edge3Set rotated;
  Edge3Set translated;
  Edge2Set projected;
  Edge2Set viewport;
  rotated.reserve(pruned.size());
  translated.reserve(pruned.size());
  projected.reserve(pruned.size());
  viewport.reserve(pruned.size());

  double last_print = window.GetTime();
  int measure_count = 0;

  float abstime = 0.0;
  while (!window.ShouldClose()) {
    double time = window.GetTime();

    std::chrono::high_resolution_clock::time_point start =
        std::chrono::high_resolution_clock::now();

    rotated.clear();
    translated.clear();
    projected.clear();
    viewport.clear();
    rotate_upwards(rotated, pruned, abstime);
    translate_forward(translated, rotated, CAMERA_DISTANCE);
    to_imageplane(projected, translated);
    to_viewport(viewport, projected);

    // TODO: Calculate second average
    if (time - last_print > 1.0) {
      std::chrono::high_resolution_clock::time_point end =
          std::chrono::high_resolution_clock::now();
      std::chrono::duration<double, std::milli> ms_double = end - start;
      std::cout << "Transformation took " << ms_double << "." << std::endl;
      last_print = time;
    }

    window.ClearBackground(RAYWHITE);

    window.BeginDrawing();
    draw(viewport);
    window.EndDrawing();

    abstime += window.GetFrameTime() * SPEED;
  }

  return 0;
}
