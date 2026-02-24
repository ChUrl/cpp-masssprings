#include <mutex>
#include <raylib.h>
#include <raymath.h>
#include <tracy/Tracy.hpp>

#include "config.hpp"
#include "input.hpp"
#include "physics.hpp"
#include "renderer.hpp"
#include "state.hpp"
#include "tracy.hpp"

// TODO: Klotski state file loading
//       - File should contain a single state per line, multiple lines possible
//       - If a file is loaded, the presets should be replaced with the states
//         from the file
//       - Automatically determine the winning condition based on a configured
//         board exit
// TODO: Graph interaction
//       - Click states to display them in the board
//       - Find shortest path to any winning state and mark it in the graph
//       - Also mark the next move along the path on the board
// TODO: Do I have a huge memory leak or is the memory just not reclaimed from
//       the C++ runtime?

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
  ThreadedPhysics physics;
  StateManager state(physics);
  InputHandler input(state);
  OrbitCamera3D camera;
  Renderer renderer(camera, state, input);

  unsigned int ups;
  std::vector<Vector3> masses;                              // Read from physics
  std::vector<std::pair<std::size_t, std::size_t>> springs; // Read from physics

  // Game loop
  while (!WindowShouldClose()) {
    FrameMarkStart("MainThread");

    // Input update
    state.previous_state = state.current_state;
    input.HandleInput();
    state.UpdateGraph(); // Add state added after user input

    // Read positions from physics thread
    FrameMarkStart("MainThreadConsumeLock");
    {
      std::unique_lock<LockableBase(std::mutex)> lock(physics.state.data_mtx);

      ups = physics.state.ups;

      // Only copy data if any has been produced
      if (physics.state.data_ready) {
        masses = physics.state.masses;
        springs = physics.state.springs;

        physics.state.data_ready = false;
        physics.state.data_consumed = true;

        lock.unlock();
        // Notify the physics thread that data has been consumed
        physics.state.data_consumed_cnd.notify_all();
      }
    }
    FrameMarkEnd("MainThreadConsumeLock");

    // Update the camera after the physics, so target lock is smooth
    std::size_t current_index = state.CurrentMassIndex();
    if (masses.size() > current_index) {
      const Mass &current_mass = masses.at(current_index);
      camera.Update(current_mass.position);
    }

    // Rendering
    renderer.UpdateTextureSizes();
    renderer.DrawMassSprings(masses, springs);
    renderer.DrawKlotski();
    renderer.DrawMenu(masses, springs);
    renderer.DrawTextures(ups);
    FrameMarkEnd("MainThread");
  }

  CloseWindow();

  return 0;
}
