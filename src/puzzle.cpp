#include "puzzle.hpp"

#include "graph_distances.hpp"
#include "state_model.hpp"

#include <algorithm>
#include <bit>
#include <boost/program_options/detail/cmdline.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <set>

auto block::create_repr(const u8 x, const u8 y, const u8 w, const u8 h, const bool t, const bool i)
    -> u16
{
    return block()
               .set_x(x)
               .set_y(y)
               .set_width(w)
               .set_height(h)
               .set_target(t)
               .set_immovable(i)
               .repr &
        ~INVALID;
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

auto puzzle::create_repr(const u8 w, const u8 h, const u8 tx, const u8 ty, const bool r,
                         const bool g, const std::array<u16, MAX_BLOCKS>& b) -> repr_cooked
{
    repr_cooked repr = puzzle()
                           .set_width(w)
                           .set_height(h)
                           .set_goal_x(tx)
                           .set_goal_y(ty)
                           .set_restricted(r)
                           .set_goal(g)
                           .set_blocks(b)
                           .repr.cooked;
    repr.meta &= ~INVALID;
    return repr;
}

auto puzzle::create_repr(const u64 byte_0, const u64 byte_1, const u64 byte_2, const u64 byte_3)
    -> repr_cooked
{
    repr_u repr{};
    repr.raw = std::array<u64, 4>{byte_0, byte_1, byte_2, byte_3};

    return repr.cooked;
}

auto puzzle::create_repr(const std::string& string_repr) -> parsed_repr
{
    const std::optional<parsed_repr> repr = try_parse_string_repr(string_repr);
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

auto puzzle::sorted_blocks() const -> puzzle
{
    return set_blocks(repr.cooked.blocks);
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
    hash_combine(h, wall_mask_);
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
    // Walls are stored separately, but the preset syntax keeps them as W*H tokens. A wall mask
    // only knows cells, so each wall cell is emitted as 1*1.
    for (u8 y = 0; y < h; ++y) {
        for (u8 x = 0; x < w; ++x) {
            if (wall_at(x, y)) {
                linear_blocks[y * w + x] = block(x, y, 1, 1, false, true);
            }
        }
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

auto puzzle::try_parse_string_repr(const std::string& string_repr) -> std::optional<parsed_repr>
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

    const auto digit = [&](const char c) { return std::string("0123456789").contains(c); };

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
            break;
        default:
            break;
        }
    }

    if (!parsed_size || !parsed_restricted || !parsed_blocks) {
        warnln("Failed to parse required attribute");
        return std::nullopt;
    }

    // Wall tokens stay valid in the preset format, but are stored in the separate wall mask.
    // Only the target and movable blocks count towards MAX_BLOCKS.
    u64 walls = 0;
    size_t movable_count = 0;
    for (const u16 value : bs) {
        const block b(value);
        if (b.get_immovable()) {
            for (int dy = 0; dy < b.get_height(); ++dy) {
                for (int dx = 0; dx < b.get_width(); ++dx) {
                    const int x = b.get_x() + dx;
                    const int y = b.get_y() + dy;
                    if (x >= size.first || y >= size.second) {
                        warnln("Wall outside the board");
                        return std::nullopt;
                    }
                    walls |= u64{1} << (y * size.first + x);
                }
            }
            continue;
        }

        if (movable_count >= MAX_BLOCKS) {
            warnln("Parsed too many movable blocks");
            return std::nullopt;
        }
        blocks[movable_count++] = value;
    }

    parsed_repr parsed;
    parsed.cooked = create_repr(size.first, size.second, goal.first, goal.second, restricted,
                                parsed_goal, blocks);
    parsed.walls = walls;
    return parsed;
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

    if ((wall_mask_ & ~board_cell_mask()) != 0) {
        return "Walls Outside Board";
    }
    if ((wall_mask_ & blocks_bitmap_without_walls()) != 0) {
        return "Wall Overlaps Block";
    }
    // traceln("Validating puzzle \"{}\"", string_repr());

    const std::optional<block>& b = try_get_target_block();
    if (g && b) {
        const block goal_block(gx, gy, b->get_width(), b->get_height());
        if (covers(goal_block) && (wall_mask_ & block_cell_mask(goal_block)) != 0) {
            return "Goal Covered By Wall";
        }
    }
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

    if (get_goal() && b && gx > 0 && gx + b->get_width() < w && gy > 0 &&
        gy + b->get_height() < h) {
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

auto puzzle::board_cell_mask() const -> u64
{
    const int cells = get_width() * get_height();
    return cells >= 64 ? ~u64{0} : ((u64{1} << cells) - 1);
}

auto puzzle::block_cell_mask(const block b) const -> u64
{
    const auto [x, y, w, h, target, immovable] = b.unpack_repr();
    (void)target;
    (void)immovable;
    const u8 width = get_width();
    u64 mask = 0;
    for (int dy = 0; dy < h; ++dy) {
        for (int dx = 0; dx < w; ++dx) {
            mask |= u64{1} << ((y + dy) * width + x + dx);
        }
    }
    return mask;
}

auto puzzle::blocks_bitmap_without_walls() const -> u64
{
    u64 bitmap = 0;
    for (const block b : block_view()) {
        bitmap |= block_cell_mask(b);
    }
    return bitmap;
}

auto puzzle::wall_mask() const -> u64
{
    return wall_mask_;
}

auto puzzle::wall_count() const -> u8
{
    return static_cast<u8>(std::popcount(wall_mask_));
}

auto puzzle::wall_at(const u8 x, const u8 y) const -> bool
{
    if (!covers(x, y)) {
        return false;
    }
    return ((wall_mask_ >> (y * get_width() + x)) & 1u) != 0;
}

auto puzzle::has_walls() const -> bool
{
    return wall_mask_ != 0;
}

auto puzzle::wall_rectangles() const -> std::vector<block>
{
    std::vector<block> result;
    u64 remaining = wall_mask_;
    const u8 width = get_width();
    const u8 height = get_height();

    while (remaining != 0) {
        const int cell = std::countr_zero(remaining);
        const int x = cell % width;
        const int y = cell / width;

        // Horizontal-first greedy rectangle. The lowest unvisited wall cell is the top-left
        // corner of the rectangle, so the decomposition is deterministic.
        int w = 0;
        while (x + w < width && ((remaining >> (y * width + x + w)) & 1u) != 0) {
            ++w;
        }
        int h = 1;
        while (y + h < height) {
            bool full = true;
            for (int dx = 0; dx < w; ++dx) {
                if (((remaining >> ((y + h) * width + x + dx)) & 1u) == 0) {
                    full = false;
                    break;
                }
            }
            if (!full) {
                break;
            }
            ++h;
        }

        for (int dy = 0; dy < h; ++dy) {
            for (int dx = 0; dx < w; ++dx) {
                remaining &= ~(u64{1} << ((y + dy) * width + x + dx));
            }
        }
        result.emplace_back(x, y, w, h, false, false);
    }

    return result;
}

auto puzzle::with_wall_mask(const u64 mask) const -> std::optional<puzzle>
{
    if ((mask & ~board_cell_mask()) != 0) {
        return std::nullopt;
    }
    if ((mask & blocks_bitmap_without_walls()) != 0) {
        return std::nullopt;
    }

    if (get_goal()) {
        if (const auto target = try_get_target_block()) {
            const block goal_block(get_goal_x(), get_goal_y(), target->get_width(),
                                   target->get_height());
            if ((mask & block_cell_mask(goal_block)) != 0) {
                return std::nullopt;
            }
        }
    }

    puzzle result = *this;
    result.wall_mask_ = mask;
    return result;
}

auto puzzle::remap_wall_mask(const u8 old_w, const u8 old_h, const u8 new_w, const u8 new_h) const
    -> u64
{
    u64 result = 0;
    const u8 width = std::min(old_w, new_w);
    const u8 height = std::min(old_h, new_h);
    for (u8 y = 0; y < height; ++y) {
        for (u8 x = 0; x < width; ++x) {
            if (((wall_mask_ >> (y * old_w + x)) & 1u) != 0) {
                result |= u64{1} << (y * new_w + x);
            }
        }
    }
    return result;
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
    if ((block_cell_mask(block(x, y, b->get_width(), b->get_height())) & wall_mask_) != 0) {
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

    puzzle p = set_width(w + 1);
    p.wall_mask_ = remap_wall_mask(w, get_height(), w + 1, get_height());
    return p;
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
    p.wall_mask_ = remap_wall_mask(w, h, static_cast<u8>(w - 1), h);

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
    p.wall_mask_ = remap_wall_mask(w, h, w, static_cast<u8>(h - 1));

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
    if (!covers(b)) {
        return std::nullopt;
    }
    if ((block_cell_mask(b) & wall_mask_) != 0) {
        return std::nullopt;
    }

    // Legacy callers could still pass an immovable block. Treat it as wall cells instead of
    // inserting it into the limited block array.
    if (b.get_immovable()) {
        return with_wall_mask(wall_mask_ | block_cell_mask(b));
    }

    const u8 count = block_count();
    if (count == MAX_BLOCKS) {
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

    return puzzle(w, h, gx, gy, r, g, blocks, wall_mask_);
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

    return puzzle(w, h, gx, gy, r, g, blocks, wall_mask_);
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
    if (covers(_b) && (block_cell_mask(_b) & wall_mask_) == 0) {
        // Old goal still valid
        return puzzle(w, h, gx, gy, r, g, blocks, wall_mask_);
    }

    return puzzle(w, h, 0, 0, r, g, blocks, wall_mask_);
}

auto puzzle::try_toggle_wall(const u8 x, const u8 y) const -> std::optional<puzzle>
{
    if (!covers(x, y)) {
        return std::nullopt;
    }

    if (wall_at(x, y)) {
        // Toggle the whole wall piece back into a movable block. The mask stores cells, so use
        // the same greedy rectangle decomposition as the editor and wall inventory.
        for (const block wall : wall_rectangles()) {
            if (!wall.covers(x, y)) {
                continue;
            }
            const std::optional<puzzle> without_wall = try_remove_wall(x, y);
            if (!without_wall) {
                return std::nullopt;
            }
            return without_wall->try_add_block(
                block(wall.get_x(), wall.get_y(), wall.get_width(), wall.get_height(), false));
        }
        return std::nullopt;
    }

    if (const std::optional<block> b = try_get_block(x, y)) {
        if (b->get_target()) {
            return std::nullopt;
        }
        const std::optional<puzzle> without_block = try_remove_block(x, y);
        if (!without_block) {
            return std::nullopt;
        }
        return without_block->with_wall_mask(without_block->wall_mask_ | block_cell_mask(*b));
    }

    return try_add_wall(x, y);
}

auto puzzle::try_remove_wall(const u8 x, const u8 y) const -> std::optional<puzzle>
{
    if (!wall_at(x, y)) {
        return std::nullopt;
    }

    for (const block wall : wall_rectangles()) {
        if (!wall.covers(x, y)) {
            continue;
        }
        u64 mask = wall_mask_;
        for (int dy = 0; dy < wall.get_height(); ++dy) {
            for (int dx = 0; dx < wall.get_width(); ++dx) {
                mask &= ~(u64{1} << ((wall.get_y() + dy) * get_width() + wall.get_x() + dx));
            }
        }
        return with_wall_mask(mask);
    }
    return std::nullopt;
}

auto puzzle::try_add_wall(const u8 x, const u8 y) const -> std::optional<puzzle>
{
    if (!covers(x, y) || wall_at(x, y) || try_get_block(x, y)) {
        return std::nullopt;
    }
    return with_wall_mask(wall_mask_ | (u64{1} << (y * get_width() + x)));
}

auto puzzle::try_add_wall_block(const block b) const -> std::optional<puzzle>
{
    if (!covers(b) || b.get_target()) {
        return std::nullopt;
    }
    return with_wall_mask(wall_mask_ | block_cell_mask(b));
}

auto puzzle::blocks_bitmap() const -> u64
{
    u64 bitmap = wall_mask_;
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

    if ((block_cell_mask(moved_b) & wall_mask_) != 0) {
        return std::nullopt;
    }

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

auto puzzle::explore_state_space_with_indices(const threadpool thread_pool,
                                              exploration_observer* observer) const
    -> state_space_result
{
    constexpr size_t batch_size = 4096;

    state_space_result result;
    auto& state_pool = result.states;
    auto& state_indices = result.indices;
    auto& links = result.links;

    // Start with the current state. Indices are assigned in insertion order, so the BFS frontier
    // is always a contiguous range of state_pool.
    state_indices.emplace(*this, 0);
    state_pool.push_back(*this);

    const bool use_parallel = thread_pool.has_value() && *thread_pool != nullptr &&
        (*thread_pool)->get_thread_count() > 1;
    std::vector<std::vector<puzzle>> neighbors;
    // A lower-index-only rule misses an edge when the higher-index endpoint already existed
    // before the lower-index endpoint was processed. Track the undirected edge explicitly so
    // every transition appears exactly once in the graph.
    boost::unordered_flat_set<u64> seen_edges;

    const auto merge_candidate = [&](const puzzle& candidate, const size_t current_idx)
    {
        auto [it, inserted] = state_indices.emplace(candidate, state_pool.size());
        if (inserted) {
            state_pool.push_back(candidate);
        }

        const size_t first = std::min(current_idx, static_cast<size_t>(it->second));
        const size_t second = std::max(current_idx, static_cast<size_t>(it->second));
        if (seen_edges.insert((u64{first} << 32) | static_cast<u64>(second)).second) {
            links.emplace_back(first, second);
        }
    };

    // Process one BFS frontier at a time. A frontier is the range of states that existed before
    // the previous frontier produced new states. Neighbour generation is parallel; merging stays
    // in state order so indices and links are identical to the sequential BFS.
    size_t frontier_begin = 0;
    size_t frontier_end = state_pool.size();
    while (frontier_begin < frontier_end) {
        for (size_t batch_begin = frontier_begin; batch_begin < frontier_end;
             batch_begin += batch_size) {
            const size_t batch_end = std::min(frontier_end, batch_begin + batch_size);
            const size_t batch_count = batch_end - batch_begin;

            if (use_parallel && batch_count >= 64) {
                neighbors.resize(batch_count);
                for (size_t i = 0; i < batch_count; ++i) {
                    auto& node_neighbors = neighbors[i];
                    node_neighbors.clear();
                    const size_t expected = 4 * state_pool[batch_begin + i].block_count();
                    if (node_neighbors.capacity() < expected) {
                        node_neighbors.reserve(expected);
                    }
                }

                (*thread_pool)
                    ->submit_loop(0, batch_count,
                                  [&](const size_t i)
                                  {
                                      state_pool[batch_begin + i].for_each_adjacent(
                                          [&](const puzzle& p) { neighbors[i].push_back(p); });
                                  })
                    .wait();

                for (size_t i = 0; i < batch_count; ++i) {
                    const size_t current_idx = batch_begin + i;
                    for (const puzzle& p : neighbors[i]) {
                        merge_candidate(p, current_idx);
                    }
                }
            } else {
                for (size_t current_idx = batch_begin; current_idx < batch_end; ++current_idx) {
                    const puzzle current = state_pool[current_idx];
                    current.for_each_adjacent([&](const puzzle& p)
                                              { merge_candidate(p, current_idx); });
                }
            }

            if (observer != nullptr && !observer->on_states_explored(state_pool.size())) {
                result.cancelled = true;
                return result;
            }
        }

        frontier_begin = frontier_end;
        frontier_end = state_pool.size();
    }

    return result;
}

auto puzzle::explore_state_space(const threadpool thread_pool) const
    -> std::pair<std::vector<puzzle>, std::vector<spring>>
{
    state_space_result result = explore_state_space_with_indices(thread_pool);
    return {std::move(result.states), std::move(result.links)};
}

auto puzzle::get_cluster_id_and_solution(exploration_observer* observer) const
    -> cluster_exploration
{
    state_space_result explored = explore_state_space_with_indices(std::nullopt, observer);

    cluster_exploration result;
    result.states = std::move(explored.states);
    result.cancelled = explored.cancelled;

    if (result.states.empty()) {
        result.cluster_id = *this;
        result.hardest = *this;
        return result;
    }

    std::vector<size_t> solutions;
    result.cluster_id = result.states[0];
    for (size_t i = 0; i < result.states.size(); ++i) {
        if (result.states[i] < result.cluster_id) {
            result.cluster_id = result.states[i];
        }
        if (result.states[i].goal_reached()) {
            solutions.emplace_back(i);
        }
    }

    result.hardest = result.cluster_id;
    if (result.cancelled) {
        return result;
    }
    if (solutions.empty()) {
        return result;
    }

    // TODO: We only need to search until a distance equal to min_moves is found.
    //       Maybe DFS instead of BFS could even be faster on average?
    // TODO: Split moves from solvable calculation. We only need to count the moves if the cluster
    // is solvable at all.
    graph_distances distances;
    distances.calculate_distances(result.states.size(), explored.links, solutions);

    result.winnable = true;
    int max_distance = 0;
    size_t hardest_index = 0;
    for (size_t i = 0; i < distances.distances.size(); ++i) {
        if (distances.distances[i] > max_distance) {
            max_distance = distances.distances[i];
            hardest_index = i;
        }
    }

    result.max_moves = max_distance;
    result.hardest = result.states[hardest_index];
    return result;
}

auto puzzle::generate_block_sets(const puzzle& p, const blockset2& permitted_blocks,
                                 const block target_block, const u8 max_blocks,
                                 const std::stop_token stop) -> std::vector<blockmap2<u8>>
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
        max_counts[b] =
            std::min(max_blocks, static_cast<u8>((board_area - target_area) / block_area));
    }

    blockmap2<u8> current_set;
    while (true) {
        if (stop.stop_requested()) {
            break;
        }

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

auto puzzle::generate_initial_puzzles(const puzzle& p, const block target_block,
                                      const std::tuple<u8, u8, u8, u8>& target_block_pos_range,
                                      const std::stop_token stop) -> std::vector<puzzle>
{
    std::vector<puzzle> result;

    const auto [txs, tys, txe, tye] = target_block_pos_range;

    for (int tx = txs; tx <= txe; ++tx) {
        for (int ty = tys; ty <= tye; ++ty) {
            if (stop.stop_requested()) {
                return result;
            }

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

            result.emplace_back(p.repr.cooked.meta, blocks, p.wall_mask_);
        }
    }

    return result;
}
