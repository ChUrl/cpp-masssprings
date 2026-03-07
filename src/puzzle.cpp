#include "puzzle.hpp"

#include "graph_distances.hpp"

#include <algorithm>
#include <set>
#include <boost/program_options/detail/cmdline.hpp>
#include <boost/unordered/unordered_flat_map.hpp>

auto block::create_repr(const u8 x, const u8 y, const u8 w, const u8 h, const bool t, const bool i) -> u16
{
    return block().set_x(x).set_y(y).set_width(w).set_height(h).set_target(t).set_immovable(i).repr & ~INVALID;
}

auto block::unpack_repr() const -> std::tuple<u8, u8, u8, u8, bool, bool>
{
    const u8 x = get_x();
    const u8 y = get_y();
    const u8 w = get_width();
    const u8 h = get_height();
    const bool t = get_target();
    const bool i = get_immovable();

    return {x, y, w, h, t, i};
}

auto block::hash() const -> size_t
{
    return std::hash<u16>{}(repr);
}

auto block::position_independent_hash() const -> size_t
{
    u16 r = repr;
    clear_bits(r, X_S, X_E);
    clear_bits(r, Y_S, Y_E);
    return std::hash<u16>{}(r);
}

auto block::valid() const -> bool
{
    // This means the first bit is set, marking the block as empty
    if (repr & INVALID) {
        return false;
    }

    const auto [x, y, w, h, t, i] = unpack_repr();

    if (t && i) {
        return false;
    }

    return w > 0 && h > 0 && x + w <= MAX_WIDTH && y + h <= MAX_HEIGHT;
}

auto block::principal_dirs() const -> u8
{
    const auto [x, y, w, h, t, i] = unpack_repr();

    if (i) {
        return 0;
    }

    if (w > h) {
        return eas | wes;
    }
    if (h > w) {
        return nor | sou;
    }
    return nor | eas | sou | wes;
}

auto block::covers(const int _x, const int _y) const -> bool
{
    const auto [x, y, w, h, t, i] = unpack_repr();

    return _x >= x && _x < x + w && _y >= y && _y < y + h;
}

auto block::collides(const block b) const -> bool
{
    const auto [x, y, w, h, t, i] = unpack_repr();
    const auto [bx, by, bw, bh, bt, bi] = b.unpack_repr();

    return x < bx + bw && x + w > bx && y < by + bh && y + h > by;
}

auto puzzle::create_meta(const std::tuple<u8, u8, u8, u8, bool, bool>& meta) -> u16
{
    const auto [w, h, gx, gy, r, g] = meta;

    u16 m = 0;
    set_bits(m, WIDTH_S, WIDTH_E, w - 1u);
    set_bits(m, HEIGHT_S, HEIGHT_E, h - 1u);
    set_bits(m, GOAL_X_S, GOAL_X_E, gx);
    set_bits(m, GOAL_Y_S, GOAL_Y_E, gy);
    set_bits(m, RESTRICTED_S, RESTRICTED_E, r);
    set_bits(m, GOAL_S, GOAL_E, g);
    return m;
}

auto puzzle::create_repr(const u8 w,
                         const u8 h,
                         const u8 tx,
                         const u8 ty,
                         const bool r,
                         const bool g,
                         const std::array<u16, MAX_BLOCKS>& b) -> repr_cooked
{
    repr_cooked repr = puzzle().set_width(w).set_height(h).set_goal_x(tx).set_goal_y(ty).set_restricted(r).set_goal(g).
                                set_blocks(b).repr.cooked;
    repr.meta &= ~INVALID;
    return repr;
}

auto puzzle::create_repr(const u64 byte_0, const u64 byte_1, const u64 byte_2, const u64 byte_3) -> repr_cooked
{
    repr_u repr{};
    repr.raw = std::array<u64, 4>{byte_0, byte_1, byte_2, byte_3};

    return repr.cooked;
}

auto puzzle::create_repr(const std::string& string_repr) -> repr_cooked
{
    const std::optional<repr_cooked> repr = try_parse_string_repr(string_repr);
    if (!repr) {
        throw std::invalid_argument("Failed to parse string repr");
    }

    return *repr;
}

auto puzzle::set_blocks(std::array<u16, MAX_BLOCKS> blocks) const -> puzzle
{
    puzzle p = *this;
    std::ranges::sort(blocks);
    p.repr.cooked.blocks = blocks;
    return p;
}

auto puzzle::unpack_meta() const -> std::tuple<u8, u8, u8, u8, bool, bool>
{
    const u8 w = get_width();
    const u8 h = get_height();
    const u8 tx = get_goal_x();
    const u8 ty = get_goal_y();
    const bool r = get_restricted();
    const bool g = get_goal();

    return {w, h, tx, ty, r, g};
}

auto puzzle::hash() const -> size_t
{
    size_t h = 0;
    for (size_t i = 0; i < 4; ++i) {
        hash_combine(h, repr.raw[i]);
    }
    return h;
}

auto puzzle::string_repr() const -> std::string
{
    // S:[3x3] G:[1,1] M:[R] B:[{1x1 _ _} {2X1 _ _} {_ _ _}]
    const auto [w, h, gx, gy, r, g] = unpack_meta();

    std::vector<block> linear_blocks(w * h);
    for (const block b : block_view()) {
        linear_blocks[b.get_y() * w + b.get_x()] = b;
    }

    std::string size = std::format("S:[{}x{}] ", w, h);
    std::string goal = g ? std::format("G:[{},{}] ", gx, gy) : "";
    std::string restricted = std::format("M:[{}] ", r ? "R" : "F");

    std::string blocks = "B:[";
    for (int y = 0; y < h; ++y) {
        blocks += '{';
        for (int x = 0; x < w; ++x) {
            const block b = block(linear_blocks[y * w + x]);
            if (!b.valid()) {
                blocks += "_ ";
            } else if (b.get_target()) {
                blocks += std::format("{}X{} ", b.get_width(), b.get_height());
            } else if (b.get_immovable()) {
                blocks += std::format("{}*{} ", b.get_width(), b.get_height());
            } else {
                blocks += std::format("{}x{} ", b.get_width(), b.get_height());
            }
        }
        blocks.pop_back(); // Remove last extra space before }
        blocks += "} ";
    }
    blocks.pop_back(); // Remove last extra space before ]
    blocks += ']';

    return std::format("{}{}{}{}", size, goal, restricted, blocks);
}

auto puzzle::try_parse_string_repr(const std::string& string_repr) -> std::optional<repr_cooked>
{
    bool parsed_size = false;
    std::pair<u8, u8> size{0, 0};

    bool parsed_goal = false;
    std::pair<u8, u8> goal{0, 0};

    bool parsed_restricted = false;
    bool restricted = true;

    bool parsed_blocks = false;
    std::vector<u16> bs;
    std::array<u16, MAX_BLOCKS> blocks = invalid_blocks();

    const auto digit = [&](const char c)
    {
        return std::string("0123456789").contains(c);
    };

    // S:[3x3]
    const auto parse_size = [&](size_t& pos)
    {
        u8 w = std::stoi(string_repr.substr(pos + 3, 1));
        u8 h = std::stoi(string_repr.substr(pos + 5, 1));
        size = std::make_pair(w, h);

        parsed_size = true;
        pos += 6;
    };

    // G:[1,1] (optional)
    const auto parse_goal = [&](size_t& pos)
    {
        u8 gx = std::stoi(string_repr.substr(pos + 3, 1));
        u8 gy = std::stoi(string_repr.substr(pos + 5, 1));
        goal = std::make_pair(gx, gy);

        parsed_goal = true;
        pos += 6;
    };

    // M:[R]
    const auto parse_restricted = [&](size_t& pos)
    {
        if (string_repr[pos + 3] == 'R') {
            restricted = true;
            parsed_restricted = true;
        }
        if (string_repr[pos + 3] == 'F') {
            restricted = false;
            parsed_restricted = true;
        }

        pos += 4;
    };

    // 1x1 or 1X1 or 1*1 or _
    const auto parse_block = [&](size_t& pos, const u8 x, const u8 y)
    {
        if (string_repr[pos] == '_') {
            return block();
        }

        const u8 w = std::stoi(string_repr.substr(pos, 1));
        const u8 h = std::stoi(string_repr.substr(pos + 2, 1));
        const bool t = string_repr[pos + 1] == 'X';
        const bool i = string_repr[pos + 1] == '*';

        pos += 2;
        return block(x, y, w, h, t, i);
    };

    // {1x1 _ _}
    const auto parse_row = [&](size_t& pos, const u8 y)
    {
        std::vector<u16> row;

        u8 x = 0;
        ++pos; // Skip {
        while (string_repr[pos] != '}') {
            if (digit(string_repr[pos]) || string_repr[pos] == '_') {
                const block b = parse_block(pos, x, y);
                if (b.valid()) {
                    row.emplace_back(b.repr);
                }
                ++x;
            }
            ++pos;
        }

        return row;
    };

    // B:[{1x1 _ _} {2X1 _ _} {_ _ _}]
    const auto parse_blocks = [&](size_t& pos)
    {
        std::vector<u16> rows;

        u8 y = 0;
        ++pos; // Skip [
        while (string_repr[pos] != ']') {
            if (string_repr[pos] == '{') {
                std::vector<u16> row = parse_row(pos, y);
                rows.insert(rows.end(), row.begin(), row.end());
                ++y;
            }
            ++pos;
        }

        parsed_blocks = true;
        return rows;
    };

    /*
     * User-readable string representation (3x3 example):
     *
     * S:[3x3] G:[1,1] M:[R] B:[{1x1 _ _} {2X1 _ _} {_ _ _}]
     */
    for (size_t pos = 0; pos < string_repr.size(); ++pos) {
        switch (string_repr[pos]) {
        case 'S':
            if (parsed_size) {
                warnln("Parsed duplicate attribute");
                return std::nullopt;
            }
            parse_size(pos);
            break;
        case 'G':
            if (parsed_goal) {
                warnln("Parsed duplicate attribute");
                return std::nullopt;
            }
            parse_goal(pos);
            break;
        case 'M':
            if (parsed_restricted) {
                warnln("Parsed duplicate attribute");
                return std::nullopt;
            }
            parse_restricted(pos);
            break;
        case 'B':
            if (parsed_blocks) {
                warnln("Parsed duplicate attribute");
                return std::nullopt;
            }
            bs = parse_blocks(pos);

            if (bs.size() > MAX_BLOCKS) {
                warnln("Parsed too many blocks");
                return std::nullopt;
            }

            std::copy_n(bs.begin(), bs.size(), blocks.begin());
            break;
        default:
            break;
        }
    }

    if (!parsed_size || !parsed_restricted || !parsed_blocks) {
        warnln("Failed to parse required attribute");
        return std::nullopt;
    }

    return create_repr(size.first, size.second, goal.first, goal.second, restricted, parsed_goal, blocks);
}

auto puzzle::valid() const -> bool
{
    if (repr.cooked.meta & INVALID) {
        return false;
    }

    const auto [w, h, gx, gy, r, g] = unpack_meta();

    return w >= MIN_WIDTH && w <= MAX_WIDTH && h >= MIN_HEIGHT && h <= MAX_HEIGHT;
}

auto puzzle::try_get_invalid_reason() const -> std::optional<std::string>
{
    if (repr.cooked.meta & INVALID) {
        return "Flagged as Invalid";
    }

    const auto [w, h, gx, gy, r, g] = unpack_meta();

    // traceln("Validating puzzle \"{}\"", string_repr());

    const std::optional<block>& b = try_get_target_block();
    if (get_goal() && !b) {
        return "Goal Without Target";
    }
    if (!get_goal() && b) {
        return "Target Without Goal";
    }

    if (get_goal() && b && r) {
        const int dirs = b->principal_dirs();
        if ((dirs & nor && b->get_x() != gx) || (dirs & eas && b->get_y() != gy)) {
            return "Goal Unreachable";
        }
    }

    if (get_goal() && b && gx > 0 && gx + b->get_width() < w && gy > 0 && gy + b->get_height() < h) {
        return "Goal Inside";
    }

    if (!valid()) {
        return "Invalid Dims";
    }

    return std::nullopt;
}

auto puzzle::block_count() const -> u8
{
    u8 count = 0;
    for (const u16 b : repr.cooked.blocks) {
        if (block(b).valid()) {
            ++count;
        }
    }
    return count;
}

auto puzzle::goal_reached() const -> bool
{
    const std::optional<block> b = try_get_target_block();
    return get_goal() && b && b->get_x() == get_goal_x() && b->get_y() == get_goal_y();
}

auto puzzle::try_get_block(const u8 x, const u8 y) const -> std::optional<block>
{
    if (!covers(x, y)) {
        return std::nullopt;
    }

    for (const block b : block_view()) {
        if (b.covers(x, y)) {
            return b;
        }
    }

    return std::nullopt;
}

auto puzzle::try_get_target_block() const -> std::optional<block>
{
    for (const block b : block_view()) {
        if (b.get_target()) {
            return b;
        }
    }

    return std::nullopt;
}

auto puzzle::covers(const u8 x, const u8 y, const u8 _w, const u8 _h) const -> bool
{
    return x + _w <= get_width() && y + _h <= get_height();
}

auto puzzle::covers(const u8 x, const u8 y) const -> bool
{
    return covers(x, y, 1, 1);
}

auto puzzle::covers(const block b) const -> bool
{
    return covers(b.get_x(), b.get_y(), b.get_width(), b.get_height());
}

auto puzzle::toggle_restricted() const -> puzzle
{
    return set_restricted(!get_restricted());
}

auto puzzle::try_set_goal(const u8 x, const u8 y) const -> std::optional<puzzle>
{
    const std::optional<block>& b = try_get_target_block();
    if (!b || !covers(x, y, b->get_width(), b->get_height())) {
        return std::nullopt;
    }

    if (get_goal_x() == x && get_goal_y() == y) {
        return clear_goal();
    }

    return set_goal_x(x).set_goal_y(y).set_goal(true);
}

auto puzzle::clear_goal() const -> puzzle
{
    return set_goal_x(0).set_goal_y(0).set_goal(false);
}

auto puzzle::try_add_column() const -> std::optional<puzzle>
{
    const u8 w = get_width();
    if (w >= MAX_WIDTH) {
        return std::nullopt;
    }

    return set_width(w + 1);
}

auto puzzle::try_remove_column() const -> std::optional<puzzle>
{
    const auto [w, h, gx, gy, r, g] = unpack_meta();
    if (w <= MIN_WIDTH) {
        return std::nullopt;
    }

    puzzle p{static_cast<u8>(w - 1), h, 0, 0, r, g};

    // Re-add all the blocks, blocks no longer fitting won't be added
    for (const block b : block_view()) {
        if (const std::optional<puzzle>& _p = p.try_add_block(b)) {
            p = *_p;
        }
    }

    const std::optional<block> b = p.try_get_target_block();
    if (p.get_goal() && b && !p.covers(gx, gy, b->get_width(), b->get_height())) {
        // Target no longer inside board
        return p.clear_goal();
    }
    if (p.get_goal() && !b) {
        // Target block removed during resize
        return p.clear_goal();
    }

    return p;
}

auto puzzle::try_add_row() const -> std::optional<puzzle>
{
    const u8 h = get_height();
    if (h >= MAX_HEIGHT) {
        return std::nullopt;
    }

    return set_height(h + 1);
}

auto puzzle::try_remove_row() const -> std::optional<puzzle>
{
    const auto [w, h, gx, gy, r, g] = unpack_meta();
    if (h <= MIN_HEIGHT) {
        return std::nullopt;
    }

    puzzle p{w, static_cast<u8>(h - 1), gx, gy, r, g};

    // Re-add all the blocks, blocks no longer fitting won't be added
    for (const block b : block_view()) {
        if (const std::optional<puzzle>& _p = p.try_add_block(b)) {
            p = *_p;
        }
    }

    const std::optional<block> b = p.try_get_target_block();
    if (p.get_goal() && b && !p.covers(gx, gy, b->get_width(), b->get_height())) {
        // Target no longer inside board
        return p.clear_goal();
    }
    if (p.get_goal() && !b) {
        // Target block removed during resize
        return p.clear_goal();
    }

    return p;
}

auto puzzle::try_add_block(const block b) const -> std::optional<puzzle>
{
    const u8 count = block_count();
    if (count == MAX_BLOCKS) {
        return std::nullopt;
    }

    if (!covers(b)) {
        return std::nullopt;
    }

    for (const block _b : block_view()) {
        if (_b.collides(b)) {
            return std::nullopt;
        }
    }

    const auto [w, h, gx, gy, r, g] = unpack_meta();
    std::array<u16, MAX_BLOCKS> blocks = repr.cooked.blocks;

    // This requires all empty blocks being at the end of the array (otherwise we might overwrite).
    // This is the case because empty blocks' most significant bit is 1 and the array is sorted.
    blocks[count] = b.repr;

    return puzzle(w, h, gx, gy, r, g, blocks);
}

auto puzzle::try_remove_block(const u8 x, const u8 y) const -> std::optional<puzzle>
{
    const std::optional<block>& b = try_get_block(x, y);
    if (!b) {
        return std::nullopt;
    }

    const auto [w, h, gx, gy, r, g] = unpack_meta();
    std::array<u16, MAX_BLOCKS> blocks = repr.cooked.blocks;
    for (u16& _b : blocks) {
        if (_b == b->repr) {
            _b = block().repr;
        }
    }

    return puzzle(w, h, gx, gy, r, g, blocks);
}

auto puzzle::try_toggle_target(const u8 x, const u8 y) const -> std::optional<puzzle>
{
    const std::optional<block> b = try_get_block(x, y);
    if (!b || b->get_immovable()) {
        return std::nullopt;
    }

    const auto [w, h, gx, gy, r, g] = unpack_meta();
    std::array<u16, MAX_BLOCKS> blocks = repr.cooked.blocks;

    for (u16& _b : blocks) {
        if (!block(_b).valid()) {
            // Empty blocks are at the end
            break;
        }

        if (_b != b->repr) {
            // Remove the old target(s)
            _b = block(_b).set_target(false).repr;
        } else {
            // Add the new target
            _b = block(_b).set_target(!b->get_target()).repr;
        }
    }

    const block _b = block(gx, gy, b->get_width(), b->get_height());
    if (covers(_b)) {
        // Old goal still valid
        return puzzle(w, h, gx, gy, r, g, blocks);
    }

    return puzzle(w, h, 0, 0, r, g, blocks);
}

auto puzzle::try_toggle_wall(const u8 x, const u8 y) const -> std::optional<puzzle>
{
    const std::optional<block> b = try_get_block(x, y);
    if (!b || b->get_target()) {
        return std::nullopt;
    }

    const auto [w, h, gx, gy, r, g] = unpack_meta();
    std::array<u16, MAX_BLOCKS> blocks = repr.cooked.blocks;

    for (u16& _b : blocks) {
        if (!block(_b).valid()) {
            // Empty blocks are at the end
            break;
        }

        if (_b == b->repr) {
            // Toggle wall
            _b = block(_b).set_immovable(!b->get_immovable()).repr;
        }
    }

    return puzzle(w, h, gx, gy, r, g, blocks);
}

auto puzzle::blocks_bitmap() const -> u64
{
    u64 bitmap = 0;
    for (u8 i = 0; i < MAX_BLOCKS; ++i) {
        block b(repr.cooked.blocks[i]);
        if (!b.valid()) {
            break;
        }

        auto [x, y, w, h, t, im] = b.unpack_repr();
        const u8 width = get_width();

        for (int dy = 0; dy < h; ++dy) {
            for (int dx = 0; dx < w; ++dx) {
                bitmap_set_bit(bitmap, width, x + dx, y + dy);
            }
        }
    }
    return bitmap;
}

auto puzzle::blocks_bitmap_h() const -> u64
{
    u64 bitmap = 0;
    for (u8 i = 0; i < MAX_BLOCKS; ++i) {
        block b(repr.cooked.blocks[i]);
        if (!b.valid()) {
            break;
        }
        const int dirs = b.principal_dirs();
        if (!(dirs & eas)) {
            continue;
        }

        auto [x, y, w, h, t, im] = b.unpack_repr();
        const u8 width = get_width();

        for (int dy = 0; dy < h; ++dy) {
            for (int dx = 0; dx < w; ++dx) {
                bitmap_set_bit(bitmap, width, x + dx, y + dy);
            }
        }
    }
    return bitmap;
}

auto puzzle::blocks_bitmap_v() const -> u64
{
    u64 bitmap = 0;
    for (u8 i = 0; i < MAX_BLOCKS; ++i) {
        block b(repr.cooked.blocks[i]);
        if (!b.valid()) {
            break;
        }
        const int dirs = b.principal_dirs();
        if (!(dirs & sou)) {
            continue;
        }

        auto [x, y, w, h, t, im] = b.unpack_repr();
        const u8 width = get_width();

        for (int dy = 0; dy < h; ++dy) {
            for (int dx = 0; dx < w; ++dx) {
                bitmap_set_bit(bitmap, width, x + dx, y + dy);
            }
        }
    }
    return bitmap;
}

auto puzzle::try_move_block_at(const u8 x, const u8 y, const dir dir) const -> std::optional<puzzle>
{
    const std::optional<block> b = try_get_block(x, y);
    const auto [bx, by, bw, bh, bt, bi] = b->unpack_repr();
    if (!b || bi) {
        return std::nullopt;
    }

    const auto [w, h, gx, gy, r, g] = unpack_meta();
    const int dirs = r ? b->principal_dirs() : nor | eas | sou | wes;

    // Get target block
    int _target_x = bx;
    int _target_y = by;
    switch (dir) {
    case nor:
        if (!(dirs & nor) || _target_y < 1) {
            return std::nullopt;
        }
        --_target_y;
        break;
    case eas:
        if (!(dirs & eas) || _target_x + bw >= w) {
            return std::nullopt;
        }
        ++_target_x;
        break;
    case sou:
        if (!(dirs & sou) || _target_y + bh >= h) {
            return std::nullopt;
        }
        ++_target_y;
        break;
    case wes:
        if (!(dirs & wes) || _target_x < 1) {
            return std::nullopt;
        }
        --_target_x;
        break;
    }
    const block moved_b = block(_target_x, _target_y, bw, bh, bt);

    // Check collisions
    for (const block _b : block_view()) {
        if (_b != b && _b.collides(moved_b)) {
            return std::nullopt;
        }
    }

    std::optional<puzzle> p = try_remove_block(x, y);
    if (!p) {
        return std::nullopt;
    }

    p = p->try_add_block(moved_b);
    if (!p) {
        return std::nullopt;
    }

    return p;
}

auto puzzle::explore_state_space() const -> std::pair<std::vector<puzzle>, std::vector<spring>>
{
    std::vector<puzzle> state_pool;
    puzzlemap<size_t> state_indices;
    std::vector<spring> links;

    // Buffer for all states we want to call GetNextStates() on
    std::vector<size_t> queue; // indices into state_pool

    // Start with the current state
    state_indices.emplace(*this, 0);
    state_pool.push_back(*this);
    queue.push_back(0);

    size_t head = 0;
    while (head < queue.size()) {
        const size_t current_idx = queue[head++];

        // Make a copy because references might be invalidated when inserting into the vector
        const puzzle current = state_pool[current_idx];

        current.for_each_adjacent([&](const puzzle& p)
        {
            auto [it, inserted] = state_indices.emplace(p, state_pool.size());
            if (inserted) {
                state_pool.push_back(p);
                queue.push_back(it->second);
            }
            links.emplace_back(current_idx, it->second);
        });
    }

    return {std::move(state_pool), std::move(links)};
}

auto puzzle::get_cluster_id_and_solution() const -> std::tuple<puzzle, std::vector<puzzle>, bool, int>
{
    const auto& [puzzles, moves] = explore_state_space();

    std::vector<size_t> solutions;
    puzzle min = puzzles[0];
    for (size_t i = 0; i < puzzles.size(); ++i) {
        if (puzzles[i] < min) {
            min = puzzles[i];
        }
        if (puzzles[i].goal_reached()) {
            solutions.emplace_back(i);
        }
    }

    // TODO: We only need to search until a distance equal to min_moves is found.
    //       Maybe DFS instead of BFS could even be faster on average?
    // TODO: Split moves from solvable calculation. We only need to count the moves if the cluster is solvable at all.
    graph_distances distances;
    distances.calculate_distances(puzzles.size(), moves, solutions);

    int max_distance = 0;
    for (size_t i = 0; i < distances.distances.size(); ++i) {
        if (distances.distances[i] > max_distance) {
            max_distance = distances.distances[i];
        }
    }

    return {min, puzzles, !solutions.empty(), max_distance};
}

auto puzzle::generate_block_sets(const puzzle& p,
                                 const blockset2& permitted_blocks,
                                 const block target_block,
                                 const u8 max_blocks) -> std::vector<blockmap2<u8>>
{
    std::vector<blockmap2<u8>> result;

    const u8 board_area = p.get_width() * p.get_height();
    blockmap2<u8> max_counts;

    blockmap2<u8> block_areas;
    for (const block& b : permitted_blocks) {
        const u8 block_area = b.get_width() * b.get_height();
        block_areas[b] = block_area;
    }

    const u8 target_area = target_block.get_width() * target_block.get_height();
    for (const block& b : permitted_blocks) {
        const u8 block_area = block_areas[b];
        max_counts[b] = std::min(max_blocks, static_cast<u8>((board_area - target_area) / block_area));
    }

    blockmap2<u8> current_set;
    while (true) {
        // Check if we have generated a valid set
        int used_blocks = 0;
        int used_area = 0;
        for (const block& b : permitted_blocks) {
            used_blocks += current_set[b];
            used_area += current_set[b] * block_areas[b];
        }
        if (used_blocks > 0 && used_blocks <= max_blocks && used_area <= board_area - target_area) {
            result.push_back(current_set);
        }

        // Increase counter
        size_t pos = 0;
        for (const block& b : permitted_blocks) {
            ++current_set[b];
            if (current_set[b] <= max_counts[b]) {
                break;
            }

            // The counter overflowed
            current_set[b] = 0;
            ++pos;
        }

        // All counters overflowed, finished
        if (pos == permitted_blocks.size()) {
            break;
        }
    }

    return result;
}

auto puzzle::generate_initial_puzzles(const puzzle& p,
                                      const block target_block,
                                      const std::tuple<u8, u8, u8, u8>& target_block_pos_range) -> std::vector<puzzle>
{
    std::vector<puzzle> result;

    const auto [txs, tys, txe, tye] = target_block_pos_range;

    for (int tx = txs; tx <= txe; ++tx) {
        for (int ty = tys; ty <= tye; ++ty) {
            block t = target_block;
            t = t.set_x(tx);
            t = t.set_y(ty);

            // This cannot happen if the target_block_pos_range is set correctly
            #ifdef RUNTIME_CHECKS
            if (!p.covers(t)) {
                continue;
            }
            #endif

            std::array<u16, MAX_BLOCKS> blocks = invalid_blocks();
            blocks[0] = t.repr;

            // Don't exclude already won configurations, as min_moves is based on the
            // max distance from the entire cluster, not only the initial state

            // puzzle _p = puzzle(p.repr.cooked.meta, blocks);
            // if (_p.goal_reached()) {
            //     continue;
            // }
            // result.emplace_back(_p);

            result.emplace_back(p.repr.cooked.meta, blocks);
        }
    }

    return result;
}

auto puzzle::explore_puzzle_space(const blockset2& permitted_blocks,
                                  const block target_block,
                                  const std::tuple<u8, u8, u8, u8>& target_block_pos_range,
                                  const size_t max_blocks,
                                  const int min_moves,
                                  const threadpool thread_pool) const -> puzzleset
{
    const auto [w, h, gx, gy, r, g] = unpack_meta();

    // Implemented in the slowest, stupidest way for now:
    // 1. Iterate through all possible permitted_blocks permutations using recursive tree descent
    // 2. Find the cluster id of the permutation by populating the entire state space
    //    - We could do some preprocessing to quickly reduce the numeric value
    //      of the state and check if its already contained in visited_clusters,
    //      this could save some state space calculations.
    // 3. Add it to visited_clusters if unseen

    // TODO: Use thread local resources instead of this fuckery (or concurrent map?)
    std::mutex mtx;
    std::mutex cache_mtx;
    std::mutex print_mtx;
    puzzleset visited_clusters;
    const puzzle empty_puzzle = puzzle(w, h, gx, gy, r, g);

    // TODO: Don't use a hashmap for this, use denser vector representation for better recursion performance
    // Call with max_blocks - 1 because the target is already placed
    const std::vector<blockmap2<u8>> sets = generate_block_sets(empty_puzzle,
                                                                permitted_blocks,
                                                                target_block,
                                                                max_blocks - 1);
    traceln("Generated {} block inventories", sets.size());

    const std::vector<puzzle> puzzles = generate_initial_puzzles(empty_puzzle, target_block, target_block_pos_range);
    traceln("Generated {} starting configurations", puzzles.size());

    int total = 0;
    int duplicates = 0;

    // TODO: This cache can easily fill system ram
    //       - Only store hashes, not puzzles?
    //       - Use an LRU cache to have a memory consumption bound?
    puzzleset seen_statespaces;

    const auto place_blocks = [&](const size_t i)
    {
        const size_t current_inventory = i / puzzles.size();
        const size_t current_puzzle = i % puzzles.size();

        const blockmap2<u8> set = sets[current_inventory];
        const puzzle& p = puzzles[current_puzzle];
        const u64 bitmap = p.blocks_bitmap();

        {
            std::lock_guard<std::mutex> lock(print_mtx);
            traceln("Placing inventory {} for starting configuration {}", current_inventory, current_puzzle);
        }
        place_block_set(p,
                        set,
                        bitmap,
                        1,
                        [&](const puzzle& _p)
                        {
                            // Sort the blocks, so identical puzzles get the same hash
                            const puzzle& sorted = _p.set_blocks(_p.repr.cooked.blocks);

                            // TODO: Pre-filter using a coarse key:
                            // - If two puzzles are in the same cluster, this key must match
                            // - If the key matches, puzzles might still be in different clusters
                            // If a new puzzle arrives, do a bidirectional BFS from the new
                            // puzzle to all puzzles with the same key, if both directions meet,
                            // we already had this cluster.
                            // If nothing meets, we have to do the full statespace generation.
                            //
                            // Key could contain:
                            // - counts of block types (inventory)
                            // - which row/column pieces live in
                            // - ordering of pieces in a row or column

                            bool inserted;
                            {
                                std::lock_guard<std::mutex> lock(cache_mtx);
                                const auto& [it, _inserted] = seen_statespaces.insert(sorted);
                                inserted = _inserted;

                                ++total;
                            }

                            // Only scan statespace if we haven't seen the state
                            if (!inserted) {
                                return;
                            }

                            // TODO: This is the main issue. Need a lot of prefiltering/as much caching as I have memory
                            const auto [cluster_id, statespace, winnable, max_moves] = sorted.
                                get_cluster_id_and_solution();

                            {
                                std::lock_guard<std::mutex> lock(cache_mtx);
                                for (const puzzle& seen : statespace) {
                                    seen_statespaces.insert(seen);
                                }
                            }

                            {
                                std::lock_guard<std::mutex> lock(mtx);
                                if (visited_clusters.contains(cluster_id)) {
                                    ++duplicates;
                                } else if (winnable && max_moves >= min_moves) {
                                    // print_bitmap(cluster_id.blocks_bitmap(), _p.get_width(), _p.get_height(), "Found Cluster");
                                    visited_clusters.emplace(cluster_id);
                                }
                            }
                        });
    };

    // Iterate through all starting configurations + inventories
    if (thread_pool) {
        (*thread_pool)->submit_loop(0, puzzles.size() * sets.size(), place_blocks, LARGE_TASK_BLOCK_SIZE).wait();
    } else {
        for (size_t i = 0; i < puzzles.size() * sets.size(); ++i) {
            place_blocks(i);
        }
    }

    infoln("Found {} solvable clusters with at least {} required moves", visited_clusters.size(), min_moves);
    traceln("- Scanned {} puzzles", total);
    traceln("- {} belong to duplicate clusters", duplicates);
    traceln("- {} don't match the requirements", total - duplicates - visited_clusters.size());

    return visited_clusters;
}