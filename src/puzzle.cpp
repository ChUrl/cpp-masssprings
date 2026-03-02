#include "puzzle.hpp"

#include <algorithm>
#include <boost/unordered/unordered_flat_map.hpp>

auto puzzle::block::create_repr(const uint8_t x, const uint8_t y, const uint8_t w, const uint8_t h, const bool t,
                                const bool i) -> uint16_t
{
    return block().set_x(x).set_y(y).set_width(w).set_height(h).set_target(t).set_immovable(i).repr & ~INVALID;
}

auto puzzle::block::set_x(const uint8_t x) const -> block
{
    if (x > 7) {
        throw std::invalid_argument("Block x-position out of bounds");
    }

    block b = *this;
    set_bits(b.repr, X_S, X_E, x);
    return b;
}

auto puzzle::block::set_y(const uint8_t y) const -> block
{
    if (y > 7) {
        throw std::invalid_argument("Block y-position out of bounds");
    }

    block b = *this;
    set_bits(b.repr, Y_S, Y_E, y);
    return b;
}

auto puzzle::block::set_width(const uint8_t width) const -> block
{
    if (width - 1 > 7) {
        throw std::invalid_argument("Block width out of bounds");
    }

    block b = *this;
    set_bits(b.repr, WIDTH_S, WIDTH_E, width - 1u);
    return b;
}

auto puzzle::block::set_height(const uint8_t height) const -> block
{
    if (height - 1 > 7) {
        throw std::invalid_argument("Block height out of bounds");
    }

    block b = *this;
    set_bits(b.repr, HEIGHT_S, HEIGHT_E, height - 1u);
    return b;
}

auto puzzle::block::set_target(const bool target) const -> block
{
    block b = *this;
    set_bits(b.repr, TARGET_S, TARGET_E, target);
    return b;
}

auto puzzle::block::set_immovable(const bool immovable) const -> block
{
    block b = *this;
    set_bits(b.repr, IMMOVABLE_S, IMMOVABLE_E, immovable);
    return b;
}

auto puzzle::block::unpack_repr() const -> std::tuple<uint8_t, uint8_t, uint8_t, uint8_t, bool, bool>
{
    const uint8_t x = get_x();
    const uint8_t y = get_y();
    const uint8_t w = get_width();
    const uint8_t h = get_height();
    const bool t = get_target();
    const bool i = get_immovable();

    return {x, y, w, h, t, i};
}

auto puzzle::block::get_x() const -> uint8_t
{
    return get_bits(repr, X_S, X_E);
}

auto puzzle::block::get_y() const -> uint8_t
{
    return get_bits(repr, Y_S, Y_E);
}

auto puzzle::block::get_width() const -> uint8_t
{
    return get_bits(repr, WIDTH_S, WIDTH_E) + 1u;
}

auto puzzle::block::get_height() const -> uint8_t
{
    return get_bits(repr, HEIGHT_S, HEIGHT_E) + 1u;
}

auto puzzle::block::get_target() const -> bool
{
    return get_bits(repr, TARGET_S, TARGET_E);
}

auto puzzle::block::get_immovable() const -> bool
{
    return get_bits(repr, IMMOVABLE_S, IMMOVABLE_E);
}

auto puzzle::block::valid() const -> bool
{
    const auto [x, y, w, h, t, i] = unpack_repr();

    if (t && i) {
        return false;
    }

    // This means the first bit is set, marking the block as empty
    if (repr & INVALID) {
        return false;
    }

    return w > 0 && h > 0 && x + w <= MAX_WIDTH && y + h <= MAX_HEIGHT;
}

auto puzzle::block::principal_dirs() const -> uint8_t
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

auto puzzle::block::covers(const int _x, const int _y) const -> bool
{
    const auto [x, y, w, h, t, i] = unpack_repr();

    return _x >= x && _x < x + w && _y >= y && _y < y + h;
}

auto puzzle::block::collides(const block b) const -> bool
{
    const auto [x, y, w, h, t, i] = unpack_repr();
    const auto [bx, by, bw, bh, bt, bi] = b.unpack_repr();

    return x < bx + bw && x + w > bx && y < by + bh && y + h > by;
}

auto puzzle::create_meta(const std::tuple<uint8_t, uint8_t, uint8_t, uint8_t, bool, bool>& meta) -> uint16_t
{
    const auto [w, h, gx, gy, r, g] = meta;

    uint16_t m = 0;
    set_bits(m, WIDTH_S, WIDTH_E, w - 1u);
    set_bits(m, HEIGHT_S, HEIGHT_E, h - 1u);
    set_bits(m, GOAL_X_S, GOAL_X_E, gx);
    set_bits(m, GOAL_Y_S, GOAL_Y_E, gy);
    set_bits(m, RESTRICTED_S, RESTRICTED_E, r);
    set_bits(m, GOAL_S, GOAL_E, g);
    return m;
}

auto puzzle::create_repr(const uint8_t w, const uint8_t h, const uint8_t tx, const uint8_t ty, const bool r,
                         const bool g, const std::array<uint16_t, MAX_BLOCKS>& b) -> repr_cooked
{
    repr_cooked repr = puzzle().set_width(w).set_height(h).set_goal_x(tx).set_goal_y(ty).set_restricted(r).set_goal(g).
                                set_blocks(b).repr.cooked;
    repr.meta &= ~INVALID;
    return repr;
}

auto puzzle::create_repr(const uint64_t byte_0, const uint64_t byte_1, const uint64_t byte_2,
                         const uint64_t byte_3) -> repr_cooked
{
    repr_u repr{};
    repr.raw = std::array<uint64_t, 4>{byte_0, byte_1, byte_2, byte_3};

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

auto puzzle::set_restricted(const bool restricted) const -> puzzle
{
    uint16_t meta = repr.cooked.meta;
    set_bits(meta, RESTRICTED_S, RESTRICTED_E, restricted);
    return puzzle(meta, repr.cooked.blocks);
}

auto puzzle::set_width(const uint8_t width) const -> puzzle
{
    if (width - 1 > MAX_WIDTH) {
        throw "Board width out of bounds";
    }

    uint16_t meta = repr.cooked.meta;
    set_bits(meta, WIDTH_S, WIDTH_E, width - 1u);
    return puzzle(meta, repr.cooked.blocks);
}

auto puzzle::set_height(const uint8_t height) const -> puzzle
{
    if (height - 1 > MAX_HEIGHT) {
        throw "Board height out of bounds";
    }

    uint16_t meta = repr.cooked.meta;
    set_bits(meta, HEIGHT_S, HEIGHT_E, height - 1u);
    return puzzle(meta, repr.cooked.blocks);
}

auto puzzle::set_goal(const bool goal) const -> puzzle
{
    uint16_t meta = repr.cooked.meta;
    set_bits(meta, GOAL_S, GOAL_E, goal);
    return puzzle(meta, repr.cooked.blocks);
}

auto puzzle::set_goal_x(const uint8_t target_x) const -> puzzle
{
    if (target_x >= MAX_WIDTH) {
        throw "Board target x out of bounds";
    }

    uint16_t meta = repr.cooked.meta;
    set_bits(meta, GOAL_X_S, GOAL_X_E, target_x);
    return puzzle(meta, repr.cooked.blocks);
}

auto puzzle::set_goal_y(const uint8_t target_y) const -> puzzle
{
    if (target_y >= MAX_HEIGHT) {
        throw "Board target y out of bounds";
    }

    uint16_t meta = repr.cooked.meta;
    set_bits(meta, GOAL_Y_S, GOAL_Y_E, target_y);
    return puzzle(meta, repr.cooked.blocks);
}

auto puzzle::set_blocks(std::array<uint16_t, MAX_BLOCKS> blocks) const -> puzzle
{
    puzzle p = *this;
    std::ranges::sort(blocks);
    p.repr.cooked.blocks = blocks;
    return p;
}

auto puzzle::unpack_meta() const -> std::tuple<uint8_t, uint8_t, uint8_t, uint8_t, bool, bool>
{
    const uint8_t w = get_width();
    const uint8_t h = get_height();
    const uint8_t tx = get_goal_x();
    const uint8_t ty = get_goal_y();
    const bool r = get_restricted();
    const bool g = get_goal();

    return {w, h, tx, ty, r, g};
}

auto puzzle::get_restricted() const -> bool
{
    return get_bits(repr.cooked.meta, RESTRICTED_S, RESTRICTED_E);
}

auto puzzle::get_width() const -> uint8_t
{
    return get_bits(repr.cooked.meta, WIDTH_S, WIDTH_E) + 1u;
}

auto puzzle::get_height() const -> uint8_t
{
    return get_bits(repr.cooked.meta, HEIGHT_S, HEIGHT_E) + 1u;
}

auto puzzle::get_goal() const -> bool
{
    return get_bits(repr.cooked.meta, GOAL_S, GOAL_E);
}

auto puzzle::get_goal_x() const -> uint8_t
{
    return get_bits(repr.cooked.meta, GOAL_X_S, GOAL_X_E);
}

auto puzzle::get_goal_y() const -> uint8_t
{
    return get_bits(repr.cooked.meta, GOAL_Y_S, GOAL_Y_E);
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
    std::pair<uint8_t, uint8_t> size{0, 0};

    bool parsed_goal = false;
    std::pair<uint8_t, uint8_t> goal{0, 0};

    bool parsed_restricted = false;
    bool restricted = true;

    bool parsed_blocks = false;
    std::vector<uint16_t> bs;
    std::array<uint16_t, MAX_BLOCKS> blocks = invalid_blocks();

    const auto digit = [&](const char c)
    {
        return std::string("0123456789").contains(c);
    };

    // S:[3x3]
    const auto parse_size = [&](size_t& pos)
    {
        uint8_t w = std::stoi(string_repr.substr(pos + 3, 1));
        uint8_t h = std::stoi(string_repr.substr(pos + 5, 1));
        size = std::make_pair(w, h);

        parsed_size = true;
        pos += 6;
    };

    // G:[1,1] (optional)
    const auto parse_goal = [&](size_t& pos)
    {
        uint8_t gx = std::stoi(string_repr.substr(pos + 3, 1));
        uint8_t gy = std::stoi(string_repr.substr(pos + 5, 1));
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
    const auto parse_block = [&](size_t& pos, const uint8_t x, const uint8_t y)
    {
        if (string_repr[pos] == '_') {
            return block();
        }

        const uint8_t w = std::stoi(string_repr.substr(pos, 1));
        const uint8_t h = std::stoi(string_repr.substr(pos + 2, 1));
        const bool t = string_repr[pos + 1] == 'X';
        const bool i = string_repr[pos + 1] == '*';

        pos += 2;
        return block(x, y, w, h, t, i);
    };

    // {1x1 _ _}
    const auto parse_row = [&](size_t& pos, const uint8_t y)
    {
        std::vector<uint16_t> row;

        uint8_t x = 0;
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
        std::vector<uint16_t> rows;

        uint8_t y = 0;
        ++pos; // Skip [
        while (string_repr[pos] != ']') {
            if (string_repr[pos] == '{') {
                std::vector<uint16_t> row = parse_row(pos, y);
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

    infoln("Validating puzzle {}", string_repr());

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

auto puzzle::block_count() const -> uint8_t
{
    uint8_t count = 0;
    for (const uint16_t b : repr.cooked.blocks) {
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

auto puzzle::try_get_block(const uint8_t x, const uint8_t y) const -> std::optional<block>
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

auto puzzle::covers(const uint8_t x, const uint8_t y, const uint8_t _w, const uint8_t _h) const -> bool
{
    return x + _w <= get_width() && y + _h <= get_height();
}

auto puzzle::covers(const uint8_t x, const uint8_t y) const -> bool
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

auto puzzle::try_set_goal(const uint8_t x, const uint8_t y) const -> std::optional<puzzle>
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
    const uint8_t w = get_width();
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

    puzzle p{static_cast<uint8_t>(w - 1), h, 0, 0, r, g};

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
    const uint8_t h = get_height();
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

    puzzle p{w, static_cast<uint8_t>(h - 1), gx, gy, r, g};

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
    const uint8_t count = block_count();
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
    std::array<uint16_t, MAX_BLOCKS> blocks = repr.cooked.blocks;

    // This requires all empty blocks being at the end of the array (otherwise we might overwrite).
    // This is the case because empty blocks' most significant bit is 1 and the array is sorted.
    blocks[count] = b.repr;

    return puzzle(w, h, gx, gy, r, g, blocks);
}

auto puzzle::try_remove_block(const uint8_t x, const uint8_t y) const -> std::optional<puzzle>
{
    const std::optional<block>& b = try_get_block(x, y);
    if (!b) {
        return std::nullopt;
    }

    const auto [w, h, gx, gy, r, g] = unpack_meta();
    std::array<uint16_t, MAX_BLOCKS> blocks = repr.cooked.blocks;
    for (uint16_t& _b : blocks) {
        if (_b == b->repr) {
            _b = block().repr;
        }
    }

    return puzzle(w, h, gx, gy, r, g, blocks);
}

auto puzzle::try_toggle_target(const uint8_t x, const uint8_t y) const -> std::optional<puzzle>
{
    const std::optional<block> b = try_get_block(x, y);
    if (!b || b->get_immovable()) {
        return std::nullopt;
    }

    const auto [w, h, gx, gy, r, g] = unpack_meta();
    std::array<uint16_t, MAX_BLOCKS> blocks = repr.cooked.blocks;

    for (uint16_t& _b : blocks) {
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

auto puzzle::try_toggle_wall(const uint8_t x, const uint8_t y) const -> std::optional<puzzle>
{
    const std::optional<block> b = try_get_block(x, y);
    if (!b || b->get_target()) {
        return std::nullopt;
    }

    const auto [w, h, gx, gy, r, g] = unpack_meta();
    std::array<uint16_t, MAX_BLOCKS> blocks = repr.cooked.blocks;

    for (uint16_t& _b : blocks) {
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

auto puzzle::try_move_block_at(const uint8_t x, const uint8_t y, const direction dir) const -> std::optional<puzzle>
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

auto puzzle::try_move_block_at_fast(const uint64_t bitmap, const uint8_t block_idx,
                                    const direction dir) const -> std::optional<puzzle>
{
    const block b = block(repr.cooked.blocks[block_idx]);
    const auto [bx, by, bw, bh, bt, bi] = b.unpack_repr();
    if (bi) {
        return std::nullopt;
    }

    const auto [w, h, gx, gy, r, g] = unpack_meta();
    const int dirs = r ? b.principal_dirs() : nor | eas | sou | wes;

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

    // Check collisions
    const uint64_t bm = bitmap_clear_block(bitmap, b);
    if (bitmap_check_collision(bm, b, dir)) {
        return std::nullopt;
    }

    // Replace block
    const std::array<uint16_t, MAX_BLOCKS> blocks = sorted_replace(repr.cooked.blocks, block_idx,
                                                                   block::create_repr(
                                                                       _target_x, _target_y, bw, bh, bt));

    // This constructor doesn't sort
    return puzzle(std::make_tuple(w, h, gx, gy, r, g), blocks);
}

auto puzzle::sorted_replace(std::array<uint16_t, MAX_BLOCKS> blocks, const uint8_t idx,
                            const uint16_t new_val) -> std::array<uint16_t, MAX_BLOCKS>
{
    // Remove old entry
    for (uint8_t i = idx; i < MAX_BLOCKS - 1; ++i) {
        blocks[i] = blocks[i + 1];
    }
    blocks[MAX_BLOCKS - 1] = block::INVALID;

    // Find insertion point for new_val
    uint8_t insert_at = 0;
    while (insert_at < MAX_BLOCKS && blocks[insert_at] < new_val) {
        ++insert_at;
    }

    // Shift right and insert
    for (uint8_t i = MAX_BLOCKS - 1; i > insert_at; --i) {
        blocks[i] = blocks[i - 1];
    }
    blocks[insert_at] = new_val;

    return blocks;
}

auto puzzle::blocks_bitmap() const -> uint64_t
{
    uint64_t bitmap = 0;
    for (uint8_t i = 0; i < MAX_BLOCKS; ++i) {
        block b(repr.cooked.blocks[i]);
        if (!b.valid()) {
            break;
        }

        auto [x, y, w, h, t, im] = b.unpack_repr();

        for (int dy = 0; dy < h; ++dy) {
            for (int dx = 0; dx < w; ++dx) {
                bitmap |= 1ULL << ((y + dy) * 8 + (x + dx));
            }
        }
    }
    return bitmap;
}

inline auto puzzle::bitmap_set_bit(const uint64_t bitmap, const uint8_t x, const uint8_t y) -> uint64_t
{
    return bitmap & ~(1ULL << (y * 8 + x));
}

inline auto puzzle::bitmap_get_bit(const uint64_t bitmap, const uint8_t x, const uint8_t y) -> bool
{
    return bitmap & (1ULL << (y * 8 + x));
}

auto puzzle::bitmap_clear_block(uint64_t bitmap, const block b) -> uint64_t
{
    const auto [x, y, w, h, t, i] = b.unpack_repr();

    for (int dy = 0; dy < h; ++dy) {
        for (int dx = 0; dx < w; ++dx) {
            bitmap = bitmap_set_bit(bitmap, x + dx, y + dy);
        }
    }

    return bitmap;
}

auto puzzle::bitmap_check_collision(const uint64_t bitmap, const block b) -> bool
{
    const auto [x, y, w, h, t, i] = b.unpack_repr();

    for (int dy = 0; dy < h; ++dy) {
        for (int dx = 0; dx < w; ++dx) {
            if (bitmap_get_bit(bitmap, x + dx, y + dy)) {
                return true; // collision
            }
        }
    }

    return false;
}

auto puzzle::bitmap_check_collision(const uint64_t bitmap, const block b, const direction dir) -> bool
{
    const auto [x, y, w, h, t, i] = b.unpack_repr();

    switch (dir) {
    case nor: // Check the row above: (x...x+w-1, y-1)
        for (int dx = 0; dx < w; ++dx) {
            if (bitmap_get_bit(bitmap, x + dx, y - 1)) {
                return true;
            }
        }
        break;
    case sou: // Check the row below: (x...x+w-1, y+h)
        for (int dx = 0; dx < w; ++dx) {
            if (bitmap_get_bit(bitmap, x + dx, y + h)) {
                return true;
            }
        }
        break;
    case wes: // Check the column left: (x-1, y...y+h-1)
        for (int dy = 0; dy < h; ++dy) {
            if (bitmap_get_bit(bitmap, x - 1, y + dy)) {
                return true;
            }
        }
        break;
    case eas: // Check the column right: (x+w, y...y+h-1)
        for (int dy = 0; dy < h; ++dy) {
            if (bitmap_get_bit(bitmap, x + w, y + dy)) {
                return true;
            }
        }
        break;
    }
    return false;
}

auto puzzle::explore_state_space() const -> std::pair<std::vector<puzzle>, std::vector<std::pair<size_t, size_t>>>
{
    const std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

    std::vector<puzzle> state_pool;
    boost::unordered_flat_map<puzzle, std::size_t, puzzle_hasher> state_indices;
    std::vector<std::pair<size_t, size_t>> links;

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

    const std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();

    infoln("Explored puzzle. Took {} ms.", std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    infoln("State space has size {} with {} transitions.", state_pool.size(), links.size());

    return {std::move(state_pool), std::move(links)};
}