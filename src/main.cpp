#include "config.hpp"
#include "input.hpp"
#include "physics.hpp"
#include "renderer.hpp"
#include "state.hpp"
#include "tracy.hpp"

#include <raylib.h>
#include <raymath.h>
#include <tracy/Tracy.hpp>

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

auto main(int argc, char *argv[]) -> int {
  // if (argc < 2) {
  //   std::cout << "Missing .klotski file." << std::endl;
  //   return 1;
  // }

  // RayLib window setup
  SetTraceLogLevel(LOG_ERROR);
  // SetConfigFlags(FLAG_VSYNC_HINT);
  SetConfigFlags(FLAG_MSAA_4X_HINT);
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN);
  InitWindow(INITIAL_WIDTH * 2, INITIAL_HEIGHT + MENU_HEIGHT, "MassSprings");

  // Game setup
  OrbitCamera3D camera;
  Renderer renderer(camera);
  MassSpringSystem mass_springs;
  StateManager state(mass_springs);
  InputHandler input(state, renderer);

  // Game loop
  double timestep_accumulator = 0.0;
  while (!WindowShouldClose()) {
    timestep_accumulator += GetFrameTime();

    // Input update
    state.previous_state = state.current_state;
    input.HandleInput();
    state.UpdateGraph(); // Add state added after user input

    // Physics update
    if (timestep_accumulator > TIMESTEP) {
      // Do not try to catch up if we're falling behind. Frametimes would get
      // larger, resulting in more catching up, resulting in even larger
      // frametimes -> death spiral.
      mass_springs.ClearForces();
      mass_springs.CalculateSpringForces();
      mass_springs.CalculateRepulsionForces();
      mass_springs.VerletUpdate(TIMESTEP * SIM_SPEED);

      timestep_accumulator -= TIMESTEP;
    }

    // Update the camera after the physics, so target lock is smooth
    camera.Update(mass_springs.GetMass(state.current_state).position);

    // Rendering
    renderer.UpdateTextureSizes();
    renderer.DrawMassSprings(mass_springs, state.current_state,
                             state.starting_state, state.winning_states,
                             state.visited_states);

    renderer.DrawKlotski(state.current_state, input.hov_x, input.hov_y,
                         input.sel_x, input.sel_y, input.block_add_x,
                         input.block_add_y, state.CurrentWinCondition());
    renderer.DrawMenu(mass_springs, state.current_preset, state.current_state,
                      state.winning_states);
    renderer.DrawTextures();
  }

  CloseWindow();

  return 0;
}
