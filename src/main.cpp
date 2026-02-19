#define VERLET_UPDATE

#include <chrono>
#include <iostream>
#include <omp.h>
#include <ratio>
#include <raylib.h>
#include <raymath.h>

#include "config.hpp"
#include "klotski.hpp"
#include "mass_springs.hpp"
#include "renderer.hpp"
#include "states.hpp"


auto apply_state(MassSpringSystem &mass_springs, StateGenerator generator)
    -> State {
  mass_springs.springs.clear();
  mass_springs.masses.clear();

  State s = generator();
  mass_springs.AddMass(MASS, Vector3Zero(), false, s.state);

  return s;
};

auto solve_closure(MassSpringSystem &mass_springs, const State board) -> void {
  std::pair<std::unordered_set<std::string>,
            std::vector<std::pair<std::string, std::string>>>
      closure = board.Closure();
  for (const auto &state : closure.first) {
    Vector3 pos =
        Vector3(static_cast<float>(GetRandomValue(-10000, 10000)) / 1000.0,
                static_cast<float>(GetRandomValue(-10000, 10000)) / 1000.0,
                static_cast<float>(GetRandomValue(-10000, 10000)) / 1000.0);

    mass_springs.AddMass(MASS, pos, false, state);
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
}

auto main(int argc, char *argv[]) -> int {
  // if (argc < 2) {
  //   std::cout << "Missing .klotski file." << std::endl;
  //   return 1;
  // }

  std::cout << "OpenMP: " << omp_get_max_threads() << " threads." << std::endl;

  SetTraceLogLevel(LOG_ERROR);

  // SetTargetFPS(60);
  SetConfigFlags(FLAG_VSYNC_HINT);
  SetConfigFlags(FLAG_MSAA_4X_HINT);
  // SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN);

  InitWindow(WIDTH * 2, HEIGHT + MENU_HEIGHT, "MassSprings");

  // Rendering configuration
  Renderer renderer;

  // Klotski configuration
  int current_preset = 0;
  MassSpringSystem masssprings;
  State board = apply_state(masssprings, generators[current_preset]);

  // Game loop
  float frametime;
  int hov_x = 0;
  int hov_y = 0;
  int sel_x = 0;
  int sel_y = 0;
  double last_print_time = GetTime();
  std::chrono::duration<double, std::milli> physics_time_accumulator =
      std::chrono::duration<double, std::milli>(0);
  std::chrono::duration<double, std::milli> render_time_accumulator =
      std::chrono::duration<double, std::milli>(0);
  int time_measure_count = 0;
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
    if (m.y - MENU_HEIGHT < y_offset) {
      hov_y = 100;
    } else {
      hov_y = (m.y - MENU_HEIGHT - y_offset) / block_size;
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
    } else if (IsKeyPressed(KEY_N)) {
      current_preset =
          (generators.size() + current_preset - 1) % generators.size();
      board = apply_state(masssprings, generators[current_preset]);
      previous_state = board.state;
    } else if (IsKeyPressed(KEY_M)) {
      current_preset = (current_preset + 1) % generators.size();
      board = apply_state(masssprings, generators[current_preset]);
      previous_state = board.state;
    } else if (IsKeyPressed(KEY_R)) {
      board = generators[current_preset]();
      previous_state = board.state;
    } else if (IsKeyPressed(KEY_C)) {
      solve_closure(masssprings, board);
      renderer.UpdateWinningStates(masssprings, win_conditions[current_preset]);
    } else if (IsKeyPressed(KEY_G)) {
      masssprings.masses.clear();
      masssprings.springs.clear();
      masssprings.AddMass(MASS, Vector3Zero(), false, board.state);
      previous_state = board.state;
    } else if (IsKeyPressed(KEY_I)) {
      renderer.mark_solutions = !renderer.mark_solutions;
    } else if (IsKeyPressed(KEY_O)) {
      renderer.connect_solutions = !renderer.connect_solutions;
    }

    if (previous_state != board.state) {
      masssprings.AddMass(
          MASS,
          Vector3(static_cast<float>(GetRandomValue(-1000, 1000)) / 1000.0,
                  static_cast<float>(GetRandomValue(-1000, 1000)) / 1000.0,
                  static_cast<float>(GetRandomValue(-1000, 1000)) / 1000.0),
          false, board.state);
      masssprings.AddSpring(board.state, previous_state, SPRING_CONSTANT,
                            DAMPENING_CONSTANT, REST_LENGTH);
      renderer.AddWinningState(board, win_conditions[current_preset]);
    }

    // Physics update
    std::chrono::high_resolution_clock::time_point ps =
        std::chrono::high_resolution_clock::now();
    masssprings.ClearForces();
    masssprings.CalculateSpringForces();
    masssprings.CalculateRepulsionForces();
#ifdef VERLET_UPDATE
    masssprings.VerletUpdate(frametime * SIM_SPEED);
#else
    mass_springs.EulerUpdate(frametime * SIM_SPEED);
#endif
    std::chrono::high_resolution_clock::time_point pe =
        std::chrono::high_resolution_clock::now();
    physics_time_accumulator += pe - ps;

    // Rendering
    std::chrono::high_resolution_clock::time_point rs =
        std::chrono::high_resolution_clock::now();
    renderer.UpdateCamera(masssprings, board);
    renderer.DrawMassSprings(masssprings, board);
    renderer.DrawKlotski(board, hov_x, hov_y, sel_x, sel_y);
    renderer.DrawMenu(masssprings, current_preset);
    renderer.DrawTextures();
    std::chrono::high_resolution_clock::time_point re =
        std::chrono::high_resolution_clock::now();
    render_time_accumulator += re - rs;

    time_measure_count++;
    if (GetTime() - last_print_time > 10.0) {
      std::cout << " - Physics time avg: "
                << physics_time_accumulator / time_measure_count << "."
                << std::endl;
      std::cout << " - Render time avg:  "
                << render_time_accumulator / time_measure_count << "."
                << std::endl;
      last_print_time = GetTime();
      physics_time_accumulator = std::chrono::duration<double, std::milli>(0);
      render_time_accumulator = std::chrono::duration<double, std::milli>(0);
      time_measure_count = 0;
    }
  }

  CloseWindow();

  return 0;
}
