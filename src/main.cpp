#include "cli.hpp"
#include "config.hpp"
#include "cpu_layout_engine.hpp"
#include "gpu_context.hpp"
#include "input_handler.hpp"
#include "puzzle_registry.hpp"
#include "renderer.hpp"
#include "settings.hpp"
#include "state_manager.hpp"
#include "user_interface.hpp"

#include <GL/glew.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <format>
#include <memory>
#include <mutex>
#include <raylib.h>
#include <thread>
#include <vector>

// Threadpool setup
#ifdef THREADPOOL
auto set_pool_thread_name(size_t idx) -> void
{
    BS::this_thread::set_os_thread_name(std::format("worker-{}", idx));
}

std::unique_ptr<BS::thread_pool<>> threads;
threadpool thread_pool = std::nullopt;
#else
constexpr threadpool thread_pool = std::nullopt;
#endif

// TODO: Export cluster to graphviz
// TODO: Fix naming:
//       - Target: The block that has to leave the board to win
//       - Goal: The opening in the board for the target
//       - Puzzle (not board or state): A puzzle configuration (width, height, goal_x, goal_y,
//       restricted, goal)
//       - Block: A puzzle block (x, y, width, height, target, immovable)
//       - Puzzle State: A specific puzzle state (width, height, goal_x, goal_y, restricted, goal,
//       blocks)
//       - Cluster: A graph of puzzle states connected by moves, generated from a specific Puzzle
//       State
//       - Puzzle Space: A number of Clusters generated from a generic Puzzle
// TODO: Add state space generation time to debug overlay
// TODO: Move selection accordingly when undoing moves (need to diff two states and get the moved
// blocks)

auto ui_mode(const cli_options& options) -> int
{
#ifdef THREADPOOL
    {
        const simulation_settings cfg = get_settings();
        const unsigned int hardware = std::thread::hardware_concurrency();
        const unsigned int default_workers = hardware > 2 ? hardware - 2 : 1;
        const unsigned int workers = cfg.worker_threads > 0
            ? static_cast<unsigned int>(cfg.worker_threads)
            : default_workers;
        threads = std::make_unique<BS::thread_pool<>>(std::max(1u, workers), set_pool_thread_name);
        thread_pool = threads.get();
        set_active_worker_threads(static_cast<int>(threads->get_thread_count()));
        infoln("Additional thread-pool enabled ({} threads).", threads->get_thread_count());
    }
#else
    infoln("Additional thread-pool disabled.");
#endif

    infoln("Using background thread for physics.");
    infoln("Using linear octree + Barnes-Hut for graph layout.");
    infoln("Octree build mode: {}.", get_settings().async_octree ? "asynchronous" : "synchronous");

    // RayLib window setup
    SetTraceLogLevel(LOG_ERROR);
    SetConfigFlags(FLAG_VSYNC_HINT);
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN);
    InitWindow(INITIAL_WIDTH * 2, INITIAL_HEIGHT + COMPACT_MENU_HEIGHT, "MassSprings");

    // GLEW setup
    glewExperimental = GL_TRUE;
    const GLenum glew_err = glewInit();
    if (glew_err != GLEW_OK) {
        TraceLog(LOG_FATAL, "Failed to initialize GLEW: %s", glewGetErrorString(glew_err));
    }

    {
        gpu_context gpu;
        if (!gpu.init(glfwGetCurrentContext())) {
            simulation_settings settings = get_settings();
            if (settings.gpu_physics) {
                settings.gpu_physics = false;
                set_settings(settings);
            }
            warnln("GPU compute unavailable: {}", gpu::runtime_status().error);
        } else {
            infoln("GPU compute context: {} ({})", gpu::runtime_status().version,
                   gpu::runtime_status().renderer);
        }

        // The asynchronous tree worker is currently disabled: it competes with the renderer for
        // the same GPU and made the frame rate worse. The synchronous GPU tree path is faster in
        // the full pipeline until tree construction itself is cheaper.
        cpu_layout_engine physics(thread_pool, &gpu);
        puzzle_registry registry;
        register_builtin_puzzles(registry);
        state_manager state(physics, registry, options.preset_file);
        if (!options.puzzle_type.empty()) {
            if (state.switch_puzzle(options.puzzle_type)) {
                infoln("Starting with puzzle type {}.", options.puzzle_type);
            } else {
                warnln("Unknown puzzle type \"{}\".", options.puzzle_type);
            }
        }
        if (options.preset_index >= 0) {
            if (static_cast<size_t>(options.preset_index) < state.get_preset_count()) {
                state.load_preset(static_cast<size_t>(options.preset_index));
                infoln("Starting with preset {}: {}", options.preset_index,
                       state.get_current_preset_comment());
            } else {
                warnln("Preset index {} is out of range (0..{}).", options.preset_index,
                       state.get_preset_count() - 1);
            }
        }
        if (options.populate) {
            infoln("Populating graph for preset {}...", state.get_current_preset());
            state.start_state_space_search();
        }
        orbit_camera camera;
        input_handler input(state, camera);
        user_interface gui(input, state, camera);
        renderer renderer(camera, state, input, gui);

        std::chrono::time_point last = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> fps_accumulator(0);
        int loop_iterations = 0;

        int fps = 0;
        int ups = 0;                 // Read from physics
        Vector3 mass_center;         // Read from physics
        std::vector<Vector3> masses; // Read from physics
        size_t mass_count = 0;
        size_t spring_count = 0;
        float physics_step_ms = 0.0f;
        float physics_build_ms = 0.0f;
        float physics_tree_ms = 0.0f;
        float physics_tree_cpu_ms = 0.0f;
        float physics_wait_ms = 0.0f;
        size_t physics_tree_nodes = 0;
        size_t physics_tree_children = 0;
        std::array<float, 6> physics_tree_stages{};
        float physics_spring_ms = 0.0f;
        float physics_repulsion_ms = 0.0f;
        float physics_gpu_step_ms = 0.0f;
        float physics_integrate_ms = 0.0f;
        double render_ms = 0.0;

        // Game loop
        while (!WindowShouldClose()) {
#ifdef TRACY
            FrameMarkStart("MainThread");
#endif

            // Time tracking
            std::chrono::time_point now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> delta_time = now - last;
            fps_accumulator += delta_time;
            last = now;

            // Input update
            input.handle_input();
            state.poll_search();

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
                mass_count = physics.state.mass_count;
                spring_count = physics.state.spring_count;
                physics_step_ms = physics.state.step_ms;
                physics_build_ms = physics.state.build_ms;
                physics_tree_ms = physics.state.tree_ms;
                physics_tree_cpu_ms = physics.state.tree_cpu_ms;
                physics_wait_ms = physics.state.wait_ms;
                physics_tree_nodes = physics.state.tree_nodes;
                physics_tree_children = physics.state.tree_children;
                physics_tree_stages = physics.state.tree_stages;
                physics_spring_ms = physics.state.spring_ms;
                physics_repulsion_ms = physics.state.repulsion_ms;
                physics_gpu_step_ms = physics.state.gpu_step_ms;
                physics_integrate_ms = physics.state.integrate_ms;

                // Take ownership of the published buffer (the physics thread reuses the one we give
                // back on its next publish), avoiding a 5 MB copy in the render thread.
                if (physics.state.data_ready) {
                    masses.swap(physics.state.masses);

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
            size_t current_index = state.get_current_index();
            if (masses.size() == state.get_state_count() && masses.size() > current_index) {
                const Vector3& current_mass = masses[current_index];
                camera.update(current_mass, mass_center, input.camera_lock,
                              input.camera_mass_center_lock);
            }

            // Rendering
            const auto render_start = std::chrono::high_resolution_clock::now();
            renderer.render(masses, fps, ups, mass_count, spring_count);
            const std::chrono::duration<double, std::milli> render_elapsed =
                std::chrono::high_resolution_clock::now() - render_start;
            render_ms = 0.9 * render_ms + 0.1 * render_elapsed.count();
            gui.add_performance_sample(physics_step_ms, render_ms);

            if (fps_accumulator.count() > 1.0) {
                // Update each second
                fps = loop_iterations;
                loop_iterations = 0;
                fps_accumulator = std::chrono::duration<double>(0);
                infoln("fps={} ups={} masses={} springs={} render={:.1f}ms physics_step={:.1f}ms "
                       "nodes={} children={}",
                       fps, ups, mass_count, spring_count, render_ms, physics_step_ms,
                       physics_tree_nodes, physics_tree_children);

                const simulation_settings cfg = get_settings();
                if (cfg.log_level >= 2) {
                    traceln("Frame: frame={:.2f}ms render={:.2f}ms physics={:.2f}ms fps={} ups={}",
                            delta_time.count() * 1000.0, render_ms, physics_step_ms, fps, ups);
                    traceln("Physics: build={:.2f} tree={:.2f} cpu={:.2f} wait={:.2f} "
                            "spring={:.2f} repulse={:.2f} gpu={:.2f} integrate={:.2f}ms",
                            physics_build_ms, physics_tree_ms, physics_tree_cpu_ms, physics_wait_ms,
                            physics_spring_ms, physics_repulsion_ms, physics_gpu_step_ms,
                            physics_integrate_ms);
                    traceln("Octree: nodes={} children={} bbox={:.2f} morton={:.2f} sort={:.2f} "
                            "leaves={:.2f} levels={:.2f} flatten={:.2f}ms",
                            physics_tree_nodes, physics_tree_children, physics_tree_stages[0],
                            physics_tree_stages[1], physics_tree_stages[2], physics_tree_stages[3],
                            physics_tree_stages[4], physics_tree_stages[5]);
                    traceln("Graph: masses={} springs={} center=({:.2f}, {:.2f}, {:.2f}) "
                            "async={} depth={} rebuild={} constraint={}",
                            mass_count, spring_count, mass_center.x, mass_center.y, mass_center.z,
                            cfg.async_octree ? "on" : "off", cfg.max_depth, cfg.rebuild_interval,
                            cfg.constraint_interval);
                }
            }
            ++loop_iterations;

#ifdef TRACY
            FrameMark;
            FrameMarkEnd("MainThread");
#endif
        }
    }

    CloseWindow();
    save_settings();

    return 0;
}

auto main(const int argc, char* argv[]) -> int
{
    load_settings();

#ifdef BACKWARD
    infoln("Backward stack-traces enabled.");
#else
    infoln("Backward stack-traces disabled.");
#endif

#ifdef TRACY
    infoln("Tracy adapter enabled.");
#else
    infoln("Tracy adapter disabled.");
#endif

    const cli_options options = parse_cli(argc, argv);

    switch (options.mode) {
    case cli_mode::gui:
        return ui_mode(options);
    case cli_mode::puzzle_space:
    case cli_mode::state_space:
        return run_headless(options);
    case cli_mode::exit:
        return options.exit_code;
    }

    return 1;
}
