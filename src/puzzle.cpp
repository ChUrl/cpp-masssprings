#include "puzzle.hpp"

#include <optional>
#include <unordered_set>

#ifdef TRACY
    #include <tracy/Tracy.hpp>
#endif

auto puzzle::block::hash() const -> size_t
{
    const std::string s = std::format("{},{},{},{}", x, y, width, height);
    return std::hash<std::string>{}(s);
}

auto puzzle::block::valid() const -> bool
{
    return width > 0 && height > 0 && x >= 0 && x + width <= MAX_WIDTH && y >= 0 && y + height <= MAX_HEIGHT;
}

auto puzzle::block::string() const -> std::string
{
    if (target) {
        return std::format("{}{}", static_cast<char>(width + static_cast<int>('a') - 1),
                           static_cast<char>(height + static_cast<int>('a') - 1));
    }
    if (immovable) {
        return std::format("{}{}", static_cast<char>(width + static_cast<int>('A') - 1),
                           static_cast<char>(height + static_cast<int>('A') - 1));
    }
    return std::format("{}{}", width, height);
}

auto puzzle::block::principal_dirs() const -> int
{
    if (immovable) {
        return 0;
    }

    if (width > height) {
        return eas | wes;
    }
    if (height > width) {
        return nor | sou;
    }
    return nor | eas | sou | wes;
}

auto puzzle::block::covers(const int _x, const int _y) const -> bool
{
    return _x >= x && _x < x + width && _y >= y && _y < y + height;
}

auto puzzle::block::collides(const block& b) const -> bool
{
    return x < b.x + b.width && x + width > b.x && y < b.y + b.height && y + height > b.y;
}

auto puzzle::get_index(const int x, const int y) const -> int
{
    if (x < 0 || x >= width || y < 0 || y >= height) {
        errln("Trying to calculating index of invalid board coordinates ({}, {})", x, y);
        exit(1);
    }
    return PREFIX + (y * width + x) * 2;
}

auto puzzle::hash() const -> size_t
{
    return std::hash<std::string>{}(state);
}

auto puzzle::has_win_condition() const -> bool
{
    return target_x != MAX_WIDTH && target_y != MAX_HEIGHT;
}

auto puzzle::won() const -> bool
{
    const std::optional<block>& b = try_get_target_block();
    return has_win_condition() && b && b->x == target_x && b->y == target_y;
}

auto puzzle::valid() const -> bool
{
    return width >= MIN_WIDTH && width <= MAX_WIDTH && height >= MIN_HEIGHT && height <= MAX_HEIGHT;
}

auto puzzle::try_get_invalid_reason() const -> std::optional<std::string>
{
    const std::optional<block>& b = try_get_target_block();
    if (has_win_condition() && !b) {
        return "Goal Without Target";
    }
    if (!has_win_condition() && b) {
        return "Target Without Goal";
    }

    if (has_win_condition() && b && restricted) {
        const int dirs = b->principal_dirs();
        if ((dirs & nor && b->x != target_x) || (dirs & eas && b->y != target_y)) {
            return "Goal Unreachable";
        }
    }

    if (target_x > 0 && target_x + b->width < width && target_y > 0 && target_y + b->height < height) {
        return "Goal Inside";
    }

    infoln("Validating puzzle {}", state);

    if (static_cast<int>(state.length()) != width * height * 2 + PREFIX) {
        infoln("Puzzle invalid: Representation length {} doesn't match {}x{} board", state.length(),
               width, height);
        return "Invalid Repr Length";
    }

    // Check prefix
    if (!std::string("FR").contains(state[0])) {
        infoln("Puzzle invalid: Representation[0] {} doesn't match [FR]", state[0]);
        return "Invalid Restricted Repr";
    }
    if (restricted && state[0] != 'R') {
        infoln("Puzzle invalid: Representation[0] {} doesn't match restricted={}", state[0],
               restricted);
        return "Restricted != Restricted Repr";
    }
    if (!std::string("0123456789").contains(state[1]) ||
        !std::string("0123456789").contains(state[2])) {
        infoln("Puzzle invalid: Representation[1/2] {}/{} doesn't match [3-9]", state[1], state[2]);
        return "Invalid Dims Repr";
    }
    if (std::stoi(state.substr(1, 1)) != width || std::stoi(state.substr(2, 1)) != height) {
        infoln("Puzzle invalid: Representation[1/2] {}/{} doesn't match {}x{} board", state[1],
               state[2], width, height);
        return "Dims != Dims Repr";
    }
    if (!std::string("0123456789").contains(state[3]) ||
        !std::string("0123456789").contains(state[4])) {
        infoln("Puzzle invalid: Representation[3/4] {}/{} doesn't match [1-9]", state[3], state[4]);
        return "Invalid Goal Repr";
    }
    if (std::stoi(state.substr(3, 1)) != target_x || std::stoi(state.substr(4, 1)) != target_y) {
        infoln("Puzzle invalid: Representation[3/4] {}/{} doesn't match target ({}, {})", state[3],
               state[4], target_x, target_y);
        return "Goal != Goal Repr";
    }

    // Check blocks
    const std::string allowed_chars = ".123456789abcdefghiABCDEFGHI";
    for (const char c : state.substr(PREFIX, state.length() - PREFIX)) {
        if (!allowed_chars.contains(c)) {
            infoln("Puzzle invalid: Block {} has invalid character", c);
            return "Invalid Block Repr";
        }
    }

    const bool success =
        width >= MIN_WIDTH && width <= MAX_WIDTH && height >= MIN_HEIGHT && height <= MAX_HEIGHT;

    if (!success) {
        infoln("Puzzle invalid: Board size {}x{} not in range [3-9]", width, height);
        return "Invalid Dims";
    } else {
        return std::nullopt;
    }
}

auto puzzle::try_get_block(const int x, const int y) const -> std::optional<block>
{
    if (!covers(x, y)) {
        return std::nullopt;
    }

    for (const block& b : *this) {
        if (b.covers(x, y)) {
            return b;
        }
    }

    return std::nullopt;
}

auto puzzle::try_get_target_block() const -> std::optional<block>
{
    for (const block b : *this) {
        if (b.target) {
            return b;
        }
    }

    return std::nullopt;
}

auto puzzle::covers(const int x, const int y, const int w, const int h) const -> bool
{
    return x >= 0 && x + w <= width && y >= 0 && y + h <= height;
}

auto puzzle::covers(const int x, const int y) const -> bool
{
    return covers(x, y, 1, 1);
}

auto puzzle::covers(const block& b) const -> bool
{
    return covers(b.x, b.y, b.width, b.height);
}

auto puzzle::try_toggle_restricted() const -> std::optional<puzzle>
{
    puzzle p = *this;
    p.restricted = !restricted;
    p.state.replace(0, 1, p.restricted ? "R" : "F");
    return p;
}

auto puzzle::try_set_goal(const int x, const int y) const -> std::optional<puzzle>
{
    const std::optional<block>& b = try_get_target_block();
    if (!b || !covers(x, y, b->width, b->height)) {
        return std::nullopt;
    }

    puzzle p = *this;
    if (target_x == x && target_y == y) {
        p.target_x = MAX_WIDTH;
        p.target_y = MAX_HEIGHT;
    } else {
        p.target_x = x;
        p.target_y = y;
    }

    p.state.replace(3, 1, std::format("{}", p.target_x));
    p.state.replace(4, 1, std::format("{}", p.target_y));

    return p;
}

auto puzzle::try_clear_goal() const -> std::optional<puzzle>
{
    puzzle p = *this;
    p.target_x = MAX_WIDTH;
    p.target_y = MAX_HEIGHT;
    p.state.replace(3, 2, std::format("{}{}", MAX_WIDTH, MAX_HEIGHT));
    return p;
}

auto puzzle::try_add_column() const -> std::optional<puzzle>
{
    if (width >= MAX_WIDTH) {
        return std::nullopt;
    }

    puzzle p = {width + 1, height, restricted};

    // Non-fitting blocks won't be added
    for (const block& b : *this) {
        if (const std::optional<puzzle>& _p = p.try_add_block(b)) {
            p = *_p;
        }
    }

    return p;
}

auto puzzle::try_remove_column() const -> std::optional<puzzle>
{
    if (width <= MIN_WIDTH) {
        return std::nullopt;
    }

    puzzle p = {width - 1, height, restricted};

    // Non-fitting blocks won't be added
    for (const block& b : *this) {
        if (const std::optional<puzzle>& _p = p.try_add_block(b)) {
            p = *_p;
        }
    }

    return p;
}

auto puzzle::try_add_row() const -> std::optional<puzzle>
{
    if (height >= 9) {
        return std::nullopt;
    }

    puzzle p = puzzle(width, height + 1, restricted);

    for (const block& b : *this) {
        if (const std::optional<puzzle>& _p = p.try_add_block(b)) {
            p = *_p;
        }
    }

    return p;
}

auto puzzle::try_remove_row() const -> std::optional<puzzle>
{
    if (height == 0) {
        return std::nullopt;
    }

    puzzle p = puzzle(width, height - 1, restricted);

    for (const block& b : *this) {
        if (const std::optional<puzzle>& _p = p.try_add_block(b)) {
            p = *_p;
        }
    }

    return p;
}

auto puzzle::try_add_block(const block& b) const -> std::optional<puzzle>
{
    if (!covers(b)) {
        return std::nullopt;
    }

    for (block _b : *this) {
        if (_b.collides(b)) {
            return std::nullopt;
        }
    }

    puzzle p = *this;
    const int index = get_index(b.x, b.y);
    p.state.replace(index, 2, b.string());

    return p;
}

auto puzzle::try_remove_block(const int x, const int y) const -> std::optional<puzzle>
{
    const std::optional<block>& b = try_get_block(x, y);
    if (!b) {
        return std::nullopt;
    }

    puzzle p = *this;
    const int index = get_index(b->x, b->y);
    p.state.replace(index, 2, "..");

    return p;
}

auto puzzle::try_toggle_target(const int x, const int y) const -> std::optional<puzzle>
{
    std::optional<block> b = try_get_block(x, y);
    if (!b || b->immovable) {
        return std::nullopt;
    }

    // Remove the current target if it exists
    puzzle p = *this;
    if (const std::optional<block>& _b = try_get_target_block()) {
        const int index = get_index(_b->x, _b->y);
        p.state.replace(index, 2, block(_b->x, _b->y, _b->width, _b->height, false).string());
    }

    // Add the new target
    b->target = !b->target;
    const int index = get_index(b->x, b->y);
    p.state.replace(index, 2, b->string());

    return p;
}

auto puzzle::try_toggle_wall(const int x, const int y) const -> std::optional<puzzle>
{
    std::optional<block> b = try_get_block(x, y);
    if (!b || b->target) {
        return std::nullopt;
    }

    // Add the new target
    puzzle p = *this;
    b->immovable = !b->immovable;
    const int index = get_index(b->x, b->y);
    p.state.replace(index, 2, b->string());

    return p;
}

auto puzzle::try_move_block_at(const int x, const int y, const direction dir) const
    -> std::optional<puzzle>
{
    const std::optional<block>& b = try_get_block(x, y);
    if (!b || b->immovable) {
        return std::nullopt;
    }

    const int dirs = restricted ? b->principal_dirs() : nor | eas | sou | wes;

    // Get target block
    int _target_x = b->x;
    int _target_y = b->y;
    switch (dir) {
    case nor:
        if (!(dirs & nor) || _target_y < 1) {
            return std::nullopt;
        }
        _target_y--;
        break;
    case eas:
        if (!(dirs & eas) || _target_x + b->width >= width) {
            return std::nullopt;
        }
        _target_x++;
        break;
    case sou:
        if (!(dirs & sou) || _target_y + b->height >= height) {
            return std::nullopt;
        }
        _target_y++;
        break;
    case wes:
        if (!(dirs & wes) || _target_x < 1) {
            return std::nullopt;
        }
        _target_x--;
        break;
    }
    const block moved_b = block(_target_x, _target_y, b->width, b->height, b->target);

    // Check collisions
    for (const block& _b : *this) {
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

auto puzzle::find_adjacent_puzzles() const -> std::vector<puzzle>
{
    std::vector<puzzle> puzzles;

    for (const block& b : *this) {
        if (b.immovable) {
            continue;
        }

        const int dirs = restricted ? b.principal_dirs() : nor | eas | sou | wes;

        if (dirs & nor) {
            if (const std::optional<puzzle>& north = try_move_block_at(b.x, b.y, nor)) {
                puzzles.push_back(*north);
            }
        }

        if (dirs & eas) {
            if (const std::optional<puzzle>& east = try_move_block_at(b.x, b.y, eas)) {
                puzzles.push_back(*east);
            }
        }

        if (dirs & sou) {
            if (const std::optional<puzzle>& south = try_move_block_at(b.x, b.y, sou)) {
                puzzles.push_back(*south);
            }
        }

        if (dirs & wes) {
            if (const std::optional<puzzle>& west = try_move_block_at(b.x, b.y, wes)) {
                puzzles.push_back(*west);
            }
        }
    }

    // for (const puzzle& p : puzzles) {
    //     println("Adjacent puzzle: {}", p.state);
    // }

    return puzzles;
}

auto puzzle::explore_state_space() const
    -> std::pair<std::vector<puzzle>, std::vector<std::pair<size_t, size_t>>>
{
#ifdef TRACY
    ZoneScoped;
#endif

    // infoln("Exploring state space, this might take a while...");

    std::vector<puzzle> state_pool;
    std::unordered_map<puzzle, std::size_t> state_indices; // Helper to construct the links vector
    std::vector<std::pair<size_t, std::size_t>> links;

    // Buffer for all states we want to call GetNextStates() on
    std::unordered_set<puzzle> remaining_states;
    remaining_states.insert(*this);

    do {
        const puzzle current = *remaining_states.begin();
        remaining_states.erase(current);

        if (!state_indices.contains(current)) {
            state_indices.emplace(current, state_pool.size());
            state_pool.push_back(current);
        }

        for (const puzzle& s : current.find_adjacent_puzzles()) {
            if (!state_indices.contains(s)) {
                remaining_states.insert(s);
                state_indices.emplace(s, state_pool.size());
                state_pool.push_back(s);
            }
            links.emplace_back(state_indices.at(current), state_indices.at(s));
        }
    } while (!remaining_states.empty());

    infoln("State space has size {} with {} transitions.", state_pool.size(), links.size());

    return std::make_pair(state_pool, links);
}