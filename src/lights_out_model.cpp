#include "lights_out_model.hpp"

#include <bit>
#include <format>
#include <random>
#include <utility>

namespace
{
auto wrap_coord(int value, const int size) -> int
{
    value %= size;
    if (value < 0) {
        value += size;
    }
    return value;
}
} // namespace

lights_out_model::lights_out_model()
{
    configure(lights_out_rules{});
}

lights_out_model::lights_out_model(const lights_out_rules& rules)
{
    configure(rules);
}

auto lights_out_model::configure(const lights_out_rules& rules) -> void
{
    rules_ = rules;
    if (rules_.name.empty()) {
        rules_.name = rules_.is_classic() ? "Lights Out 4x4" : "Lights Out Custom";
    }

    build_tables();
    states.clear();
    indices.clear();
}

auto lights_out_model::rules() const -> const lights_out_rules&
{
    return rules_;
}

auto lights_out_model::build_tables() -> void
{
    const int cells = rules_.cell_count();
    const int width = rules_.width;
    const int height = rules_.height;

    powers.assign(static_cast<size_t>(cells) + 1, 1);
    for (int i = 1; i <= cells; ++i) {
        powers[static_cast<size_t>(i)] =
            powers[static_cast<size_t>(i - 1)] * static_cast<std::uint64_t>(rules_.base);
    }

    move_deltas.assign(static_cast<size_t>(cells) * static_cast<size_t>(cells), 0);
    allowed_cells.clear();

    const bool wrap = rules_.neighborhood == lights_out_neighborhood::orthogonal_wrap ||
        rules_.neighborhood == lights_out_neighborhood::diagonal_wrap;
    const bool diagonal = rules_.neighborhood == lights_out_neighborhood::diagonal ||
        rules_.neighborhood == lights_out_neighborhood::diagonal_wrap;

    const auto set_delta = [&](const int cell, const int cx, const int cy)
    {
        int x = cx;
        int y = cy;
        if (wrap) {
            x = wrap_coord(x, width);
            y = wrap_coord(y, height);
        }
        if (x < 0 || x >= width || y < 0 || y >= height) {
            return;
        }
        move_deltas[static_cast<size_t>(cell) * static_cast<size_t>(cells) +
                    static_cast<size_t>(y * width + x)] = 1;
    };

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (!rules_.is_pressable(x, y)) {
                continue;
            }

            const int cell = y * width + x;
            allowed_cells.push_back(cell);

            set_delta(cell, x, y);
            if (diagonal) {
                set_delta(cell, x - 1, y - 1);
                set_delta(cell, x + 1, y - 1);
                set_delta(cell, x - 1, y + 1);
                set_delta(cell, x + 1, y + 1);
            } else {
                set_delta(cell, x - 1, y);
                set_delta(cell, x + 1, y);
                set_delta(cell, x, y - 1);
                set_delta(cell, x, y + 1);
            }
        }
    }
}

auto lights_out_model::state_count() const -> size_t
{
    return states.size();
}

auto lights_out_model::memory_bytes() const -> size_t
{
    constexpr size_t entry_overhead = 32;
    return states.capacity() * sizeof(std::uint64_t) +
        indices.size() * (sizeof(std::uint64_t) + sizeof(size_t) + entry_overhead) +
        powers.capacity() * sizeof(std::uint64_t) + move_deltas.capacity() * sizeof(std::uint8_t) +
        allowed_cells.capacity() * sizeof(int) + rules_.name.capacity();
}

auto lights_out_model::is_goal(const state_id id) const -> bool
{
    return code(id) == rules_.target;
}

auto lights_out_model::is_valid(const state_id id) const -> bool
{
    if (id >= states.size()) {
        return false;
    }

    std::uint64_t state = states[id];
    for (int i = 0; i < rules_.cell_count(); ++i) {
        state /= static_cast<std::uint64_t>(rules_.base);
    }
    return state == 0;
}

auto lights_out_model::describe(const state_id id) const -> std::string
{
    const std::string state = state_string(code(id));

    if (rules_.is_classic()) {
        return std::format("LO:[{}x{}] B:[{}]", rules_.width, rules_.height, state);
    }

    return std::format("LO:[{}x{}] V:[{}] N:[{}] P:[{}] T:[{}] B:[{}]", rules_.width, rules_.height,
                       rules_.base, rules_.neighborhood_name(), pressable_string(),
                       state_string(rules_.target), state);
}

auto lights_out_model::initial_state() -> state_id
{
    if (states.empty()) {
        return find_or_insert(all_on_code());
    }
    return 0;
}

auto lights_out_model::reset_to(const state_id id) -> state_id
{
    const std::uint64_t state = code(id);
    states.clear();
    indices.clear();
    return find_or_insert(state);
}

auto lights_out_model::clone() const -> std::unique_ptr<state_model>
{
    return std::make_unique<lights_out_model>(*this);
}

auto lights_out_model::explore(const state_id start, const state_id current, const threadpool pool,
                               exploration_observer* observer) -> exploration_result
{
    (void)pool;

    const std::uint64_t start_code = code(start);
    const std::uint64_t current_code = code(current);

    states.clear();
    indices.clear();

    const state_id start_id = insert_code(start_code).first;

    std::vector<state_id> queue;
    queue.reserve(1u << 16);
    queue.push_back(start_id);

    std::vector<spring> links;
    links.reserve(1u << 18);

    size_t head = 0;
    while (head < queue.size()) {
        if (observer != nullptr && (head & 0xFFFu) == 0 &&
            !observer->on_states_explored(queue.size())) {
            exploration_result result;
            result.links = std::move(links);
            result.start = start_id;
            result.current = start_id;
            result.complete = false;
            result.cancelled = true;
            return result;
        }

        const state_id id = queue[head++];
        const std::uint64_t state = states[id];

        for (const int cell : allowed_cells) {
            const std::uint64_t next_state = apply_press(state, cell);
            const auto [next_id, inserted] = insert_code(next_state);
            if (inserted) {
                queue.push_back(next_id);
            }

            if (directed_edges()) {
                links.emplace_back(id, next_id);
            } else if (id < next_id) {
                links.emplace_back(id, next_id);
            }
        }
    }

    // Insert the current state only after the BFS. Inserting it before the BFS would mark it as
    // visited, so BFS would never expand its outgoing transitions.
    const state_id current_id = insert_code(current_code).first;

    exploration_result result;
    result.links = std::move(links);
    result.start = start_id;
    result.current = current_id;
    result.complete = true;
    result.directed = directed_edges();
    return result;
}

auto lights_out_model::directed_edges() const -> bool
{
    return rules_.base == 3;
}

auto lights_out_model::code(const state_id id) const -> std::uint64_t
{
    return states.at(id);
}

auto lights_out_model::mask(const state_id id) const -> std::uint16_t
{
    return static_cast<std::uint16_t>(code(id));
}

auto lights_out_model::value(const state_id id, const int cell) const -> int
{
    if (cell < 0 || cell >= rules_.cell_count()) {
        return 0;
    }
    return static_cast<int>((code(id) / powers[static_cast<size_t>(cell)]) %
                            static_cast<std::uint64_t>(rules_.base));
}

auto lights_out_model::on_count(const state_id id) const -> int
{
    int count = 0;
    const std::uint64_t state = code(id);
    for (int i = 0; i < rules_.cell_count(); ++i) {
        if ((state / powers[static_cast<size_t>(i)]) % static_cast<std::uint64_t>(rules_.base) !=
            0) {
            ++count;
        }
    }
    return count;
}

auto lights_out_model::find_or_insert(const std::uint64_t state) -> state_id
{
    return insert_code(state).first;
}

auto lights_out_model::try_toggle(const state_id id, const int x, const int y) -> state_id
{
    if (!rules_.is_pressable(x, y)) {
        return id;
    }
    return find_or_insert(apply_press(code(id), y * rules_.width + x));
}

auto lights_out_model::random_state() -> state_id
{
    if (allowed_cells.empty()) {
        return solved_state();
    }

    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> cell_distribution(
        0, static_cast<int>(allowed_cells.size()) - 1);
    std::uniform_int_distribution<int> count_distribution(8, 24);

    std::uint64_t state = rules_.target;
    const int presses = count_distribution(rng);
    for (int i = 0; i < presses; ++i) {
        state = apply_press(state, allowed_cells[static_cast<size_t>(cell_distribution(rng))]);
    }

    if (state == rules_.target) {
        state = apply_press(state, allowed_cells[static_cast<size_t>(cell_distribution(rng))]);
    }

    return find_or_insert(state);
}

auto lights_out_model::solved_state() -> state_id
{
    return find_or_insert(rules_.target);
}

auto lights_out_model::all_on_code() const -> std::uint64_t
{
    const size_t cells = static_cast<size_t>(rules_.cell_count());
    if (powers.size() > cells) {
        return powers[cells] - 1;
    }
    return 0;
}

auto lights_out_model::toggle_mask(const int x, const int y) -> std::uint16_t
{
    if (x < 0 || x >= SIZE || y < 0 || y >= SIZE) {
        return 0;
    }

    std::uint16_t mask = 0;
    const auto set = [&mask](const int cx, const int cy)
    {
        if (cx >= 0 && cx < SIZE && cy >= 0 && cy < SIZE) {
            mask ^= static_cast<std::uint16_t>(1u << (cy * SIZE + cx));
        }
    };

    set(x, y);
    set(x - 1, y);
    set(x + 1, y);
    set(x, y - 1);
    set(x, y + 1);

    return mask;
}

auto lights_out_model::apply_press(const std::uint64_t state, const int cell) const -> std::uint64_t
{
    const int cells = rules_.cell_count();
    const std::uint64_t base = static_cast<std::uint64_t>(rules_.base);
    const size_t offset = static_cast<size_t>(cell) * static_cast<size_t>(cells);

    std::uint64_t result = 0;
    for (int i = 0; i < cells; ++i) {
        const std::uint64_t power = powers[static_cast<size_t>(i)];
        int digit = static_cast<int>((state / power) % base);
        digit = (digit + move_deltas[offset + static_cast<size_t>(i)]) % rules_.base;
        result += static_cast<std::uint64_t>(digit) * power;
    }
    return result;
}

auto lights_out_model::state_string(const std::uint64_t state) const -> std::string
{
    std::string result;
    result.reserve(static_cast<size_t>(rules_.cell_count()));

    std::uint64_t remaining = state;
    for (int i = 0; i < rules_.cell_count(); ++i) {
        result.push_back(
            static_cast<char>('0' + remaining % static_cast<std::uint64_t>(rules_.base)));
        remaining /= static_cast<std::uint64_t>(rules_.base);
    }

    return result;
}

auto lights_out_model::pressable_string() const -> std::string
{
    std::string result;
    result.reserve(static_cast<size_t>(rules_.cell_count()));
    for (int i = 0; i < rules_.cell_count(); ++i) {
        result.push_back((rules_.pressable_mask & (1u << i)) != 0 ? '1' : '0');
    }
    return result;
}

auto lights_out_model::insert_code(const std::uint64_t state) -> std::pair<state_id, bool>
{
    if (const auto it = indices.find(state); it != indices.end()) {
        return {it->second, false};
    }

    const state_id id = states.size();
    states.push_back(state);
    indices.emplace(state, id);
    return {id, true};
}
