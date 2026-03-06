#include <chrono>
#include <mutex>
#include <raylib.h>

#include "config.hpp"
#include "input_handler.hpp"
#include "cpu_layout_engine.hpp"
#include "renderer.hpp"
#include "state_manager.hpp"
#include "user_interface.hpp"

#include <filesystem>

#if not defined(_WIN32)
#include <boost/program_options.hpp>
namespace po = boost::program_options;
#endif

// TODO: Implement state discovery/enumeration
//       - Find all possible initial board states (single one for each possible statespace).
//         Currently wer're just finding all states given the initial state
//       - Would allow to generate random puzzles with a certain move count
// TODO: Export cluster to graphviz
// TODO: Fix naming:
//       - Target: The block that has to leave the board to win
//       - Goal: The opening in the board for the target
//       - Puzzle (not board or state): A puzzle configuration (width, height, goal_x, goal_y, restricted, goal)
//       - Block: A puzzle block (x, y, width, height, target, immovable)
//       - Puzzle State: A specific puzzle state (width, height, goal_x, goal_y, restricted, goal, blocks)
//       - Cluster: A graph of puzzle states connected by moves, generated from a specific Puzzle State
//       - Puzzle Space: A number of Clusters generated from a generic Puzzle
// TODO: Add state space generation time to debug overlay
// TODO: Move selection accordingly when undoing moves (need to diff two states and get the moved blocks)

// TODO: Click states in the graph to display them in the board

#ifdef THREADPOOL
auto set_pool_thread_name(size_t idx) -> void
{
    BS::this_thread::set_os_thread_name(std::format("worker-{}", idx));
}

BS::thread_pool<> threads(std::thread::hardware_concurrency() - 2, set_pool_thread_name);
constexpr std::optional<BS::thread_pool<>* const> thread_pool = &threads;
#else
constexpr std::optional<BS::thread_pool<>* const> thread_pool = std::nullopt;
#endif

std::string preset_file = "default.puzzle";
std::string output_file = "clusters.puzzle";
int max_blocks = 5;
int board_width = 6;
int board_height = 6;
int goal_x = 4;
int goal_y = 2;
bool restricted = true;

auto ui_mode() -> int
{
    // RayLib window setup
    SetTraceLogLevel(LOG_ERROR);
    SetConfigFlags(FLAG_VSYNC_HINT);
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN);
    InitWindow(INITIAL_WIDTH * 2, INITIAL_HEIGHT + MENU_HEIGHT, "MassSprings");

    // Game setup
    cpu_layout_engine physics(thread_pool);
    state_manager state(physics, preset_file);
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
        size_t current_index = state.get_current_index();
        if (masses.size() > current_index) {
            const Vector3& current_mass = masses[current_index];
            camera.update(current_mass, mass_center, input.camera_lock, input.camera_mass_center_lock);
        }

        // Rendering
        renderer.render(masses, fps, ups, mass_count, spring_count);

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

auto rush_hour_puzzle_space() -> int
{
    const boost::unordered_flat_set<puzzle::block, block_hasher2, block_equal2> permitted_blocks = {
        puzzle::block(0, 0, 2, 1, false, false),
        puzzle::block(0, 0, 3, 1, false, false),
        puzzle::block(0, 0, 1, 2, false, false),
        puzzle::block(0, 0, 1, 3, false, false)
    };
    const puzzle::block target_block = puzzle::block(0, 0, 2, 1, true, false);
    const std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> target_block_pos_range = {
        0,
        goal_y,
        board_width - target_block.get_width() - 1,
        goal_y
    };

    infoln("Exploring Rush-Hour puzzle space:");
    infoln("- Size:       {}x{}", board_width, board_height);
    infoln("- Goal:       {},{}", goal_x, goal_y);
    infoln("- Restricted: {}", restricted);
    infoln("- Max Blocks: {}", max_blocks);
    infoln("- Target:     {}x{}", target_block.get_width(), target_block.get_height());
    infoln("- Permitted block sizes:");
    std::cout << "         ";
    for (const puzzle::block b : permitted_blocks) {
        std::cout << std::format(" {}x{},", b.get_width(), b.get_height());
    }
    std::cout << std::endl;
    const std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

    const puzzle p = puzzle(board_width, board_height, goal_x, goal_y, restricted, true);
    const boost::unordered_flat_set<puzzle, puzzle_hasher> result = p.explore_puzzle_space(
        permitted_blocks,
        target_block,
        target_block_pos_range,
        max_blocks,
        thread_pool);

    const std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
    infoln("Found {} different clusters. Took {}s.",
           result.size(),
           std::chrono::duration_cast<std::chrono::seconds>(end - start).count());

    infoln("Sorting clusters...");
    std::vector<puzzle> result_sorted{result.begin(), result.end()};
    std::ranges::sort(result_sorted, std::ranges::greater{});
    // for (const puzzle& _p : result_sorted) {
    //     traceln("{}", _p.string_repr());
    // }

    size_t i = 0;
    size_t success = 0;
    std::filesystem::remove(output_file);
    for (const puzzle& _p : result_sorted) {
        if (append_preset_file_quiet(output_file, std::format("Cluster {}", i), _p, true)) {
            ++success;
        }
        ++i;
    }
    if (success != result_sorted.size()) {
        warnln("Saved {} of {} clusters", success, result_sorted.size());
    } else {
        infoln("Saved {} of {} clusters", success, result_sorted.size());
    }

    return 0;
}

enum class runmode
{
    USER_INTERFACE, RUSH_HOUR_PUZZLE_SPACE, EXIT,
};

auto argparse(const int argc, char* argv[]) -> runmode
{
    #if not defined(_WIN32)
    po::options_description desc("Allowed options");
    desc.add_options()                                                                                            //
        ("help", "produce help message")                                                                          //
        ("presets", po::value<std::string>()->default_value(preset_file), "load presets from file")               //
        ("output", po::value<std::string>()->default_value(output_file), "output file for generated clusters")    //
        ("space", po::value<std::string>()->value_name("rh|klotski"), "generate puzzle space with ruleset")       //
        ("w", po::value<int>()->default_value(board_width)->value_name("[3, 8]"), "board width")                  //
        ("h", po::value<int>()->default_value(board_height)->value_name("[3, 8"), "board height")                 //
        ("gx", po::value<int>()->default_value(goal_x)->value_name("[0, w-1]"), "board goal horizontal position") //
        ("gy", po::value<int>()->default_value(goal_y)->value_name("[0, h-1]"), "board goal vertical position")   //
        ("free", "allow free block movement")                                                                     //
        ("blocks",
         po::value<int>()->default_value(max_blocks)->value_name("[1, 15]"),
         "block limit for puzzle space generation") //
        ;

    po::positional_options_description positional;
    positional.add("presets", -1);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(desc).positional(positional).run(), vm);
    po::notify(vm);

    if (vm.contains("help")) {
        std::cout << desc << std::endl;
        return runmode::EXIT;
    }

    if (vm.contains("output")) {
        output_file = vm["output"].as<std::string>();
    }

    if (vm.contains("w")) {
        board_width = vm["w"].as<int>();
        board_width = std::max(static_cast<int>(puzzle::MIN_WIDTH),
                               std::min(board_width, static_cast<int>(puzzle::MAX_WIDTH)));
    }

    if (vm.contains("h")) {
        board_height = vm["h"].as<int>();
        board_height = std::max(static_cast<int>(puzzle::MIN_HEIGHT),
                                std::min(board_height, static_cast<int>(puzzle::MAX_HEIGHT)));
    }

    if (vm.contains("gx")) {
        goal_x = vm["gx"].as<int>();
        goal_x = std::max(0, std::min(goal_x, static_cast<int>(puzzle::MAX_WIDTH) - 1));
    }

    if (vm.contains("gy")) {
        goal_y = vm["gy"].as<int>();
        goal_y = std::max(0, std::min(goal_y, static_cast<int>(puzzle::MAX_HEIGHT) - 1));
    }

    if (vm.contains("free")) {
        restricted = false;
    }

    if (vm.contains("blocks")) {
        max_blocks = vm["blocks"].as<int>();
        max_blocks = std::max(1, std::min(max_blocks, static_cast<int>(puzzle::MAX_BLOCKS)));
    }

    if (vm.contains("space")) {
        const std::string ruleset = vm["space"].as<std::string>();
        if (ruleset == "rh") {
            return runmode::RUSH_HOUR_PUZZLE_SPACE;
        }
        if (ruleset == "klotski") {
            throw std::runtime_error("Not implemented");
        }
    }

    if (vm.contains("presets")) {
        preset_file = vm["presets"].as<std::string>();
    }
    #endif

    return runmode::USER_INTERFACE;
}

auto main(const int argc, char* argv[]) -> int
{
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

    infoln("Using background thread for physics.");
    infoln("Using octree-barnes-hut for graph layout.");

    #ifdef THREADPOOL
    infoln("Additional thread-pool enabled ({} threads).", threads.get_thread_count());
    #else
    infoln("Additional thread-pool disabled.");
    #endif

    switch (argparse(argc, argv)) {
    case runmode::USER_INTERFACE:
        return ui_mode();
    case runmode::RUSH_HOUR_PUZZLE_SPACE:
        return rush_hour_puzzle_space();
    case runmode::EXIT:
        return 0;
    };

    return 1;
}