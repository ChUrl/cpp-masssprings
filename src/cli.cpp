#include "cli.hpp"

#include "config.hpp"
#include "load_save.hpp"
#include "puzzle.hpp"
#include "puzzle_registry.hpp"
#include "puzzle_space.hpp"
#include "puzzle_space_io.hpp"
#include "search_controller.hpp"
#include "settings.hpp"
#include "sliding_block_model.hpp"
#include "util.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <format>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#if not defined(_WIN32)
    #include <boost/program_options.hpp>
namespace po = boost::program_options;
#endif

namespace
{
volatile std::sig_atomic_t g_interrupted = 0;

extern "C" void cli_handle_sigint(int)
{
    g_interrupted = 1;
}

auto phase_name(const puzzle_space_phase phase) -> const char*
{
    switch (phase) {
    case puzzle_space_phase::idle:
        return "idle";
    case puzzle_space_phase::inventories:
        return "preparing";
    case puzzle_space_phase::starting_positions:
        return "starting-positions";
    case puzzle_space_phase::counting:
        return "counting";
    case puzzle_space_phase::probing:
        return "preparing-partitions";
    case puzzle_space_phase::exploring:
        return "exploring";
    case puzzle_space_phase::finishing:
        return "finishing";
    case puzzle_space_phase::done:
        return "done";
    case puzzle_space_phase::cancelled:
        return "cancelled";
    case puzzle_space_phase::failed:
        return "failed";
    }
    return "unknown";
}

class search_thread_pool
{
public:
    explicit search_thread_pool(const int threads)
    {
#ifdef THREADPOOL
        count_ = std::max(1, resolve_search_threads(threads));
        pool_ = std::make_unique<BS::thread_pool<>>(static_cast<unsigned int>(count_));
#else
        (void)threads;
        count_ = 1;
#endif
    }

    [[nodiscard]] auto get() -> threadpool
    {
#ifdef THREADPOOL
        return pool_ ? threadpool{pool_.get()} : std::nullopt;
#else
        return std::nullopt;
#endif
    }

    [[nodiscard]] auto count() const -> int
    {
        return count_;
    }

private:
    std::unique_ptr<BS::thread_pool<>> pool_;
    int count_ = 1;
};

struct loaded_preset
{
    preset_entry entry;
    std::unique_ptr<puzzle_kind> kind;
    state_id id = 0;
    size_t index = 0;
};

auto load_preset(const cli_options& options, const std::string& required_type)
    -> std::optional<loaded_preset>
{
    const std::vector<preset_entry> presets = parse_preset_file(options.preset_file);
    if (presets.empty()) {
        errln("Preset file \"{}\" contains no presets.", options.preset_file);
        return std::nullopt;
    }

    size_t index = 0;
    if (!required_type.empty() && !options.preset_given) {
        const auto preset = std::ranges::find_if(presets, [&](const preset_entry& entry)
                                                 { return entry.type == required_type; });
        if (preset == presets.end()) {
            errln("Preset file \"{}\" contains no {} preset.", options.preset_file, required_type);
            return std::nullopt;
        }
        index = static_cast<size_t>(std::distance(presets.begin(), preset));
    } else {
        if (options.preset_index < 0 ||
            static_cast<size_t>(options.preset_index) >= presets.size()) {
            errln("Preset index {} is out of range (0..{}).", options.preset_index,
                  presets.size() - 1);
            return std::nullopt;
        }
        index = static_cast<size_t>(options.preset_index);
    }

    const preset_entry& entry = presets[index];
    if (!required_type.empty() && entry.type != required_type) {
        errln("Preset {} is a {} preset, not a {} preset.", index, entry.type, required_type);
        return std::nullopt;
    }

    puzzle_registry registry;
    register_builtin_puzzles(registry);
    std::unique_ptr<puzzle_kind> kind = registry.create(entry.type);
    if (!kind) {
        errln("Failed to create puzzle type \"{}\".", entry.type);
        return std::nullopt;
    }

    const std::optional<state_id> id = kind->codec->parse(entry.payload, *kind->model);
    if (!id) {
        errln("Failed to parse preset \"{}\" as {}.", entry.comment, entry.type);
        return std::nullopt;
    }

    return loaded_preset{entry, std::move(kind), *id, index};
}

auto rush_hour_puzzle_space(const int max_blocks, const int min_blocks, const int min_moves)
    -> puzzle_space_config
{
    constexpr u8 board_width = 6;
    constexpr u8 board_height = 6;
    constexpr u8 goal_x = 4;
    constexpr u8 goal_y = 2;
    const block target = block(0, 0, 2, 1, true, false);

    puzzle_space_config config;
    config.width = board_width;
    config.height = board_height;
    config.goal_x = goal_x;
    config.goal_y = goal_y;
    config.restricted = true;
    config.inventory = {block(0, 0, 2, 1, false, false), block(0, 0, 3, 1, false, false),
                        block(0, 0, 1, 2, false, false), block(0, 0, 1, 3, false, false)};
    config.target = target;
    config.target_positions = {0, goal_y, board_width - target.get_width(), goal_y};
    config.max_blocks = static_cast<size_t>(std::max(1, max_blocks));
    config.min_blocks = static_cast<size_t>(std::max(1, min_blocks));
    config.min_moves = std::max(0, min_moves);
    return config;
}

auto klotski_puzzle_space(const int max_blocks, const int min_blocks, const int min_moves)
    -> puzzle_space_config
{
    constexpr u8 board_width = 4;
    constexpr u8 board_height = 5;
    constexpr u8 goal_x = 1;
    constexpr u8 goal_y = 3;
    const block target = block(0, 0, 2, 2, true, false);

    puzzle_space_config config;
    config.width = board_width;
    config.height = board_height;
    config.goal_x = goal_x;
    config.goal_y = goal_y;
    config.restricted = false;
    config.inventory = {
        block(0, 0, 1, 1, false, false),
        block(0, 0, 1, 2, false, false),
        block(0, 0, 2, 1, false, false),
    };
    config.target = target;
    config.target_positions = {0, 0, board_width - target.get_width(),
                               board_height - target.get_height()};
    config.max_blocks = static_cast<size_t>(std::max(1, max_blocks));
    config.min_blocks = static_cast<size_t>(std::max(1, min_blocks));
    config.min_moves = std::max(0, min_moves);
    return config;
}

auto print_puzzle_space_config(const puzzle_space_config& config) -> void
{
    int upper_set_count = 0;
    const int min_other = std::max(1, static_cast<int>(config.min_blocks) - 1);
    for (int i = min_other; i <= static_cast<int>(config.max_blocks); ++i) {
        upper_set_count += binom(static_cast<int>(config.inventory.size()) + i - 1, i);
    }

    std::cout << "Exploring puzzle space:\n";
    std::cout << std::format("  Size:       {}x{}\n", config.width, config.height);
    std::cout << std::format("  Goal:       {},{}\n", config.goal_x, config.goal_y);
    std::cout << std::format("  Restricted: {}\n", config.restricted);
    std::cout << std::format("  Min Blocks: {}\n", config.min_blocks);
    std::cout << std::format("  Max Blocks: {}\n", config.max_blocks);
    std::cout << std::format("  Min Moves:  {}\n", config.min_moves);
    std::cout << std::format("  Target:     {}x{}\n", config.target.get_width(),
                             config.target.get_height());
    std::cout << std::format("  Max Partitions (upper bound): {}\n", upper_set_count);
    if (config.wall_inventory_enabled) {
        std::cout << std::format("  Wall inventory: yes, {}..{} pieces\n", config.min_walls,
                                 config.max_walls);
        std::cout << "  Permitted wall sizes:";
        for (const block b : config.wall_inventory) {
            std::cout << std::format(" {}x{},", b.get_width(), b.get_height());
        }
        std::cout << '\n';
        std::cout << "  Wall layouts can be very numerous; choose Max Walls carefully.\n";
    } else if (config.walls != 0) {
        std::cout << std::format("  Walls:      0x{:x} (fixed)\n", config.walls);
    }
    std::cout << "  Permitted block sizes:";
    for (const block b : config.inventory) {
        std::cout << std::format(" {}x{},", b.get_width(), b.get_height());
    }
    std::cout << std::endl;
}

auto wall_options_for_board(const puzzle& board, const cli_options& options) -> wall_search_options
{
    wall_search_options wall;
    wall.inventory = options.wall_inventory;
    if (!wall.inventory) {
        return wall;
    }
    const size_t pieces = board.wall_rectangles().size();
    wall.min_walls = static_cast<size_t>(options.min_walls);
    wall.max_walls = options.max_walls < 0 ? pieces : static_cast<size_t>(options.max_walls);
    return wall;
}

class state_space_observer : public exploration_observer
{
public:
    state_space_observer() : start_(std::chrono::steady_clock::now())
    {}

    auto on_states_explored(const size_t states) -> bool override
    {
        if (g_interrupted != 0) {
            return false;
        }

        const auto now = std::chrono::steady_clock::now();
        const auto milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - start_).count();
        auto previous = last_milliseconds_.load(std::memory_order_relaxed);
        if (milliseconds - previous >= 1000 &&
            last_milliseconds_.compare_exchange_strong(previous, milliseconds)) {
            std::cout << std::format("  {} states, {:.1f}s\n", format_count(states),
                                     static_cast<double>(milliseconds) / 1000.0)
                      << std::flush;
        }
        return true;
    }

private:
    std::chrono::steady_clock::time_point start_;
    std::atomic<long long> last_milliseconds_{0};
};

auto run_puzzle_space_mode(const cli_options& options) -> int
{
    std::signal(SIGINT, cli_handle_sigint);
    std::signal(SIGTERM, cli_handle_sigint);

    puzzle_space_config config;
    if (options.space_ruleset == "rh") {
        std::cout << "Puzzle space source: Rush Hour ruleset\n";
        config = rush_hour_puzzle_space(options.max_blocks, options.min_blocks, options.min_moves);
    } else if (options.space_ruleset == "klotski") {
        std::cout << "Puzzle space source: Klotski ruleset\n";
        config = klotski_puzzle_space(options.max_blocks, options.min_blocks, options.min_moves);
    } else {
        std::optional<loaded_preset> loaded = load_preset(options, "sliding_block");
        if (!loaded) {
            return 1;
        }

        const auto* sliding = dynamic_cast<const sliding_block_model*>(loaded->kind->model.get());
        if (sliding == nullptr) {
            errln("Puzzle-space search is only available for sliding-block puzzles.");
            return 1;
        }

        const puzzle& board = sliding->get(loaded->id);
        const puzzle_space_config_result built = make_puzzle_space_config(
            board, static_cast<size_t>(std::max(0, options.max_blocks)), options.min_moves,
            static_cast<size_t>(std::max(0, options.min_blocks)),
            wall_options_for_board(board, options));
        if (!built.config) {
            errln("{}", built.error);
            return 1;
        }

        std::cout << std::format("Puzzle space source: preset {} \"{}\" ({})\n", loaded->index,
                                 loaded->entry.comment, loaded->entry.type);
        config = *built.config;
    }

    if (config.min_blocks > config.max_blocks) {
        errln("Minimum blocks ({}) cannot exceed maximum blocks ({}).", config.min_blocks,
              config.max_blocks);
        return 1;
    }

    print_puzzle_space_config(config);

    search_thread_pool pool(options.threads);
    std::cout << std::format("Search threads: {}\n", pool.count());

    puzzle_space_progress progress;
    std::stop_source stop;
    const auto search_start = std::chrono::steady_clock::now();

    std::jthread monitor(
        [&progress, &stop, search_start](const std::stop_token token)
        {
            auto last = std::chrono::steady_clock::now();
            while (!token.stop_requested()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if (token.stop_requested()) {
                    break;
                }
                if (g_interrupted != 0) {
                    stop.request_stop();
                }

                const auto now = std::chrono::steady_clock::now();
                if (now - last < std::chrono::seconds(1)) {
                    continue;
                }
                last = now;

                const double elapsed = std::chrono::duration<double>(now - search_start).count();
                const auto phase = progress.phase.load(std::memory_order_relaxed);
                if (phase == puzzle_space_phase::counting) {
                    const size_t partitions = progress.inventories.load(std::memory_order_relaxed);
                    const size_t counted =
                        progress.counted_partitions.load(std::memory_order_relaxed);
                    const size_t placements =
                        progress.counted_placements.load(std::memory_order_relaxed);
                    std::cout << std::format(
                                     "[{:6.1f}s] counting: partitions {}/{} placements {}\n",
                                     elapsed, format_count(counted), format_count(partitions),
                                     format_count(placements))
                              << std::flush;
                    continue;
                }

                const size_t done = progress.candidates_done.load(std::memory_order_relaxed);
                const size_t total = progress.candidates_total.load(std::memory_order_relaxed);
                const size_t clusters = progress.clusters_found.load(std::memory_order_relaxed);
                const auto eta = progress.eta_seconds.load(std::memory_order_relaxed);

                std::cout << std::format("[{:6.1f}s] {}: placements {}/{} clusters {} eta {}\n",
                                         elapsed, phase_name(phase), format_count(done),
                                         format_count(total), format_count(clusters),
                                         eta >= 0 ? format_duration(static_cast<double>(eta))
                                                  : "--")
                          << std::flush;
            }
        });

    puzzle_space_result result =
        run_puzzle_space_search(config, &progress, stop.get_token(), pool.get());

    monitor.request_stop();
    monitor.join();

    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - search_start).count();

    if (result.cancelled) {
        std::cout << std::format("Puzzle-space search cancelled after {:.2f}s.\n", elapsed);
        return 130;
    }
    if (!result.error.empty()) {
        errln("Puzzle-space search failed: {}", result.error);
        return 1;
    }

    const std::optional<std::string> save_error =
        save_puzzle_space_file(options.output_file, result);
    if (save_error) {
        errln("{}", *save_error);
        return 1;
    }

    std::cout << std::format(
        "Puzzle space: {} clusters, {} placements, {:.2f}s, saved to \"{}\".\n",
        format_count(result.clusters.size()), format_count(progress.candidates_done.load()),
        elapsed, options.output_file);
    return 0;
}

auto run_state_space_mode(const cli_options& options) -> int
{
    std::signal(SIGINT, cli_handle_sigint);
    std::signal(SIGTERM, cli_handle_sigint);

    std::optional<loaded_preset> loaded = load_preset(options, options.puzzle_type);
    if (!loaded) {
        return 1;
    }

    search_thread_pool pool(options.threads);
    std::cout << std::format("State space: type={} preset={} \"{}\" threads={}\n",
                             loaded->entry.type, loaded->index, loaded->entry.comment,
                             pool.count());

    state_space_observer observer;
    const auto start = std::chrono::steady_clock::now();
    exploration_result result =
        loaded->kind->model->explore(loaded->id, loaded->id, pool.get(), &observer);
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    const size_t states = loaded->kind->model->state_count();
    const double rate = elapsed > 0.0 ? static_cast<double>(states) / elapsed : 0.0;

    std::cout << std::format(
        "Result: states={} links={} complete={} cancelled={} directed={} time={:.3f}s "
        "rate={:.0f} states/s memory={}\n",
        format_count(states), format_count(result.links.size()), result.complete, result.cancelled,
        result.directed, elapsed, rate, format_memory(loaded->kind->model->memory_bytes()));

    return result.cancelled ? 130 : 0;
}
} // namespace

auto parse_cli(const int argc, char* argv[]) -> cli_options
{
    cli_options options;

#if not defined(_WIN32)
    po::options_description desc("Allowed options");
    desc.add_options()                     //
        ("help,h", "produce help message") //
        ("presets", po::value<std::string>(&options.preset_file),
         "load presets from file (default: default.puzzle)") //
        ("preset", po::value<int>(&options.preset_index)->value_name("INDEX"),
         "use preset INDEX (0-based) for the GUI or a headless mode") //
        ("puzzle", po::value<std::string>(&options.puzzle_type)->value_name("TYPE"),
         "start with puzzle TYPE (sliding_block, lights_out, hanoi, eight_puzzle or sudoku4)") //
        ("populate", po::bool_switch(&options.populate),
         "populate the graph for the selected preset at startup") //
        ("space", po::value<std::string>(&options.space_ruleset)->value_name("preset|rh|klotski"),
         "generate a puzzle space headlessly (preset uses --presets/--preset)") //
        ("state-space", po::bool_switch(&options.state_space),
         "explore a preset's state space headlessly and print benchmark stats") //
        ("output", po::value<std::string>(&options.output_file),
         "output file for generated puzzle spaces (default: puzzle_space.puzzles)") //
        ("blocks", po::value<int>(&options.max_blocks)->value_name("N"),
         "block limit for puzzle-space generation (default: 5; 0 = all movable blocks)") //
        ("min-blocks", po::value<int>(&options.min_blocks)->value_name("N"),
         "minimum blocks per generated puzzle, target included (default: 2)") //
        ("wall-inventory", po::bool_switch(&options.wall_inventory),
         "treat the current board's wall shapes as an inventory") //
        ("min-walls", po::value<int>(&options.min_walls)->value_name("N"),
         "minimum wall pieces for a wall inventory (default: 0)") //
        ("max-walls", po::value<int>(&options.max_walls)->value_name("N"),
         "maximum wall pieces for a wall inventory (default: current piece count)") //
        ("moves", po::value<int>(&options.min_moves),
         "only save puzzles with at least this many required moves") //
        ("threads", po::value<int>(&options.threads)->value_name("N"),
         "search threads (default: settings search_threads; 0 = auto)") //
        ;

    po::positional_options_description positional;
    positional.add("presets", 1);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(desc).positional(positional).run(), vm);
    po::notify(vm);

    if (vm.count("help") != 0) {
        std::cout << desc << std::endl;
        options.mode = cli_mode::exit;
        return options;
    }

    options.preset_given = vm.count("preset") != 0;
    options.space_given = vm.count("space") != 0;
    options.min_walls_given = vm.count("min-walls") != 0;
    options.max_walls_given = vm.count("max-walls") != 0;
    options.max_blocks = std::clamp(options.max_blocks, 0, static_cast<int>(puzzle::MAX_BLOCKS));
    options.min_blocks = std::clamp(options.min_blocks, 1, static_cast<int>(puzzle::MAX_BLOCKS));
    options.min_moves = std::max(0, options.min_moves);
    options.threads = std::max(0, options.threads);
    options.min_walls = std::max(0, options.min_walls);
    if (options.max_walls >= 0) {
        options.max_walls = std::max(0, options.max_walls);
    }

    if (options.space_given && options.state_space) {
        errln("--space and --state-space cannot be combined.");
        options.mode = cli_mode::exit;
        options.exit_code = 1;
        return options;
    }

    if ((options.min_walls_given || options.max_walls_given) && !options.wall_inventory) {
        errln("--min-walls and --max-walls require --wall-inventory.");
        options.mode = cli_mode::exit;
        options.exit_code = 1;
        return options;
    }

    if (options.wall_inventory && !options.space_given) {
        errln("--wall-inventory requires --space.");
        options.mode = cli_mode::exit;
        options.exit_code = 1;
        return options;
    }

    if (options.space_given) {
        if (options.space_ruleset != "preset" && options.space_ruleset != "rh" &&
            options.space_ruleset != "klotski") {
            errln("Unknown --space value \"{}\"; expected preset, rh or klotski.",
                  options.space_ruleset);
            options.mode = cli_mode::exit;
            options.exit_code = 1;
            return options;
        }
        if (options.wall_inventory && options.space_ruleset != "preset") {
            errln("--wall-inventory requires --space preset; the built-in rulesets have no walls.");
            options.mode = cli_mode::exit;
            options.exit_code = 1;
            return options;
        }
        if (!options.preset_given) {
            options.preset_index = 0;
        }
        options.mode = cli_mode::puzzle_space;
        return options;
    }

    if (options.state_space) {
        if (!options.preset_given) {
            options.preset_index = 0;
        }
        options.mode = cli_mode::state_space;
        return options;
    }

    options.mode = cli_mode::gui;
#endif

    return options;
}

auto run_headless(const cli_options& options) -> int
{
    switch (options.mode) {
    case cli_mode::puzzle_space:
        return run_puzzle_space_mode(options);
    case cli_mode::state_space:
        return run_state_space_mode(options);
    case cli_mode::gui:
    case cli_mode::exit:
        break;
    }
    return 0;
}
