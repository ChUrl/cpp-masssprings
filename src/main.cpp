#include <iostream>
#include <raylib.h>
#include <raymath.h>

#include "config.hpp"
#include "klotski.hpp"
#include "mass_springs.hpp"
#include "renderer.hpp"
#include "states.hpp"

#ifndef WEB
#include <omp.h>
#endif
#ifdef PRINT_TIMINGS
#include <chrono>
#include <ratio>
#endif

// TODO: Klotski state file loading
//       - File should contain a single state per line, multiple lines possible
//       - If a file is loaded, the presets should be replaced with the states
//         from the file
//       - Automatically determine the winning condition based on a configured
//         board exit
// TODO: Graph interaction
//       - Click states to display them in the board
//       - Find shortest path to any winning state and mark it in the graph
//         - Also mark the next move along the path on the board

auto apply_state(MassSpringSystem &mass_springs, StateGenerator generator)
    -> State {
  mass_springs.springs.clear();
  mass_springs.masses.clear();

  State s = generator();
  mass_springs.AddMass(MASS, Vector3Zero(), false, s.state);

  return s;
};

auto populate_masssprings(MassSpringSystem &mass_springs,
                          const State &current_state) -> void {
  std::pair<std::unordered_set<std::string>,
            std::vector<std::pair<std::string, std::string>>>
      closure = current_state.Closure();
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

auto clear_masssprings(MassSpringSystem &masssprings,
                       const State &current_state) -> std::string {
  masssprings.masses.clear();
  masssprings.springs.clear();
  masssprings.AddMass(MASS, Vector3Zero(), false, current_state.state);
  return current_state.state;
}

auto main(int argc, char *argv[]) -> int {
  // if (argc < 2) {
  //   std::cout << "Missing .klotski file." << std::endl;
  //   return 1;
  // }

#ifndef WEB
  std::cout << "OpenMP: " << omp_get_max_threads() << " threads." << std::endl;
#endif

  SetTraceLogLevel(LOG_ERROR);

  // SetTargetFPS(60);
  SetConfigFlags(FLAG_VSYNC_HINT);
  SetConfigFlags(FLAG_MSAA_4X_HINT);
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN);

  InitWindow(INITIAL_WIDTH * 2, INITIAL_HEIGHT + MENU_HEIGHT, "MassSprings");

  // Rendering configuration
  Renderer renderer;

  // Klotski configuration
  int current_preset = 0;
  MassSpringSystem masssprings;
  State current_state = apply_state(masssprings, generators[current_preset]);

  // Game loop
  float frametime;
  bool has_block_add_xy = false;
  int block_add_x = -1;
  int block_add_y = -1;
  int hov_x = 0;
  int hov_y = 0;
  int sel_x = 0;
  int sel_y = 0;
#ifdef PRINT_TIMINGS
  double last_print_time = GetTime();
  std::chrono::duration<double, std::milli> physics_time_accumulator =
      std::chrono::duration<double, std::milli>(0);
  std::chrono::duration<double, std::milli> render_time_accumulator =
      std::chrono::duration<double, std::milli>(0);
  int time_measure_count = 0;
#endif
  while (!WindowShouldClose()) {
    frametime = GetFrameTime();
    std::string previous_state = current_state.state;

    // Mouse handling
    const int board_width = GetScreenWidth() / 2.0 - 2 * BOARD_PADDING;
    const int board_height =
        GetScreenHeight() - MENU_HEIGHT - 2 * BOARD_PADDING;
    int block_size = std::min(board_width / current_state.width,
                              board_height / current_state.height) -
                     2 * BLOCK_PADDING;
    int x_offset =
        (board_width - (block_size + 2 * BLOCK_PADDING) * current_state.width) /
        2.0;
    int y_offset = (board_height -
                    (block_size + 2 * BLOCK_PADDING) * current_state.height) /
                   2.0;
    Vector2 m = GetMousePosition();
    if (m.x < x_offset) {
      hov_x = 100;
    } else {
      hov_x = (m.x - x_offset) / (block_size + 2 * BLOCK_PADDING);
    }
    if (m.y - MENU_HEIGHT < y_offset) {
      hov_y = 100;
    } else {
      hov_y = (m.y - MENU_HEIGHT - y_offset) / (block_size + 2 * BLOCK_PADDING);
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      // If we clicked a block...
      if (current_state.GetBlock(hov_x, hov_y).IsValid()) {
        sel_x = hov_x;
        sel_y = hov_y;
      }
      // If we clicked empty space...
      else {
        // Select a position
        if (!has_block_add_xy) {
          if (hov_x >= 0 && hov_x < current_state.width && hov_y >= 0 &&
              hov_y < current_state.height) {
            block_add_x = hov_x;
            block_add_y = hov_y;
            has_block_add_xy = true;
          }
        }
        // If we have already selected a position
        else {
          int block_add_width = hov_x - block_add_x + 1;
          int block_add_height = hov_y - block_add_y + 1;
          if (block_add_width <= 0 || block_add_height <= 0) {
            block_add_x = -1;
            block_add_y = -1;
            has_block_add_xy = false;
          } else if (block_add_x >= 0 &&
                     block_add_x + block_add_width <= current_state.width &&
                     block_add_y >= 0 &&
                     block_add_y + block_add_height <= current_state.height) {
            bool success = current_state.AddBlock(
                Block(block_add_x, block_add_y, block_add_width,
                      block_add_height, false));

            if (success) {
              block_add_x = -1;
              block_add_y = -1;
              has_block_add_xy = false;
              previous_state = clear_masssprings(masssprings, current_state);
            }
          }
        }
      }
    } else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
      if (current_state.RemoveBlock(hov_x, hov_y)) {
        previous_state = clear_masssprings(masssprings, current_state);
      } else if (has_block_add_xy) {
        block_add_x = -1;
        block_add_y = -1;
        has_block_add_xy = false;
      }
    }

    // Key handling
    if (IsKeyPressed(KEY_W)) {
      if (current_state.MoveBlockAt(sel_x, sel_y, Direction::NOR)) {
        sel_y--;
      }
    } else if (IsKeyPressed(KEY_A)) {
      if (current_state.MoveBlockAt(sel_x, sel_y, Direction::WES)) {
        sel_x--;
      }
    } else if (IsKeyPressed(KEY_S)) {
      if (current_state.MoveBlockAt(sel_x, sel_y, Direction::SOU)) {
        sel_y++;
      }
    } else if (IsKeyPressed(KEY_D)) {
      if (current_state.MoveBlockAt(sel_x, sel_y, Direction::EAS)) {
        sel_x++;
      }
    } else if (IsKeyPressed(KEY_P)) {
      std::cout << "State: " << current_state.state << std::endl;
      Block sel = current_state.GetBlock(sel_x, sel_y);
      int idx = current_state.GetIndex(sel.x, sel.y) - 5;
      if (sel.IsValid()) {
        std::cout << "Sel:   " << current_state.state.substr(0, 5)
                  << std::string(idx, '.') << sel.ToString()
                  << std::string(current_state.state.length() - idx - 7, '.')
                  << std::endl;
      }
    } else if (IsKeyPressed(KEY_N)) {
      block_add_x = -1;
      block_add_y = -1;
      has_block_add_xy = false;
      current_preset =
          (generators.size() + current_preset - 1) % generators.size();
      current_state = apply_state(masssprings, generators[current_preset]);
      previous_state = current_state.state;
    } else if (IsKeyPressed(KEY_M)) {
      block_add_x = -1;
      block_add_y = -1;
      has_block_add_xy = false;
      current_preset = (current_preset + 1) % generators.size();
      current_state = apply_state(masssprings, generators[current_preset]);
      previous_state = current_state.state;
    } else if (IsKeyPressed(KEY_R)) {
      current_state = generators[current_preset]();
      previous_state = clear_masssprings(masssprings, current_state);
    } else if (IsKeyPressed(KEY_G)) {
      previous_state = clear_masssprings(masssprings, current_state);
      populate_masssprings(masssprings, current_state);
      renderer.UpdateWinningStates(masssprings, win_conditions[current_preset]);
    } else if (IsKeyPressed(KEY_C)) {
      // We also need to clear the graph, in case the state has been edited.
      // Then the graph would contain states that are impossible.
      previous_state = clear_masssprings(masssprings, current_state);
    } else if (IsKeyPressed(KEY_I)) {
      renderer.mark_solutions = !renderer.mark_solutions;
    } else if (IsKeyPressed(KEY_O)) {
      renderer.connect_solutions = !renderer.connect_solutions;
    } else if (IsKeyPressed(KEY_F)) {
      current_state.restricted = !current_state.restricted;
    } else if (IsKeyPressed(KEY_T)) {
      current_state.ToggleTarget(sel_x, sel_y);
      previous_state = clear_masssprings(masssprings, current_state);
    } else if (IsKeyPressed(KEY_LEFT) && current_state.width > 1) {
      current_state = current_state.RemoveColumn();
      previous_state = clear_masssprings(masssprings, current_state);
    } else if (IsKeyPressed(KEY_RIGHT) && current_state.width < 9) {
      current_state = current_state.AddColumn();
      previous_state = clear_masssprings(masssprings, current_state);

    } else if (IsKeyPressed(KEY_UP) && current_state.height > 1) {
      current_state = current_state.RemoveRow();
      previous_state = clear_masssprings(masssprings, current_state);
    } else if (IsKeyPressed(KEY_DOWN) && current_state.height < 9) {
      current_state = current_state.AddRow();
      previous_state = clear_masssprings(masssprings, current_state);
    }

    if (previous_state != current_state.state) {
      masssprings.AddMass(
          MASS,
          Vector3(static_cast<float>(GetRandomValue(-1000, 1000)) / 1000.0,
                  static_cast<float>(GetRandomValue(-1000, 1000)) / 1000.0,
                  static_cast<float>(GetRandomValue(-1000, 1000)) / 1000.0),
          false, current_state.state);
      masssprings.AddSpring(current_state.state, previous_state,
                            SPRING_CONSTANT, DAMPENING_CONSTANT, REST_LENGTH);
      renderer.AddWinningState(current_state, win_conditions[current_preset]);
    }

    // Physics update
#ifdef PRINT_TIMINGS
    std::chrono::high_resolution_clock::time_point ps =
        std::chrono::high_resolution_clock::now();
#endif
    for (int i = 0; i < UPDATES_PER_FRAME; ++i) {
      masssprings.ClearForces();
      masssprings.CalculateSpringForces();
      masssprings.CalculateRepulsionForces();
#ifdef VERLET_UPDATE
      masssprings.VerletUpdate(frametime / UPDATES_PER_FRAME * SIM_SPEED);
#else
      mass_springs.EulerUpdate(frametime * SIM_SPEED);
#endif
    }
#ifdef PRINT_TIMINGS
    std::chrono::high_resolution_clock::time_point pe =
        std::chrono::high_resolution_clock::now();
    physics_time_accumulator += pe - ps;
#endif

    // Rendering
#ifdef PRINT_TIMINGS
    std::chrono::high_resolution_clock::time_point rs =
        std::chrono::high_resolution_clock::now();
#endif
    renderer.UpdateCamera(masssprings, current_state);
    renderer.UpdateTextureSizes();
    renderer.DrawMassSprings(masssprings, current_state);
    renderer.DrawKlotski(current_state, hov_x, hov_y, sel_x, sel_y, block_add_x,
                         block_add_y, win_conditions[current_preset]);
    renderer.DrawMenu(masssprings, current_preset, current_state);
    renderer.DrawTextures();
#ifdef PRINT_TIMINGS
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
#endif
  }

  CloseWindow();

  return 0;
}
