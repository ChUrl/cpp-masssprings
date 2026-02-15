#define TINYOBJLOADER_IMPLEMENTATION

#define TIME // Print transformation times
// #define MATRIX_TRANSFORM // Use combined matrix transform
// #define PARALLEL_TRANSFORM
#define AVX_TRANSFORM
#define PRUNE      // Prune duplicate edges
#define PRUNE_HASH // Do hash-based pruning

#ifdef PRUNE
#include <algorithm>
#ifdef PRUNE_HASH
#include <unordered_set>
#endif // #ifdef PRUNE_HASH
#endif // #ifdef PRUNE

#ifdef TIME
#include <chrono>
#endif

#ifndef MATRIX_TRANSFORM
#include <cmath>
#ifdef PARALLEL_TRANSFORM
#include <execution>
#elifdef AVX_TRANSFORM
#include <immintrin.h>
#endif // #ifdef PARALLEL_TRANSFORM
#endif // #ifdef MATRIX_TRANSFORM

#include <iostream>
#include <raylib.h>
#include <raymath.h>
#include <tiny_obj_loader.h>
#include <vector>

constexpr int WIDTH = 800;
constexpr int HEIGHT = 800;
constexpr float VERTEX_SIZE = 1.5;
constexpr Color VERTEX_COLOR = {27, 188, 104, 255};
constexpr Color EDGE_COLOR = {20, 133, 38, 255};
constexpr float SPEED = 1.0;
constexpr float CAMERA_DISTANCE = 2.2;

using Edge2Set = std::vector<std::pair<Vector2, Vector2>>;
using Edge3Set = std::vector<std::pair<Vector3, Vector3>>;

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

#ifdef PRUNE
#ifdef PRUNE_HASH
struct Vector3Hash {
  size_t operator()(const Vector3 &v) const {
    return std::hash<float>()(v.x) ^ (std::hash<float>()(v.y) << 1) ^
           (std::hash<float>()(v.z) << 2);
  }
};

struct EdgeHash {
  size_t operator()(const std::pair<Vector3, Vector3> &e) const {
    Vector3Hash h;
    return h(e.first) ^ (h(e.second) << 1);
  }
};

auto prune_edges(Edge3Set &result, const Edge3Set &edges) -> void {
  std::unordered_set<std::pair<Vector3, Vector3>, EdgeHash> seen;

  for (const auto &edge : edges) {
    auto normalized =
        (edge.first.x < edge.second.x ||
         (edge.first.x == edge.second.x && edge.first.y < edge.second.y) ||
         (edge.first.x == edge.second.x && edge.first.y == edge.second.y &&
          edge.first.z < edge.second.z))
            ? edge
            : std::make_pair(edge.second, edge.first);

    if (seen.insert(normalized).second) {
      result.emplace_back(edge);
    }
  }

  std::cout << "Found " << edges.size() - result.size() << " duplicate edges."
            << std::endl;
}
#else
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
#endif // #ifdef PRUNE_HASH
#endif // #ifdef PRUNE

#ifdef MATRIX_TRANSFORM
auto matrix_transform(Edge2Set &result, const Edge3Set &edges,
                      const Camera &camera, const Matrix &model_transformation)
    -> void {
  for (const auto &[a, b] : edges) {
    const Vector3 modelA = Vector3Transform(a, model_transformation);
    const Vector3 modelB = Vector3Transform(b, model_transformation);
    const Vector2 screenA = GetWorldToScreen(modelA, camera);
    const Vector2 screenB = GetWorldToScreen(modelB, camera);
    result.emplace_back(screenA, screenB);
  }
}
#else
auto manual_transform(Edge2Set &result, const Edge3Set &edges,
                      const float angle, const float distance) -> void {
  const float cos_angle = cos(angle);
  const float sin_angle = sin(angle);

  auto rotate = [&](const Vector3 &a) -> Vector3 {
    return Vector3(a.x * cos_angle - a.z * sin_angle, a.y,
                   a.x * sin_angle + a.z * cos_angle);
  };

  auto translate = [&](const Vector3 &a) -> Vector3 {
    return Vector3(a.x, a.y, a.z + distance);
  };

  auto project = [&](const Vector3 &a) -> Vector2 {
    return Vector2(a.x / a.z, a.y / a.z);
  };

  auto map = [&](const Vector2 &a) -> Vector2 {
    return Vector2((a.x + 1.0) / 2.0 * WIDTH,
                   (1.0 - (a.y + 1.0)) / 2.0 * HEIGHT + HEIGHT / 2.0);
  };

#ifdef PARALLEL_TRANSFORM
  result.resize(edges.size());
  std::transform(
      std::execution::par_unseq, edges.begin(), edges.end(), result.begin(),
      [&](const auto &edge) -> std::pair<Vector2, Vector2> {
        const Vector2 at = map(project(translate(rotate(edge.first))));
        const Vector2 bt = map(project(translate(rotate(edge.second))));

        return std::make_pair(at, bt);
      });
#elifdef AVX_TRANSFORM
  result.resize(edges.size());

  // Broadcast constants to all 16 lanes
  const __m512 cos_a = _mm512_set1_ps(cos(angle));
  const __m512 sin_a = _mm512_set1_ps(sin(angle));
  const __m512 dist = _mm512_set1_ps(distance);
  const __m512 half_width = _mm512_set1_ps(WIDTH * 0.5f);
  const __m512 half_height = _mm512_set1_ps(HEIGHT * 0.5f);
  const __m512 one = _mm512_set1_ps(1.0f);

  size_t i = 0;

  // Process 8 edges at a time (16 points total)
  for (; i + 7 < edges.size(); i += 8) {
    // Load 8 edge start points (interleaved)
    __m512 ax, ay, az, bx, by, bz;

    // Gather x coordinates for 8 start points
    ax = _mm512_set_ps(
        edges[i + 7].first.x, edges[i + 6].first.x, edges[i + 5].first.x,
        edges[i + 4].first.x, edges[i + 3].first.x, edges[i + 2].first.x,
        edges[i + 1].first.x, edges[i].first.x, edges[i + 7].first.x,
        edges[i + 6].first.x, edges[i + 5].first.x, edges[i + 4].first.x,
        edges[i + 3].first.x, edges[i + 2].first.x, edges[i + 1].first.x,
        edges[i].first.x);

    ay = _mm512_set_ps(
        edges[i + 7].first.y, edges[i + 6].first.y, edges[i + 5].first.y,
        edges[i + 4].first.y, edges[i + 3].first.y, edges[i + 2].first.y,
        edges[i + 1].first.y, edges[i].first.y, edges[i + 7].first.y,
        edges[i + 6].first.y, edges[i + 5].first.y, edges[i + 4].first.y,
        edges[i + 3].first.y, edges[i + 2].first.y, edges[i + 1].first.y,
        edges[i].first.y);

    az = _mm512_set_ps(
        edges[i + 7].first.z, edges[i + 6].first.z, edges[i + 5].first.z,
        edges[i + 4].first.z, edges[i + 3].first.z, edges[i + 2].first.z,
        edges[i + 1].first.z, edges[i].first.z, edges[i + 7].first.z,
        edges[i + 6].first.z, edges[i + 5].first.z, edges[i + 4].first.z,
        edges[i + 3].first.z, edges[i + 2].first.z, edges[i + 1].first.z,
        edges[i].first.z);

    // Gather x,y,z for 8 end points
    bx = _mm512_set_ps(
        edges[i + 7].second.x, edges[i + 6].second.x, edges[i + 5].second.x,
        edges[i + 4].second.x, edges[i + 3].second.x, edges[i + 2].second.x,
        edges[i + 1].second.x, edges[i].second.x, edges[i + 7].second.x,
        edges[i + 6].second.x, edges[i + 5].second.x, edges[i + 4].second.x,
        edges[i + 3].second.x, edges[i + 2].second.x, edges[i + 1].second.x,
        edges[i].second.x);

    by = _mm512_set_ps(
        edges[i + 7].second.y, edges[i + 6].second.y, edges[i + 5].second.y,
        edges[i + 4].second.y, edges[i + 3].second.y, edges[i + 2].second.y,
        edges[i + 1].second.y, edges[i].second.y, edges[i + 7].second.y,
        edges[i + 6].second.y, edges[i + 5].second.y, edges[i + 4].second.y,
        edges[i + 3].second.y, edges[i + 2].second.y, edges[i + 1].second.y,
        edges[i].second.y);

    bz = _mm512_set_ps(
        edges[i + 7].second.z, edges[i + 6].second.z, edges[i + 5].second.z,
        edges[i + 4].second.z, edges[i + 3].second.z, edges[i + 2].second.z,
        edges[i + 1].second.z, edges[i].second.z, edges[i + 7].second.z,
        edges[i + 6].second.z, edges[i + 5].second.z, edges[i + 4].second.z,
        edges[i + 3].second.z, edges[i + 2].second.z, edges[i + 1].second.z,
        edges[i].second.z);

    // Rotate: x' = x*cos - z*sin, z' = x*sin + z*cos
    __m512 ax_rot = _mm512_fmsub_ps(ax, cos_a, _mm512_mul_ps(az, sin_a));
    __m512 az_rot = _mm512_fmadd_ps(ax, sin_a, _mm512_mul_ps(az, cos_a));
    __m512 bx_rot = _mm512_fmsub_ps(bx, cos_a, _mm512_mul_ps(bz, sin_a));
    __m512 bz_rot = _mm512_fmadd_ps(bx, sin_a, _mm512_mul_ps(bz, cos_a));

    // Translate z
    az_rot = _mm512_add_ps(az_rot, dist);
    bz_rot = _mm512_add_ps(bz_rot, dist);

    // Project: x/z, y/z
    __m512 ax_proj = _mm512_div_ps(ax_rot, az_rot);
    __m512 ay_proj = _mm512_div_ps(ay, az_rot);
    __m512 bx_proj = _mm512_div_ps(bx_rot, bz_rot);
    __m512 by_proj = _mm512_div_ps(by, bz_rot);

    // Map to screen: (proj + 1) * width/2
    __m512 ax_screen = _mm512_mul_ps(_mm512_add_ps(ax_proj, one), half_width);
    __m512 ay_screen =
        _mm512_fmadd_ps(_mm512_sub_ps(one, _mm512_add_ps(ay_proj, one)),
                        half_height, half_height);
    __m512 bx_screen = _mm512_mul_ps(_mm512_add_ps(bx_proj, one), half_width);
    __m512 by_screen =
        _mm512_fmadd_ps(_mm512_sub_ps(one, _mm512_add_ps(by_proj, one)),
                        half_height, half_height);

    // Store results
    alignas(64) float ax_out[16], ay_out[16], bx_out[16], by_out[16];
    _mm512_store_ps(ax_out, ax_screen);
    _mm512_store_ps(ay_out, ay_screen);
    _mm512_store_ps(bx_out, bx_screen);
    _mm512_store_ps(by_out, by_screen);

    // Extract to result vector
    for (size_t j = 0; j < 8; ++j) {
      result[i + j] = {{ax_out[j], ay_out[j]}, {bx_out[j], by_out[j]}};
    }
  }

  // Handle remaining edges with scalar code
  for (; i < edges.size(); ++i) {
    const auto &[a, b] = edges[i];

    auto rotate = [angle](const Vector3 &v) -> Vector3 {
      return Vector3(v.x * cos(angle) - v.z * sin(angle), v.y,
                     v.x * sin(angle) + v.z * cos(angle));
    };

    auto translate = [distance](const Vector3 &v) -> Vector3 {
      return Vector3(v.x, v.y, v.z + distance);
    };

    auto project = [](const Vector3 &v) -> Vector2 {
      return Vector2(v.x / v.z, v.y / v.z);
    };

    auto map = [](const Vector2 &v) -> Vector2 {
      return Vector2((v.x + 1) * WIDTH / 2,
                     (1 - (v.y + 1)) * HEIGHT / 2.0 + HEIGHT / 2.0);
    };

    auto at = map(project(translate(rotate(a))));
    auto bt = map(project(translate(rotate(b))));

    result[i] = {at, bt};
  }
#else
  for (const auto &[a, b] : edges) {
    const Vector2 at = map(project(translate(rotate(a))));
    const Vector2 bt = map(project(translate(rotate(b))));
    result.emplace_back(at, bt);
  }
#endif
}
#endif

auto draw_edges(const Edge2Set &edges) -> void {
  for (const auto &[a, b] : edges) {
    DrawLine(a.x, a.y, b.x, b.y, EDGE_COLOR);
    // DrawCircle(a.x, a.y, VERTEX_SIZE, VERTEX_COLOR);
    // std::cout << "Drawing (" << a.x << ", " << a.y << ") -> (" << b.x << ", "
    //           << b.y << ")" << std::endl;
  }
}

auto main(int argc, char *argv[]) -> int {
  if (argc < 2) {
    std::cout << "Missing .obj file." << std::endl;
    return 1;
  }

  SetTraceLogLevel(LOG_ERROR);

  // SetTargetFPS(60);
  // SetConfigFlags(FLAG_VSYNC_HINT);
  SetConfigFlags(FLAG_MSAA_4X_HINT);

  InitWindow(WIDTH, HEIGHT, "ObjRender");

  Edge3Set edges;
  parse_obj_file(edges, argv[1]);

#ifdef PRUNE
  Edge3Set pruned;

#ifdef TIME
  std::chrono::high_resolution_clock::time_point start_prune =
      std::chrono::high_resolution_clock::now();
#endif // #ifdef TIME
  prune_edges(pruned, edges);
#ifdef TIME
  std::chrono::high_resolution_clock::time_point end_prune =
      std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> prune_time =
      end_prune - start_prune;
  std::cout << "Edge pruning took " << prune_time << "." << std::endl;
#endif // #ifdef TIME

#endif // #ifdef PRUNE

#ifdef MATRIX_TRANSFORM
  Camera3D camera = Camera3D(Vector3(0.0, 0.0, -1.0 * CAMERA_DISTANCE),
                             Vector3(0.0, 0.0, 1.0), Vector3(0.0, 1.0, 0.0),
                             90.0, CAMERA_PERSPECTIVE);

  Matrix translation = MatrixTranslate(0.0, 0.0, CAMERA_DISTANCE);
#endif

  Edge2Set viewport;
#ifdef PRUNE
  viewport.reserve(pruned.size());
#else
  viewport.reserve(edges.size());
#endif

#ifdef TIME
  double last_print_time = GetTime();
  std::chrono::duration<double, std::milli> time_accumulator =
      std::chrono::duration<double, std::milli>(0);
  int time_measure_count = 0;
#endif

  RenderTexture2D render_target;
  render_target = LoadRenderTexture(WIDTH, HEIGHT);

  float abstime = 0.0;
  while (!WindowShouldClose()) {

#ifdef TIME
    double time = GetTime();
    std::chrono::high_resolution_clock::time_point start_transform =
        std::chrono::high_resolution_clock::now();
#endif

#ifdef MATRIX_TRANSFORM
    Matrix rotation = MatrixRotateY(abstime);

    viewport.clear();
#ifdef PRUNE
    matrix_transform(viewport, pruned, camera, rotation);
#else
    matrix_transform(viewport, edges, camera, rotation);
#endif

#else
    viewport.clear();
#ifdef PRUNE
    manual_transform(viewport, pruned, abstime, CAMERA_DISTANCE);
#else
    manual_transform(viewport, edges, abstime, CAMERA_DISTANCE);
#endif // #ifdef PRUNE

#endif // #ifdef MATRIX_TRANSFORM

#ifdef TIME
    std::chrono::high_resolution_clock::time_point end_transform =
        std::chrono::high_resolution_clock::now();
    time_accumulator += end_transform - start_transform;
    time_measure_count++;
    if (time - last_print_time > 5.0) {
      std::cout << "Transformation time avg: "
                << time_accumulator / time_measure_count << "." << std::endl;
      last_print_time = time;
      time_accumulator = std::chrono::duration<double, std::milli>(0);
      time_measure_count = 0;
    }
#endif

    BeginTextureMode(render_target);
    ClearBackground(RAYWHITE);
    draw_edges(viewport);
    EndTextureMode();

    BeginDrawing();
    DrawTextureRec(render_target.texture,
                   Rectangle(0, 0, (float)WIDTH, -(float)HEIGHT), Vector2(0, 0),
                   WHITE);
    DrawFPS(10, 10);
    EndDrawing();

    abstime += GetFrameTime() * SPEED;
  }

  UnloadRenderTexture(render_target);

  return 0;
}
