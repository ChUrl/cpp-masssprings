#ifndef PUZZLE_HPP_
#define PUZZLE_HPP_

#include "util.hpp"

#include <array>
#include <cstddef>
#include <format>
#include <functional>
#include <string>
#include <vector>

enum direction
{
    nor = 1 << 0,
    eas = 1 << 1,
    sou = 1 << 2,
    wes = 1 << 3,
};

// A state is represented by a string "MWHXYblocks", where M is "R"
// (restricted) or "F" (free), W is the board width, H is the board height, X
// is the target block x goal, Y is the target block y goal and blocks is an
// enumeration of each of the board's cells, with each cell being a 2-letter
// or 2-digit block representation (a 3x3 board would have a string
// representation with length 5 + 3*3 * 2). The board's cells are enumerated
// from top-left to bottom-right with each block's pivot being its top-left
// corner.
class puzzle
{
public:
    // A block is represented as a 2-digit string "wh", where w is the block width
    // and h the block height.
    // The target block (to remove from the board) is represented as a 2-letter
    // lower-case string "xy", where x is the block width and y the block height and
    // width/height are represented by [abcdefghi] (~= [123456789]).
    // Immovable blocks are represented as a 2-letter upper-case string "XY", where
    // X is the block width and Y the block height and width/height are represented
    // by [ABCDEFGHI] (~= [123456789]).
    class block
    {
    public:
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        bool target = false;
        bool immovable = false;

    public:
        block(const int _x, const int _y, const int _width, const int _height, const bool _target = false,
              const bool _immovable = false)
            : x(_x), y(_y), width(_width), height(_height), target(_target), immovable(_immovable)
        {
            if (_x < 0 || _x + _width > 9 || _y < 0 || _y + _height > 9) {
                errln("Block must fit in a 9x9 board!");
                exit(1);
            }
        }

        block(const int _x, const int _y, const std::string& b) : x(_x), y(_y)
        {
            if (b.length() != 2) {
                errln("Block representation must have length 2!");
                exit(1);
            }

            if (b == "..") {
                this->x = 0;
                this->y = 0;
                width = 0;
                height = 0;
                target = false;
                return;
            }

            target = false;
            constexpr std::array<char, 9> target_chars{'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i'};
            for (const char c : target_chars) {
                if (b.contains(c)) {
                    target = true;
                    break;
                }
            }

            immovable = false;
            constexpr std::array<char, 9> immovable_chars{'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I'};
            for (const char c : immovable_chars) {
                if (b.contains(c)) {
                    immovable = true;
                    break;
                }
            }

            if (target) {
                width = static_cast<int>(b.at(0)) - static_cast<int>('a') + 1;
                height = static_cast<int>(b.at(1)) - static_cast<int>('a') + 1;
            } else if (immovable) {
                width = static_cast<int>(b.at(0)) - static_cast<int>('A') + 1;
                height = static_cast<int>(b.at(1)) - static_cast<int>('A') + 1;
            } else {
                // println("Parsing block string '{}' at ({}, {})", b, _x, _y);
                width = std::stoi(b.substr(0, 1));
                height = std::stoi(b.substr(1, 1));
            }

            if (_x < 0 || _x + width > 9 || _y < 0 || _y + height > 9) {
                errln("Block must fit in a 9x9 board!");
                exit(1);
            }
        }

        block() = delete;

        auto operator==(const block& other) const -> bool
        {
            return x == other.x && y == other.y && width && other.width && target == other.target &&
                immovable == other.immovable;
        }

        auto operator!=(const block& other) const -> bool
        {
            return !(*this == other);
        }

    public:
        [[nodiscard]] auto hash() const -> size_t;
        [[nodiscard]] auto valid() const -> bool;
        [[nodiscard]] auto string() const -> std::string;

        // Movement

        [[nodiscard]] auto principal_dirs() const -> int;
        [[nodiscard]] auto covers(int _x, int _y) const -> bool;
        [[nodiscard]] auto collides(const block& b) const -> bool;
    };

private:
    // https://en.cppreference.com/w/cpp/iterator/input_iterator.html
    class block_iterator
    {
    public:
        using difference_type = std::ptrdiff_t;
        using value_type = block;

    private:
        const puzzle& state;
        int current_pos;

    public:
        explicit block_iterator(const puzzle& _state) : state(_state), current_pos(0)
        {}

        block_iterator(const puzzle& _state, const int _current_pos) : state(_state), current_pos(_current_pos)
        {}

        auto operator*() const -> block
        {
            return {current_pos % state.width, current_pos / state.width,
                    state.state.substr(current_pos * 2 + PREFIX, 2)};
        }

        auto operator++() -> block_iterator&
        {
            do {
                current_pos++;
            } while (current_pos < state.width * state.height &&
                     state.state.substr(current_pos * 2 + PREFIX, 2) == "..");
            return *this;
        }

        auto operator==(const block_iterator& other) const -> bool
        {
            return state == other.state && current_pos == other.current_pos;
        }

        auto operator!=(const block_iterator& other) const -> bool
        {
            return !(*this == other);
        }
    };

public:
    static constexpr int PREFIX = 5;
    static constexpr int MIN_WIDTH = 3;
    static constexpr int MIN_HEIGHT = 3;
    static constexpr int MAX_WIDTH = 9;
    static constexpr int MAX_HEIGHT = 9;

    int width = 0;
    int height = 0;
    int target_x = 9;
    int target_y = 9;
    bool restricted = false; // Only allow blocks to move in their principal direction
    std::string state;

public:
    puzzle() = delete;

    puzzle(const int w, const int h, const int tx, const int ty, const bool r)
        : width(w), height(h), target_x(tx), target_y(ty), restricted(r),
          state(std::format("{}{}{}{}{}{}", r ? "R" : "F", w, h, tx, ty, std::string(w * h * 2, '.')))
    {
        if (w < 1 || w > 9 || h < 1 || h > 9) {
            errln("State width/height must be in [1, 9]!");
            exit(1);
        }
        if (tx < 0 || tx >= 9 || ty < 0 || ty >= 9) {
            if (tx != 9 && ty != 9) {
                errln("State target  must be within the board bounds!");
                exit(1);
            }
        }
    }

    puzzle(const int w, const int h, const bool r) : puzzle(w, h, 9, 9, r)
    {}

    explicit puzzle(const std::string& s)
        : width(std::stoi(s.substr(1, 1))), height(std::stoi(s.substr(2, 1))), target_x(std::stoi(s.substr(3, 1))),
          target_y(std::stoi(s.substr(4, 1))), restricted(s.substr(0, 1) == "R"), state(s)
    {
        if (width < 1 || width > 9 || height < 1 || height > 9) {
            errln("State width/height must be in [1, 9]!");
            exit(1);
        }
        if (target_x < 0 || target_x >= 9 || target_y < 0 || target_y >= 9) {
            if (target_x != 9 && target_y != 9) {
                errln("State target  must be within the board bounds!");
                exit(1);
            }
        }
        if (static_cast<int>(s.length()) != width * height * 2 + PREFIX) {
            errln("State representation must have length width * "
                  "height * 2 + {}!",
                  PREFIX);
            exit(1);
        }
    }

public:
    auto operator==(const puzzle& other) const -> bool
    {
        return state == other.state;
    }

    auto operator!=(const puzzle& other) const -> bool
    {
        return !(*this == other);
    }

    [[nodiscard]] auto begin() const -> block_iterator
    {
        block_iterator it{*this};
        if (!(*it).valid()) {
            ++it;
        }
        return it;
    }

    [[nodiscard]] auto end() const -> block_iterator
    {
        return {*this, width * height};
    }

private:
    [[nodiscard]] auto get_index(int x, int y) const -> int;

public:
    [[nodiscard]] auto hash() const -> size_t;
    [[nodiscard]] auto has_win_condition() const -> bool;
    [[nodiscard]] auto won() const -> bool;
    [[nodiscard]] auto valid() const -> bool;
    [[nodiscard]] auto valid_thorough() const -> bool;

    // Repr helpers

    [[nodiscard]] auto try_get_block(int x, int y) const -> std::optional<block>;
    [[nodiscard]] auto try_get_target_block() const -> std::optional<block>;
    [[nodiscard]] auto covers(int x, int y, int w, int h) const -> bool;
    [[nodiscard]] auto covers(int x, int y) const -> bool;
    [[nodiscard]] auto covers(const block& b) const -> bool;

    // Editing

    [[nodiscard]] auto try_toggle_restricted() const -> std::optional<puzzle>;
    [[nodiscard]] auto try_set_goal(int x, int y) const -> std::optional<puzzle>;
    [[nodiscard]] auto try_clear_goal() const -> std::optional<puzzle>;
    [[nodiscard]] auto try_add_column() const -> std::optional<puzzle>;
    [[nodiscard]] auto try_remove_column() const -> std::optional<puzzle>;
    [[nodiscard]] auto try_add_row() const -> std::optional<puzzle>;
    [[nodiscard]] auto try_remove_row() const -> std::optional<puzzle>;
    [[nodiscard]] auto try_add_block(const block& b) const -> std::optional<puzzle>;
    [[nodiscard]] auto try_remove_block(int x, int y) const -> std::optional<puzzle>;
    [[nodiscard]] auto try_toggle_target(int x, int y) const -> std::optional<puzzle>;
    [[nodiscard]] auto try_toggle_wall(int x, int y) const -> std::optional<puzzle>;

    // Playing

    [[nodiscard]] auto try_move_block_at(int x, int y, direction dir) const -> std::optional<puzzle>;

    // Statespace

    [[nodiscard]] auto find_adjacent_puzzles() const -> std::vector<puzzle>;
    [[nodiscard]] auto explore_state_space() const
        -> std::pair<std::vector<puzzle>, std::vector<std::pair<size_t, size_t>>>;
};

// Provide hash functions so we can use State and <State, State> as hash-set
// keys for masses and springs.
template <>
struct std::hash<puzzle>
{
    auto operator()(const puzzle& s) const noexcept -> size_t
    {
        return s.hash();
    }
};

template <>
struct std::hash<std::pair<puzzle, puzzle>>
{
    auto operator()(const std::pair<puzzle, puzzle>& s) const noexcept -> size_t
    {
        const size_t h1 = std::hash<puzzle>{}(s.first);
        const size_t h2 = std::hash<puzzle>{}(s.second);

        return h1 + h2 + h1 * h2;
        // return (h1 ^ h2) + 0x9e3779b9 + (std::min(h1, h2) << 6) + (std::max(h1, h2) >> 2);
    }
};

template <>
struct std::equal_to<std::pair<puzzle, puzzle>>
{
    auto operator()(const std::pair<puzzle, puzzle>& a, const std::pair<puzzle, puzzle>& b) const noexcept -> bool
    {
        return (a.first == b.first && a.second == b.second) || (a.first == b.second && a.second == b.first);
    }
};

using win_condition = std::function<bool(const puzzle&)>;

#endif
