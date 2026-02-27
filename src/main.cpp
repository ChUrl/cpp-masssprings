#include <chrono>
#include <mutex>
#include <raylib.h>
#include <raymath.h>

#include "config.hpp"
#include "gui.hpp"
#include "input.hpp"
#include "physics.hpp"
#include "renderer.hpp"
#include "state.hpp"

#ifdef TRACY
#include "tracy.hpp"
#include <tracy/Tracy.hpp>
#endif

// TODO: Click states in the graph to display them in the board
// TODO: Move selection accordingly when undoing moves (need to diff two states
//       and get the moved blocks)

// TODO: Add some popups (my split between input.cpp/gui.cpp makes this ugly)
//       - Next move, goto target, goto worst: Notify that the graph needs to be
//         populated
//       - Clear graph: Notify that this will clear the visited states and move
//         history
//       - Reset state: Notify that this will reset the move count
//       Remove the keybindings, as it's simpler to show the popups from the
//       button?

// TODO: Reduce memory usage
//       - State.cpp stores a lot of duplicates, do I need all of them?
//       - The memory model of the puzzle board is terrible (bitboards?)

// TODO: Improve solver
//       - Move discovery is terrible
//         - Instead of trying each direction for each block, determine the
//           possible moves more efficiently (requires a different memory model)
//       - Implement state discovery/enumeration
//         - Find all possible initial board states (single one for each
//           possible statespace). Currently wer're just finding all states
//           given the initial state
//         - Would allow to generate random puzzles with a certain move count

// NOTE: Tracy uses a huge amount of memory. For longer testing disable Tracy.

auto main(int argc, char *argv[]) -> int {
  std::string preset_file;
  if (argc != 2) {
    preset_file = "default.puzzle";
  } else {
    preset_file = argv[1];
  }

#ifdef BACKWARD
  std::cout << std::format("Backward stack-traces enabled.") << std::endl;
#else
  std::cout << std::format("Backward stack-traces disabled.") << std::endl;
#endif

#ifdef TRACY
  std::cout << std::format("Tracy adapter enabled.") << std::endl;
#else
  std::cout << std::format("Tracy adapter disabled.") << std::endl;
#endif

  // RayLib window setup
  SetTraceLogLevel(LOG_ERROR);
  SetConfigFlags(FLAG_VSYNC_HINT);
  SetConfigFlags(FLAG_MSAA_4X_HINT);
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN);
  InitWindow(INITIAL_WIDTH * 2, INITIAL_HEIGHT + MENU_HEIGHT, "MassSprings");

  // Game setup
  ThreadedPhysics physics;
  StateManager state(physics, preset_file);
  OrbitCamera3D camera;
  InputHandler input(state, camera);
  Gui gui(input, state, camera);
  Renderer renderer(camera, state, input, gui);

  std::chrono::time_point last = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> fps_accumulator(0);
  unsigned int loop_iterations = 0;

  unsigned int fps = 0;
  unsigned int ups = 0;                // Read from physics
  Vector3 mass_center = Vector3Zero(); // Read from physics
  std::vector<Vector3> masses;         // Read from physics

  // Game loop
  while (!WindowShouldClose()) {
#ifdef TRACY
    FrameMarkStart("MainThread");
#endif

    // Time tracking
    std::chrono::time_point now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> deltatime = now - last;
    fps_accumulator += deltatime;
    last = now;

    // Input update
    input.HandleInput();
    state.UpdateGraph(); // Add state added after user input

    // Read positions from physics thread
#ifdef TRACY
    FrameMarkStart("MainThreadConsumeLock");
#endif
    {
#ifdef TRACY
      std::unique_lock<LockableBase(std::mutex)> lock(physics.state.data_mtx);
#else
      std::unique_lock<std::mutex> lock(physics.state.data_mtx);
#endif

      ups = physics.state.ups;
      mass_center = physics.state.mass_center;

      // Only copy data if any has been produced
      if (physics.state.data_ready) {
        masses = physics.state.masses;

        physics.state.data_ready = false;
        physics.state.data_consumed = true;

        lock.unlock();
        // Notify the physics thread that data has been consumed
        physics.state.data_consumed_cnd.notify_all();
      }
    }
#ifdef TRACY
    FrameMarkEnd("MainThreadConsumeLock");
#endif

    // Update the camera after the physics, so target lock is smooth
    std::size_t current_index = state.CurrentMassIndex();
    if (masses.size() > current_index) {
      const Mass &current_mass = masses.at(current_index);
      camera.Update(current_mass.position, mass_center, input.camera_lock,
                    input.camera_mass_center_lock);
    }

    // Rendering
    renderer.UpdateTextureSizes();
    renderer.DrawMassSprings(masses);
    renderer.DrawKlotski();
    renderer.DrawMenu(masses);
    renderer.DrawTextures(fps, ups);

    if (fps_accumulator.count() > 1.0) {
      // Update each second
      fps = loop_iterations;
      loop_iterations = 0;
      fps_accumulator = std::chrono::duration<double>(0);
    }
    ++loop_iterations;

#ifdef TRACY
    FrameMark;
    FrameMarkEnd("MainThread");
#endif
  }

  CloseWindow();

  return 0;
}
