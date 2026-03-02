#ifndef PUZZLE_HPP_
#define PUZZLE_HPP_

#include "util.hpp"

#include <array>
#include <cstddef>
#include <format>
#include <functional>
#include <ranges>
#include <string>
#include <vector>
#include <bits/fs_fwd.h>

/*
 * 8x8 board
 *  -> 64 (sizes) * 2 (target) * 2 (movable) blocks = 1 Byte
 *
 * 1. Encode the position inside the board (max 64 cells)
 *     -> 64 (slots) * 1 Byte (block) = 64 Byte
 * 2. Encode the position inside the block (max 64 cells)
 *     -> 64 (blocks) * 2 Byte (size + pos) = 128 Byte
 * 3. Encode the position inside the block (with 15 block limit)
 *     -> 15 (blocks) * 2 (block) = 30 Byte
 *
 * Store board size + restricted: +1 Byte
 * Store target position: +1 Byte
 *
 * => Limit to 15 blocks max and use option 3. (4x uint64_t)
 *
 */
class puzzle
{
public:
    /*
     * A block is represented as uint16_t.
     * It stores its position, width, height and if it's the target block or immovable.
     */
    class block
    {
        friend class puzzle;

    private:
        static constexpr uint16_t INVALID = 0x8000;

        static constexpr uint8_t IMMOVABLE_S = 0;
        static constexpr uint8_t IMMOVABLE_E = 0;
        static constexpr uint8_t TARGET_S = 1;
        static constexpr uint8_t TARGET_E = 1;
        static constexpr uint8_t WIDTH_S = 2;
        static constexpr uint8_t WIDTH_E = 4;
        static constexpr uint8_t HEIGHT_S = 5;
        static constexpr uint8_t HEIGHT_E = 7;
        static constexpr uint8_t X_S = 8;
        static constexpr uint8_t X_E = 10;
        static constexpr uint8_t Y_S = 11;
        static constexpr uint8_t Y_E = 13;

        /*
         * Memory layout:
         *
         * 154 321 098 765 432 1 0
         *  B0 YYY XXX HHH WWW T I
         *  ---------- -----------
         *    Byte 1     Byte 0
         *
         * Store the y-position at the most significant bits to obtain a row-major ordering when comparing reprs.
         * The block at (0, 0) will be the smallest, the block at (width, height) the largest,
         * the block at (1, 0) will be smaller than the block at (0, 1), all independent of block size.
         * Then, the block with size (1, 1) will be smaller than the block with size (2, 2),
         * the block with size (1, 0) - horizontal - will be smaller than the block with size (0, 1) - vertical.
         *
         * To mark if a block is empty, the first B bit is set to 1. This is required,
         * since otherwise uint16_t{0} would be a valid block. This also makes empty blocks sorted last.
         */
        uint16_t repr;

    public:
        // Produces an invalid block, for usage with std::array<block, MaxBlocks>
        block()
            : repr(INVALID) {}

        explicit block(const uint16_t _repr)
            : repr(_repr) {}

        block(const int x, const int y, const int w, const int h, const bool t = false, const bool i = false)
            : repr(create_repr(x, y, w, h, t, i))
        {
            if (x < 0 || x + w > MAX_WIDTH || y < 0 || y + h > MAX_HEIGHT) {
                throw std::invalid_argument("Block size out of bounds");
            }
            if (t && i) {
                throw std::invalid_argument("Target block can't be immovable");
            }
        }

        auto operator==(const block other) const -> bool
        {
            return repr == other.repr;
        }

        auto operator!=(const block other) const -> bool
        {
            return repr != other.repr;
        }

        auto operator<(const block other) const -> bool
        {
            return repr < other.repr;
        }

        auto operator<=(const block other) const -> bool
        {
            return repr <= other.repr;
        }

        auto operator>(const block other) const -> bool
        {
            return repr > other.repr;
        }

        auto operator>=(const block other) const -> bool
        {
            return repr >= other.repr;
        }

    private:
        [[nodiscard]] static auto create_repr(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool t = false,
                                              bool i = false) -> uint16_t;

        // Repr setters
        [[nodiscard]] auto set_x(uint8_t x) const -> block;
        [[nodiscard]] auto set_y(uint8_t y) const -> block;
        [[nodiscard]] auto set_width(uint8_t width) const -> block;
        [[nodiscard]] auto set_height(uint8_t height) const -> block;
        [[nodiscard]] auto set_target(bool target) const -> block;
        [[nodiscard]] auto set_immovable(bool immovable) const -> block;

    public:
        [[nodiscard]] auto unpack_repr() const -> std::tuple<uint8_t, uint8_t, uint8_t, uint8_t, bool, bool>;

        // Repr getters
        [[nodiscard]] auto get_x() const -> uint8_t;
        [[nodiscard]] auto get_y() const -> uint8_t;
        [[nodiscard]] auto get_width() const -> uint8_t;
        [[nodiscard]] auto get_height() const -> uint8_t;
        [[nodiscard]] auto get_target() const -> bool;
        [[nodiscard]] auto get_immovable() const -> bool;

        // Util
        [[nodiscard]] auto valid() const -> bool;
        [[nodiscard]] auto principal_dirs() const -> uint8_t;
        [[nodiscard]] auto covers(int _x, int _y) const -> bool;
        [[nodiscard]] auto collides(block b) const -> bool;
    };

public:
    static constexpr uint8_t MAX_BLOCKS = 15;
    static constexpr uint8_t MIN_WIDTH = 3;
    static constexpr uint8_t MIN_HEIGHT = 3;
    static constexpr uint8_t MAX_WIDTH = 8;
    static constexpr uint8_t MAX_HEIGHT = 8;

private:
    static constexpr uint16_t INVALID = 0x8000;

    static constexpr uint8_t RESTRICTED_S = 0;
    static constexpr uint8_t RESTRICTED_E = 0;
    static constexpr uint8_t GOAL_X_S = 1;
    static constexpr uint8_t GOAL_X_E = 3;
    static constexpr uint8_t GOAL_Y_S = 4;
    static constexpr uint8_t GOAL_Y_E = 6;
    static constexpr uint8_t WIDTH_S = 7;
    static constexpr uint8_t WIDTH_E = 9;
    static constexpr uint8_t HEIGHT_S = 10;
    static constexpr uint8_t HEIGHT_E = 12;
    static constexpr uint8_t GOAL_S = 13;
    static constexpr uint8_t GOAL_E = 13;

    struct repr_cooked
    {
        /*
         * Memory layout:
         *
         * 1543 210 98 7 654 321 0
         *  B0G HHH WW W YYY XXX R
         *  ---------- -----------
         *    Byte 1     Byte 0
         *
         * To mark if a puzzle is empty, the first B bit is set to 1.
         * An extra bit is used to mark if the board has a goal, because we can't store MAX_WIDTH=8 in 3 bits.
         */
        uint16_t meta;

        // NOTE: For the hashes to work, this array needs to be sorted always.
        // NOTE: This array might contain empty blocks at the end. The iterator handles this.
        std::array<uint16_t, MAX_BLOCKS> blocks;

        // repr_cooked() = delete;
        // repr_cooked(const repr_cooked& copy) = delete;
        // repr_cooked(repr_cooked&& move) = delete;
    } __attribute__((packed));

    /**
     * With gcc, were allowed to acces the members arbitrarily, even if they're not active (not the ones last written):
     * - https://gcc.gnu.org/onlinedocs/gcc-4.7.1/gcc/Structures-unions-enumerations-and-bit_002dfields-implementation.html#Structures-unions-enumerations-and-bit_002dfields-implementation
     * - https://gcc.gnu.org/onlinedocs/gcc-4.7.1/gcc/Optimize-Options.html#Type_002dpunning
     */
    union repr_u
    {
        // The representation split into meta information and the blocks array
        repr_cooked cooked;

        // For 15 blocks, we have sizeof(meta) + blocks.size() * sizeof(block) = 2 + 15 * 2 = 32 Bytes
        std::array<uint64_t, 4> raw;
    };

    repr_u repr;

public:
    // Produces an invalid puzzle, for usage with containers
    puzzle()
        : repr(repr_cooked{INVALID, invalid_blocks()}) {}

    explicit puzzle(const uint16_t meta)
        : repr(repr_cooked{meta, invalid_blocks()}) {}

    // NOTE: This constructor does not sort the blocks and is only for state space generation
    puzzle(const std::tuple<uint8_t, uint8_t, uint8_t, uint8_t, bool, bool>& meta,
           const std::array<uint16_t, MAX_BLOCKS>& sorted_blocks)
        : repr(repr_cooked{create_meta(meta), sorted_blocks}) {}

    puzzle(const uint64_t byte_0, const uint64_t byte_1, const uint64_t byte_2, const uint64_t byte_3)
        : repr(create_repr(byte_0, byte_1, byte_2, byte_3)) {}

    puzzle(const uint16_t meta, const std::array<uint16_t, MAX_BLOCKS>& blocks)
        : repr(repr_cooked{meta, blocks}) {}

    puzzle(const uint8_t w, const uint8_t h, const uint8_t tx, const uint8_t ty, const bool r, const bool g)
        : repr(create_repr(w, h, tx, ty, r, g, invalid_blocks()))
    {
        if (w < MIN_WIDTH || w > MAX_WIDTH || h < MIN_HEIGHT || h > MAX_HEIGHT) {
            throw std::invalid_argument("Board size out of bounds");
        }
        if (tx >= MAX_WIDTH || ty >= MAX_HEIGHT) {
            throw std::invalid_argument("Goal out of bounds");
        }
    }

    puzzle(const uint8_t w, const uint8_t h, const uint8_t tx, const uint8_t ty, const bool r, const bool g,
           const std::array<uint16_t, MAX_BLOCKS>& b)
        : repr(create_repr(w, h, tx, ty, r, g, b))
    {
        if (w < MIN_WIDTH || w > MAX_WIDTH || h < MIN_HEIGHT || h > MAX_HEIGHT) {
            throw std::invalid_argument("Board size out of bounds");
        }
        if (tx >= MAX_WIDTH || ty >= MAX_HEIGHT) {
            throw std::invalid_argument("Goal out of bounds");
        }
    }

    explicit puzzle(const std::string& string_repr)
        : repr(create_repr(string_repr)) {}

public:
    auto operator==(const puzzle& other) const -> bool
    {
        return repr.raw == other.repr.raw;
    }

    auto operator!=(const puzzle& other) const -> bool
    {
        return repr.raw != other.repr.raw;
    }

    auto operator<(const puzzle& other) const -> bool
    {
        // Start from MSB and go to LSB. If equal, check the next.
        for (int i = 3; i >= 0; --i) {
            if (repr.raw[i] < other.repr.raw[i]) {
                return true;
            }
            if (repr.raw[i] > other.repr.raw[i]) {
                return false;
            }
        }

        // All are equal
        return false;
    }

    auto operator<=(const puzzle& other) const -> bool
    {
        return *this < other || *this == other;
    }

    auto operator>(const puzzle& other) const -> bool
    {
        return !(*this <= other);
    }

    auto operator>=(const puzzle& other) const -> bool
    {
        return !(*this < other);
    }

    auto repr_view() const
    {
        return std::span<const uint16_t>(repr.cooked.blocks.data(), block_count());
    }

    auto block_view() const
    {
        return std::span<const uint16_t>(repr.cooked.blocks.data(), block_count()) | std::views::transform(
            [](const uint16_t val)
            {
                return block(val);
            });
    }

    template <typename T, typename... Rest>
    static auto hash_combine(std::size_t& seed, const T& v, const Rest&... rest) -> void;

private:
    [[nodiscard]] static constexpr auto invalid_blocks() -> std::array<uint16_t, MAX_BLOCKS>
    {
        std::array<uint16_t, MAX_BLOCKS> blocks;
        for (size_t i = 0; i < MAX_BLOCKS; ++i) {
            blocks[i] = block::INVALID;
        }
        return blocks;
    }

    [[nodiscard]] static auto create_meta(const std::tuple<uint8_t, uint8_t, uint8_t, uint8_t, bool, bool>& meta) -> uint16_t;
    [[nodiscard]] static auto create_repr(uint8_t w, uint8_t h, uint8_t tx, uint8_t ty, bool r, bool g,
                                          const std::array<uint16_t, MAX_BLOCKS>& b) -> repr_cooked;

    [[nodiscard]] static auto create_repr(uint64_t byte_0, uint64_t byte_1, uint64_t byte_2,
                                          uint64_t byte_3) -> repr_cooked;

    [[nodiscard]] static auto create_repr(const std::string& string_repr) -> repr_cooked;

    // Repr setters
    [[nodiscard]] auto set_restricted(bool restricted) const -> puzzle;
    [[nodiscard]] auto set_width(uint8_t width) const -> puzzle;
    [[nodiscard]] auto set_height(uint8_t height) const -> puzzle;
    [[nodiscard]] auto set_goal(bool goal) const -> puzzle;
    [[nodiscard]] auto set_goal_x(uint8_t target_x) const -> puzzle;
    [[nodiscard]] auto set_goal_y(uint8_t target_y) const -> puzzle;
    [[nodiscard]] auto set_blocks(std::array<uint16_t, MAX_BLOCKS> blocks) const -> puzzle;

public:
    // Repr getters
    [[nodiscard]] auto unpack_meta() const -> std::tuple<uint8_t, uint8_t, uint8_t, uint8_t, bool, bool>;
    [[nodiscard]] auto get_restricted() const -> bool;
    [[nodiscard]] auto get_width() const -> uint8_t;
    [[nodiscard]] auto get_height() const -> uint8_t;
    [[nodiscard]] auto get_goal() const -> bool;
    [[nodiscard]] auto get_goal_x() const -> uint8_t;
    [[nodiscard]] auto get_goal_y() const -> uint8_t;

    // Util
    [[nodiscard]] auto hash() const -> size_t;
    [[nodiscard]] auto string_repr() const -> std::string;
    [[nodiscard]] static auto try_parse_string_repr(const std::string& string_repr) -> std::optional<repr_cooked>;
    [[nodiscard]] auto valid() const -> bool;
    [[nodiscard]] auto try_get_invalid_reason() const -> std::optional<std::string>;
    [[nodiscard]] auto block_count() const -> uint8_t;
    [[nodiscard]] auto goal_reached() const -> bool;
    [[nodiscard]] auto try_get_block(uint8_t x, uint8_t y) const -> std::optional<block>;
    [[nodiscard]] auto try_get_target_block() const -> std::optional<block>;
    [[nodiscard]] auto covers(uint8_t x, uint8_t y, uint8_t _w, uint8_t _h) const -> bool;
    [[nodiscard]] auto covers(uint8_t x, uint8_t y) const -> bool;
    [[nodiscard]] auto covers(block b) const -> bool;

    // Editing
    [[nodiscard]] auto toggle_restricted() const -> puzzle;
    [[nodiscard]] auto try_set_goal(uint8_t x, uint8_t y) const -> std::optional<puzzle>;
    [[nodiscard]] auto clear_goal() const -> puzzle;
    [[nodiscard]] auto try_add_column() const -> std::optional<puzzle>;
    [[nodiscard]] auto try_remove_column() const -> std::optional<puzzle>;
    [[nodiscard]] auto try_add_row() const -> std::optional<puzzle>;
    [[nodiscard]] auto try_remove_row() const -> std::optional<puzzle>;
    [[nodiscard]] auto try_add_block(block b) const -> std::optional<puzzle>;
    [[nodiscard]] auto try_remove_block(uint8_t x, uint8_t y) const -> std::optional<puzzle>;
    [[nodiscard]] auto try_toggle_target(uint8_t x, uint8_t y) const -> std::optional<puzzle>;
    [[nodiscard]] auto try_toggle_wall(uint8_t x, uint8_t y) const -> std::optional<puzzle>;

    // Playing
    [[nodiscard]] auto try_move_block_at(uint8_t x, uint8_t y, direction dir) const -> std::optional<puzzle>;

    // Statespace
    [[nodiscard]] auto try_move_block_at_fast(uint64_t bitmap, uint8_t block_idx,
                                              direction dir) const -> std::optional<puzzle>;
    static auto sorted_replace(std::array<uint16_t, MAX_BLOCKS> blocks, uint8_t idx,
                               uint16_t new_val) -> std::array<uint16_t, MAX_BLOCKS>;
    auto blocks_bitmap() const -> uint64_t;
    static inline auto bitmap_set_bit(uint64_t bitmap, uint8_t x, uint8_t y) -> uint64_t;
    static inline auto bitmap_get_bit(uint64_t bitmap, uint8_t x, uint8_t y) -> bool;
    static auto bitmap_clear_block(uint64_t bitmap, block b) -> uint64_t;
    static auto bitmap_check_collision(uint64_t bitmap, block b) -> bool;
    static auto bitmap_check_collision(uint64_t bitmap, block b, direction dir) -> bool;

    template <typename F>
    auto for_each_adjacent(F&& callback) const -> void
    {
        const uint64_t bitmap = blocks_bitmap();
        const bool r = get_restricted();
        for (uint8_t idx = 0; idx < MAX_BLOCKS; idx++) {
            const block b = block(repr.cooked.blocks[idx]);
            if (!b.valid()) {
                break;
            }
            if (b.get_immovable()) {
                continue;
            }
            const int dirs = r ? b.principal_dirs() : nor | eas | sou | wes;
            for (const direction d : {nor, eas, sou, wes}) {
                if (dirs & d) {
                    if (auto moved = try_move_block_at_fast(bitmap, idx, d)) {
                        callback(*moved);
                    }
                }
            }
        }
    }

    [[nodiscard]] auto explore_state_space() const
        -> std::pair<std::vector<puzzle>, std::vector<std::pair<size_t, size_t>>>;
};

// Hash functions for sets and maps

struct puzzle_hasher
{
    auto operator()(const puzzle& s) const noexcept -> size_t
    {
        return s.hash();
    }
};

struct link_hasher
{
    auto operator()(const std::pair<puzzle, puzzle>& s) const noexcept -> size_t
    {
        size_t h = 0;
        if (s.first < s.second) {
            puzzle::hash_combine(h, s.first, s.second);
        } else {
            puzzle::hash_combine(h, s.second, s.first);
        }
        return h;
    }
};

struct link_equal_to
{
    auto operator()(const std::pair<puzzle, puzzle>& a, const std::pair<puzzle, puzzle>& b) const noexcept -> bool
    {
        return (a.first == b.first && a.second == b.second) || (a.first == b.second && a.second == b.first);
    }
};

template <typename T, typename... Rest>
auto puzzle::hash_combine(std::size_t& seed, const T& v, const Rest&... rest) -> void
{
    auto h = []<typename HashedType>(const HashedType& val) -> std::size_t
    {
        if constexpr (std::is_same_v<std::decay_t<HashedType>, puzzle>) {
            return puzzle_hasher{}(val);
        } else if constexpr (std::is_same_v<std::decay_t<HashedType>, std::pair<puzzle, puzzle>>) {
            return link_hasher{}(val);
        } else {
            return std::hash<std::decay_t<HashedType>>{}(val);
        }
    };

    seed ^= h(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    (hash_combine(seed, rest), ...);
}

#endif