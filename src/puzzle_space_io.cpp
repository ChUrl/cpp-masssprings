#include "puzzle_space_io.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <utility>

namespace
{
auto trim_cr(std::string& line) -> void
{
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
}

auto read_non_empty_line(std::istream& input, std::string& line) -> bool
{
    while (std::getline(input, line)) {
        trim_cr(line);
        if (!line.empty()) {
            return true;
        }
    }
    return false;
}

auto parse_cluster(const std::string& path, std::istream& input, puzzle_space_result& result,
                   std::string& error) -> bool
{
    std::string line;
    if (!read_non_empty_line(input, line)) {
        error = "Unexpected end of file while reading a cluster header";
        return false;
    }

    std::istringstream header(line);
    std::string tag;
    puzzle_space_cluster cluster;
    header >> tag;
    if (tag != "CLUSTER" ||
        !(header >> cluster.max_moves >> cluster.state_count >> cluster.block_count)) {
        error = "Invalid cluster header in " + path;
        return false;
    }
    // WALL_COUNT was appended for wall-inventory results; older files do not have it.
    size_t wall_count = 0;
    if (header >> wall_count) {
        cluster.wall_count = wall_count;
    }

    if (!read_non_empty_line(input, line) || !line.starts_with("ID ")) {
        error = "Missing cluster ID in " + path;
        return false;
    }

    try {
        cluster.cluster_id = puzzle(line.substr(3));
    }
    catch (const std::exception& exception) {
        error = "Invalid cluster ID: " + std::string(exception.what());
        return false;
    }
    if (cluster.cluster_id.try_get_invalid_reason()) {
        error = "Invalid cluster ID state in " + path;
        return false;
    }

    if (!read_non_empty_line(input, line) || !line.starts_with("HARD ")) {
        error = "Missing hardest state in " + path;
        return false;
    }

    try {
        cluster.hardest = puzzle(line.substr(5));
    }
    catch (const std::exception& exception) {
        error = "Invalid hardest state: " + std::string(exception.what());
        return false;
    }
    if (cluster.hardest.try_get_invalid_reason()) {
        error = "Invalid hardest state in " + path;
        return false;
    }

    result.clusters.push_back(cluster);
    return true;
}
} // namespace

auto save_puzzle_space_file(const std::string& path, const puzzle_space_result& result)
    -> std::optional<std::string>
{
    if (path.empty()) {
        return "Filename is empty";
    }

    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file) {
        return "Could not open \"" + path + "\" for writing";
    }

    file << "MSPACE 1\n";
    file << "CONFIG " << static_cast<int>(result.config.width) << ' '
         << static_cast<int>(result.config.height) << ' ' << static_cast<int>(result.config.goal_x)
         << ' ' << static_cast<int>(result.config.goal_y) << ' '
         << (result.config.restricted ? 1 : 0) << ' ' << result.config.max_blocks << ' '
         << result.config.min_moves << ' ' << result.config.min_blocks << '\n';
    file << "TARGET " << static_cast<int>(result.config.target.get_width()) << ' '
         << static_cast<int>(result.config.target.get_height()) << '\n';
    file << "WALLS " << std::hex << static_cast<unsigned long long>(result.config.walls) << std::dec
         << '\n';
    if (result.config.wall_inventory_enabled) {
        std::vector<std::pair<int, int>> wall_shapes;
        wall_shapes.reserve(result.config.wall_inventory.size());
        for (const block shape : result.config.wall_inventory) {
            wall_shapes.emplace_back(shape.get_width(), shape.get_height());
        }
        std::ranges::sort(wall_shapes);
        file << "WALL_INVENTORY " << wall_shapes.size() << ' ' << result.config.min_walls << ' '
             << result.config.max_walls << '\n';
        for (const auto& [width, height] : wall_shapes) {
            file << width << ' ' << height << '\n';
        }
    }

    const auto [x0, y0, x1, y1] = result.config.target_positions;
    file << "RANGE " << static_cast<int>(x0) << ' ' << static_cast<int>(y0) << ' '
         << static_cast<int>(x1) << ' ' << static_cast<int>(y1) << '\n';

    std::vector<std::pair<int, int>> shapes;
    shapes.reserve(result.config.inventory.size());
    for (const block shape : result.config.inventory) {
        shapes.emplace_back(shape.get_width(), shape.get_height());
    }
    std::ranges::sort(shapes);

    file << "INVENTORY " << shapes.size() << '\n';
    for (const auto& [width, height] : shapes) {
        file << width << ' ' << height << '\n';
    }

    file << "CLUSTERS " << result.clusters.size() << '\n';
    for (const puzzle_space_cluster& cluster : result.clusters) {
        file << "CLUSTER " << cluster.max_moves << ' ' << cluster.state_count << ' '
             << cluster.block_count << ' ' << cluster.wall_count << '\n';
        file << "ID " << cluster.cluster_id.string_repr() << '\n';
        file << "HARD " << cluster.hardest.string_repr() << '\n';
    }

    if (!file) {
        return "Failed while writing \"" + path + "\"";
    }
    return std::nullopt;
}

auto load_puzzle_space_file(const std::string& path) -> puzzle_space_io_result
{
    puzzle_space_io_result io;
    if (path.empty()) {
        io.error = "Filename is empty";
        return io;
    }

    std::ifstream file(path, std::ios::in);
    if (!file) {
        io.error = "Could not open \"" + path + "\" for reading";
        return io;
    }

    std::string line;
    if (!read_non_empty_line(file, line)) {
        io.error = "Empty puzzle-space file";
        return io;
    }

    puzzle_space_result result;
    {
        std::istringstream header(line);
        std::string tag;
        int version = 0;
        if (!(header >> tag >> version) || tag != "MSPACE" || version != 1) {
            io.error = "Unsupported puzzle-space file format";
            return io;
        }
    }

    if (!read_non_empty_line(file, line)) {
        io.error = "Missing configuration";
        return io;
    }
    {
        std::istringstream config_line(line);
        std::string tag;
        int width = 0;
        int height = 0;
        int goal_x = 0;
        int goal_y = 0;
        int restricted = 0;
        size_t max_blocks = 0;
        int min_moves = 0;
        size_t min_blocks = 2;
        if (!(config_line >> tag >> width >> height >> goal_x >> goal_y >> restricted >>
              max_blocks >> min_moves) ||
            tag != "CONFIG" || width < 3 || width > puzzle::MAX_WIDTH || height < 3 ||
            height > puzzle::MAX_HEIGHT) {
            io.error = "Invalid CONFIG line";
            return io;
        }
        // MIN_BLOCKS was appended to the CONFIG line; older files do not have
        // it and keep the historical lower bound.
        if (!(config_line >> min_blocks)) {
            min_blocks = max_blocks == 0 ? 2 : std::min<size_t>(2, max_blocks);
        }
        if (min_blocks < 1 || (max_blocks != 0 && min_blocks > max_blocks)) {
            io.error = "Invalid block range in CONFIG line";
            return io;
        }
        result.config.width = static_cast<u8>(width);
        result.config.height = static_cast<u8>(height);
        result.config.goal_x = static_cast<u8>(goal_x);
        result.config.goal_y = static_cast<u8>(goal_y);
        result.config.restricted = restricted != 0;
        result.config.max_blocks = max_blocks;
        result.config.min_blocks = min_blocks;
        result.config.min_moves = min_moves;
    }

    if (!read_non_empty_line(file, line)) {
        io.error = "Missing target block";
        return io;
    }
    {
        std::istringstream target_line(line);
        std::string tag;
        int width = 0;
        int height = 0;
        if (!(target_line >> tag >> width >> height) || tag != "TARGET" || width < 1 ||
            width > puzzle::MAX_WIDTH || height < 1 || height > puzzle::MAX_HEIGHT) {
            io.error = "Invalid TARGET line";
            return io;
        }
        result.config.target = block(0, 0, width, height, true, false);
    }

    if (!read_non_empty_line(file, line)) {
        io.error = "Missing target-position range";
        return io;
    }

    // WALLS is optional so older result files (which had no wall support) still load.
    if (line.starts_with("WALLS ")) {
        std::istringstream walls_line(line);
        std::string tag;
        unsigned long long walls = 0;
        if (!(walls_line >> tag >> std::hex >> walls >> std::dec) || tag != "WALLS") {
            io.error = "Invalid WALLS line";
            return io;
        }
        result.config.walls = static_cast<u64>(walls);
        const int cells = static_cast<int>(result.config.width) * result.config.height;
        const u64 board_mask = cells >= 64 ? ~u64{0} : ((u64{1} << cells) - 1);
        if ((result.config.walls & ~board_mask) != 0) {
            io.error = "Walls outside the board in WALLS line";
            return io;
        }
        if (!read_non_empty_line(file, line)) {
            io.error = "Missing target-position range";
            return io;
        }
    }

    if (line.starts_with("WALL_INVENTORY ")) {
        std::istringstream inventory_line(line);
        std::string tag;
        size_t count = 0;
        size_t min_walls = 0;
        size_t max_walls = 0;
        if (!(inventory_line >> tag >> count >> min_walls >> max_walls) ||
            tag != "WALL_INVENTORY" || min_walls > max_walls) {
            io.error = "Invalid WALL_INVENTORY line";
            return io;
        }
        result.config.wall_inventory_enabled = true;
        result.config.min_walls = min_walls;
        result.config.max_walls = max_walls;
        for (size_t i = 0; i < count; ++i) {
            if (!read_non_empty_line(file, line)) {
                io.error = "Unexpected end of file in wall inventory";
                return io;
            }
            std::istringstream shape_line(line);
            int width = 0;
            int height = 0;
            if (!(shape_line >> width >> height) || width < 1 || width > puzzle::MAX_WIDTH ||
                height < 1 || height > puzzle::MAX_HEIGHT) {
                io.error = "Invalid wall inventory shape";
                return io;
            }
            result.config.wall_inventory.insert(block(0, 0, width, height, false, false));
        }
        if (!read_non_empty_line(file, line)) {
            io.error = "Missing target-position range";
            return io;
        }
    }

    {
        std::istringstream range_line(line);
        std::string tag;
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        if (!(range_line >> tag >> x0 >> y0 >> x1 >> y1) || tag != "RANGE") {
            io.error = "Invalid RANGE line";
            return io;
        }
        result.config.target_positions = {static_cast<u8>(x0), static_cast<u8>(y0),
                                          static_cast<u8>(x1), static_cast<u8>(y1)};
    }

    if (!read_non_empty_line(file, line)) {
        io.error = "Missing inventory";
        return io;
    }
    {
        std::istringstream inventory_line(line);
        std::string tag;
        size_t count = 0;
        if (!(inventory_line >> tag >> count) || tag != "INVENTORY") {
            io.error = "Invalid INVENTORY line";
            return io;
        }
        for (size_t i = 0; i < count; ++i) {
            if (!read_non_empty_line(file, line)) {
                io.error = "Unexpected end of file in inventory";
                return io;
            }
            std::istringstream shape_line(line);
            int width = 0;
            int height = 0;
            if (!(shape_line >> width >> height) || width < 1 || width > puzzle::MAX_WIDTH ||
                height < 1 || height > puzzle::MAX_HEIGHT) {
                io.error = "Invalid inventory shape";
                return io;
            }
            result.config.inventory.insert(block(0, 0, width, height, false, false));
        }
    }

    if (!read_non_empty_line(file, line)) {
        io.error = "Missing cluster count";
        return io;
    }
    size_t cluster_count = 0;
    {
        std::istringstream clusters_line(line);
        std::string tag;
        if (!(clusters_line >> tag >> cluster_count) || tag != "CLUSTERS") {
            io.error = "Invalid CLUSTERS line";
            return io;
        }
    }

    result.clusters.reserve(cluster_count);
    for (size_t i = 0; i < cluster_count; ++i) {
        if (!parse_cluster(path, file, result, io.error)) {
            return io;
        }
    }

    for (puzzle_space_cluster& cluster : result.clusters) {
        if (cluster.cluster_id.wall_mask() != cluster.hardest.wall_mask()) {
            io.error = "Cluster ID and hardest state have different wall masks";
            return io;
        }
        if (!result.config.wall_inventory_enabled) {
            if (cluster.cluster_id.wall_mask() != result.config.walls) {
                io.error = "Cluster wall mask does not match the WALLS line";
                return io;
            }
        } else {
            if (cluster.wall_count == 0) {
                cluster.wall_count = cluster.cluster_id.wall_rectangles().size();
            }
            if (cluster.wall_count < result.config.min_walls ||
                cluster.wall_count > result.config.max_walls) {
                io.error = "Cluster wall count is outside the wall inventory range";
                return io;
            }
        }
    }

    io.result = std::move(result);
    return io;
}
