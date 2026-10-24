#ifndef CLI_HPP_
#define CLI_HPP_

#include <string>

enum class cli_mode
{
    gui,
    puzzle_space,
    state_space,
    exit,
};

struct cli_options
{
    cli_mode mode = cli_mode::gui;

    std::string preset_file = "default.puzzle";
    std::string output_file = "puzzle_space.puzzles";
    int preset_index = -1;
    std::string puzzle_type;
    bool populate = false;

    std::string space_ruleset = "preset";
    bool state_space = false;
    int max_blocks = 5;
    int min_blocks = 2;
    int min_moves = 10;
    int threads = 0;

    bool wall_inventory = false;
    int min_walls = 0;
    int max_walls = -1;
    bool min_walls_given = false;
    bool max_walls_given = false;

    bool preset_given = false;
    bool space_given = false;
    int exit_code = 0;
};

auto parse_cli(int argc, char* argv[]) -> cli_options;
auto run_headless(const cli_options& options) -> int;

#endif
