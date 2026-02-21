#include <iostream>
#include <raylib.h>
#include <raymath.h>

#include "config.hpp"
#include "input.hpp"
#include "mass_springs.hpp"
#include "renderer.hpp"
#include "state.hpp"

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
// TODO: Don't tie the simulation step resolution to the FPS (frametime)
//       - This breaks the simulation on slower systems
//       - Add a modifiable speed setting?
//       - Clamp the frametime?
//       - Use a fixed step size and control how often it runs per frame?

auto main(int argc, char *argv[]) -> int {
  // if (argc < 2) {
  //   std::cout << "Missing .klotski file." << std::endl;
  //   return 1;
  // }

#ifndef WEB
  std::cout << "OpenMP: " << omp_get_max_threads() << " threads." << std::endl;
#endif

  // RayLib window setup
  SetTraceLogLevel(LOG_ERROR);
  SetConfigFlags(FLAG_VSYNC_HINT);
  SetConfigFlags(FLAG_MSAA_4X_HINT);
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN);
  InitWindow(INITIAL_WIDTH * 2, INITIAL_HEIGHT + MENU_HEIGHT, "MassSprings");

  // Game setup
  Renderer renderer;
  MassSpringSystem mass_springs;
  StateManager state = StateManager(mass_springs);
  InputHandler input = InputHandler(state, renderer);

  // Game loop
#ifdef PRINT_TIMINGS
  double last_print_time = GetTime();
  std::chrono::duration<double, std::milli> physics_time_accumulator =
      std::chrono::duration<double, std::milli>(0);
  std::chrono::duration<double, std::milli> render_time_accumulator =
      std::chrono::duration<double, std::milli>(0);
  int time_measure_count = 0;
#endif
  while (!WindowShouldClose()) {
    // Input update
    state.previous_state = state.current_state;
    input.HandleInput();
    state.UpdateGraph();

    // Physics update
#ifdef PRINT_TIMINGS
    std::chrono::high_resolution_clock::time_point ps =
        std::chrono::high_resolution_clock::now();
#endif
    for (int i = 0; i < UPDATES_PER_FRAME; ++i) {
      mass_springs.ClearForces();
      mass_springs.CalculateSpringForces();
      mass_springs.CalculateRepulsionForces();
#ifdef VERLET_UPDATE
      mass_springs.VerletUpdate(GetFrameTime() / UPDATES_PER_FRAME * SIM_SPEED);
#else
      mass_springs.EulerUpdate(GetFrameTime() * SIM_SPEED);
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
    renderer.UpdateCamera(mass_springs, state.current_state);
    renderer.UpdateTextureSizes();
    renderer.DrawMassSprings(mass_springs, state.current_state,
                             state.winning_states);
    renderer.DrawKlotski(state.current_state, input.hov_x, input.hov_y,
                         input.sel_x, input.sel_y, input.block_add_x,
                         input.block_add_y, state.CurrentWinCondition());
    renderer.DrawMenu(mass_springs, state.current_preset, state.current_state,
                      state.winning_states);
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
