#define VERLET_UPDATE

#include <iostream>
#include <raylib.h>
#include <raymath.h>

#include "config.hpp"
#include "klotski.hpp"
#include "mass_springs.hpp"
#include "renderer.hpp"

auto klotski_a() -> State {
  State s = State(4, 5);
  Block a = Block(0, 0, 1, 2, false);
  Block b = Block(1, 0, 2, 2, true);
  Block c = Block(3, 0, 1, 2, false);
  Block d = Block(0, 2, 1, 2, false);
  Block e = Block(1, 2, 2, 1, false);
  Block f = Block(3, 2, 1, 2, false);
  Block g = Block(1, 3, 1, 1, false);
  Block h = Block(2, 3, 1, 1, false);
  Block i = Block(0, 4, 1, 1, false);
  Block j = Block(3, 4, 1, 1, false);

  s.AddBlock(a);
  s.AddBlock(b);
  s.AddBlock(c);
  s.AddBlock(d);
  s.AddBlock(e);
  s.AddBlock(f);
  s.AddBlock(g);
  s.AddBlock(h);
  s.AddBlock(i);
  s.AddBlock(j);

  return s;
}

auto main(int argc, char *argv[]) -> int {
  // if (argc < 2) {
  //   std::cout << "Missing .klotski file." << std::endl;
  //   return 1;
  // }

  SetTraceLogLevel(LOG_ERROR);

  // SetTargetFPS(60);
  SetConfigFlags(FLAG_VSYNC_HINT);
  SetConfigFlags(FLAG_MSAA_4X_HINT);

  InitWindow(WIDTH * 2, HEIGHT, "MassSprings");

  // Mass springs configuration
  MassSpringSystem mass_springs;

  // Klotski configuration
  State board = klotski_a();
  mass_springs.AddMass(1.0, Vector3Zero(), true, board.state);

  // Rendering configuration
  Renderer renderer(WIDTH, HEIGHT);
  Edge2Set edges;
  edges.reserve(mass_springs.springs.size());
  Vertex2Set vertices;
  vertices.reserve(mass_springs.masses.size());

  // Game loop
  float camera_distance = CAMERA_DISTANCE;
  float horizontal = 0.0;
  float vertical = 0.0;
  float frametime;
  float abstime = 0.0;
  int hov_x = 0;
  int hov_y = 0;
  int sel_x = 0;
  int sel_y = 0;
  while (!WindowShouldClose()) {
    frametime = GetFrameTime();

    // Mouse handling
    Vector2 m = GetMousePosition();
    float block_size;
    float x_offset = 0.0;
    float y_offset = 0.0;
    if (board.width > board.height) {
      block_size = static_cast<float>(WIDTH) / board.width;
      y_offset = (HEIGHT - block_size * board.height) / 2.0;
    } else {
      block_size = static_cast<float>(HEIGHT) / board.height;
      x_offset = (WIDTH - block_size * board.width) / 2.0;
    }
    if (m.x < x_offset) {
      hov_x = 100;
    } else {
      hov_x = (m.x - x_offset) / block_size;
    }
    if (m.y < y_offset) {
      hov_y = 100;
    } else {
      hov_y = (m.y - y_offset) / block_size;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      sel_x = hov_x;
      sel_y = hov_y;
    }
    camera_distance += GetMouseWheelMove() / -10.0;

    // Key handling
    std::string previous_state = board.state;
    if (IsKeyPressed(KEY_W)) {
      if (board.MoveBlockAt(sel_x, sel_y, Direction::NOR)) {
        sel_y--;
      }
    } else if (IsKeyPressed(KEY_A)) {
      if (board.MoveBlockAt(sel_x, sel_y, Direction::WES)) {
        sel_x--;
      }
    } else if (IsKeyPressed(KEY_S)) {
      if (board.MoveBlockAt(sel_x, sel_y, Direction::SOU)) {
        sel_y++;
      }
    } else if (IsKeyPressed(KEY_D)) {
      if (board.MoveBlockAt(sel_x, sel_y, Direction::EAS)) {
        sel_x++;
      }
    }
    // TODO: Need to check for duplicate springs
    if (previous_state != board.state) {
      mass_springs.AddMass(
          1.0,
          Vector3(static_cast<float>(GetRandomValue(-1000, 1000)) / 1000.0,
                  static_cast<float>(GetRandomValue(-1000, 1000)) / 1000.0,
                  static_cast<float>(GetRandomValue(-1000, 1000)) / 1000.0),
          false, board.state);
      mass_springs.AddSpring(board.state, previous_state,
                             DEFAULT_SPRING_CONSTANT,
                             DEFAULT_DAMPENING_CONSTANT, DEFAULT_REST_LENGTH);
    }
    if (IsKeyPressed(KEY_UP)) {
      vertical += 0.1;
    } else if (IsKeyPressed(KEY_RIGHT)) {
      horizontal += 0.1;
    } else if (IsKeyPressed(KEY_DOWN)) {
      vertical -= 0.1;
    } else if (IsKeyPressed(KEY_LEFT)) {
      horizontal -= 0.1;
    }

    // Physics update
    mass_springs.ClearForces();
    mass_springs.CalculateSpringForces();
    mass_springs.CalculateRepulsionForces();
#ifdef VERLET_UPDATE
    mass_springs.VerletUpdate(frametime * SIM_SPEED);
#else
    mass_springs.EulerUpdate(frametime * SIM_SPEED);
#endif

    // Rendering
    renderer.Transform(edges, vertices, mass_springs, abstime * ROTATION_SPEED,
                       camera_distance, horizontal, vertical);
    renderer.DrawMassSprings(edges, vertices);
    renderer.DrawKlotski(board, hov_x, hov_y, sel_x, sel_y);
    renderer.DrawTextures();

    abstime += frametime;
  }

  CloseWindow();

  return 0;
}
