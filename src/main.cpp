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
  // Block e = Block(1, 2, 2, 1, false);
  // Block f = Block(3, 2, 1, 2, false);
  // Block g = Block(1, 3, 1, 1, false);
  // Block h = Block(2, 3, 1, 1, false);
  // Block i = Block(0, 4, 1, 1, false);
  // Block j = Block(3, 4, 1, 1, false);

  s.AddBlock(a);
  s.AddBlock(b);
  s.AddBlock(c);
  s.AddBlock(d);
  // s.AddBlock(e);
  // s.AddBlock(f);
  // s.AddBlock(g);
  // s.AddBlock(h);
  // s.AddBlock(i);
  // s.AddBlock(j);

  return s;
}

auto main(int argc, char *argv[]) -> int {
  // if (argc < 2) {
  //   std::cout << "Missing .klotski file." << std::endl;
  //   return 1;
  // }

  SetTraceLogLevel(LOG_ERROR);

  // SetTargetFPS(165);
  SetConfigFlags(FLAG_VSYNC_HINT);
  SetConfigFlags(FLAG_MSAA_4X_HINT);
  // SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN);

  InitWindow(WIDTH * 2, HEIGHT, "MassSprings");

  // Mass springs configuration
  MassSpringSystem mass_springs;

  // Klotski configuration
  State board = klotski_a();
  mass_springs.AddMass(1.0, Vector3Zero(), true, board.state);

  // Closure solving
  std::pair<std::unordered_set<std::string>,
            std::vector<std::pair<std::string, std::string>>>
      closure = board.Closure();
  for (const auto &state : closure.first) {
    Vector3 pos =
        Vector3(static_cast<float>(GetRandomValue(-10000, 10000)) / 1000.0,
                static_cast<float>(GetRandomValue(-10000, 10000)) / 1000.0,
                static_cast<float>(GetRandomValue(-10000, 10000)) / 1000.0);

    mass_springs.AddMass(1.0, pos, false, state);
  }
  for (const auto &[from, to] : closure.second) {
    mass_springs.AddSpring(from, to, SPRING_CONSTANT, DAMPENING_CONSTANT,
                           REST_LENGTH);
  }
  std::cout << "Inserted " << mass_springs.masses.size() << " masses and "
            << mass_springs.springs.size() << " springs." << std::endl;
  std::cout << "Consuming "
            << sizeof(decltype(*mass_springs.masses.begin())) *
                   mass_springs.masses.size()
            << " Bytes for masses." << std::endl;
  std::cout << "Consuming "
            << sizeof(decltype(*mass_springs.springs.begin())) *
                   mass_springs.springs.size()
            << " Bytes for springs." << std::endl;

  // Rendering configuration
  Renderer renderer(WIDTH, HEIGHT);

  // Game loop
  float frametime;
  int hov_x = 0;
  int hov_y = 0;
  int sel_x = 0;
  int sel_y = 0;
  while (!WindowShouldClose()) {
    frametime = GetFrameTime();

    // Mouse handling
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
    Vector2 m = GetMousePosition();
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
    } else if (IsKeyPressed(KEY_P)) {
      std::cout << board.state << std::endl;
    }
    if (previous_state != board.state) {
      mass_springs.AddMass(
          1.0,
          Vector3(static_cast<float>(GetRandomValue(-1000, 1000)) / 1000.0,
                  static_cast<float>(GetRandomValue(-1000, 1000)) / 1000.0,
                  static_cast<float>(GetRandomValue(-1000, 1000)) / 1000.0),
          false, board.state);
      mass_springs.AddSpring(board.state, previous_state, SPRING_CONSTANT,
                             DAMPENING_CONSTANT, REST_LENGTH);
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
    renderer.DrawMassSprings(mass_springs);
    renderer.DrawKlotski(board, hov_x, hov_y, sel_x, sel_y);
    renderer.DrawTextures();
    renderer.UpdateCamera();
  }

  CloseWindow();

  return 0;
}
