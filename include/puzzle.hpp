#ifndef PUZZLE_HPP_
#define PUZZLE_HPP_

#include "config.hpp"
#include "util.hpp"
#include "bits.hpp"

#include <array>
#include <cstddef>
#include <functional>
#include <ranges>
#include <string>
#include <vector>
#include <boost/unordered/unordered_flat_set.hpp>

// #define RUNTIME_CHECKS

// Forward declare to use in puzzle member functions
struct block_hasher;
struct block_hasher2;
struct block_equal2;
struct puzzle_hasher;

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
        // Repr setters
        [[nodiscard]] static auto create_repr(uint8_t x,
                                              uint8_t y,
                                              uint8_t w,
                                              uint8_t h,
                                              bool t = false,
                                              bool i = false) -> uint16_t;
        [[nodiscard]] inline auto set_x(uint8_t x) const -> block;
        [[nodiscard]] inline auto set_y(uint8_t y) const -> block;
        [[nodiscard]] inline auto set_width(uint8_t width) const -> block;
        [[nodiscard]] inline auto set_height(uint8_t height) const -> block;
        [[nodiscard]] inline auto set_target(bool target) const -> block;
        [[nodiscard]] inline auto set_immovable(bool immovable) const -> block;

    public:
        // Repr getters
        [[nodiscard]] auto unpack_repr() const -> std::tuple<uint8_t, uint8_t, uint8_t, uint8_t, bool, bool>;
        [[nodiscard]] inline auto get_x() const -> uint8_t;
        [[nodiscard]] inline auto get_y() const -> uint8_t;
        [[nodiscard]] inline auto get_width() const -> uint8_t;
        [[nodiscard]] inline auto get_height() const -> uint8_t;
        [[nodiscard]] inline auto get_target() const -> bool;
        [[nodiscard]] inline auto get_immovable() const -> bool;

        // Util
        [[nodiscard]] auto hash() const -> size_t;
        [[nodiscard]] auto position_independent_hash() const -> size_t;
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
    }
        PACKED;

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

    puzzle(const uint8_t w,
           const uint8_t h,
           const uint8_t tx,
           const uint8_t ty,
           const bool r,
           const bool g,
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

    puzzle(const uint8_t w, const uint8_t h)
        : repr(create_repr(w, h, 0, 0, false, false, invalid_blocks()))
    {
        if (w < MIN_WIDTH || w > MAX_WIDTH || h < MIN_HEIGHT || h > MAX_HEIGHT) {
            throw std::invalid_argument("Board size out of bounds");
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

    // Repr setters
    [[nodiscard]] static auto create_meta(
        const std::tuple<uint8_t, uint8_t, uint8_t, uint8_t, bool, bool>& meta) -> uint16_t;
    [[nodiscard]] static auto create_repr(uint8_t w,
                                          uint8_t h,
                                          uint8_t tx,
                                          uint8_t ty,
                                          bool r,
                                          bool g,
                                          const std::array<uint16_t, MAX_BLOCKS>& b) -> repr_cooked;
    [[nodiscard]] static auto create_repr(uint64_t byte_0,
                                          uint64_t byte_1,
                                          uint64_t byte_2,
                                          uint64_t byte_3) -> repr_cooked;
    [[nodiscard]] static auto create_repr(const std::string& string_repr) -> repr_cooked;
    [[nodiscard]] inline auto set_restricted(bool restricted) const -> puzzle;
    [[nodiscard]] inline auto set_width(uint8_t width) const -> puzzle;
    [[nodiscard]] inline auto set_height(uint8_t height) const -> puzzle;
    [[nodiscard]] inline auto set_goal(bool goal) const -> puzzle;
    [[nodiscard]] inline auto set_goal_x(uint8_t target_x) const -> puzzle;
    [[nodiscard]] inline auto set_goal_y(uint8_t target_y) const -> puzzle;
    [[nodiscard]] auto set_blocks(std::array<uint16_t, MAX_BLOCKS> blocks) const -> puzzle;

public:
    // Repr getters
    [[nodiscard]] auto unpack_meta() const -> std::tuple<uint8_t, uint8_t, uint8_t, uint8_t, bool, bool>;
    [[nodiscard]] inline auto get_restricted() const -> bool;
    [[nodiscard]] inline auto get_width() const -> uint8_t;
    [[nodiscard]] inline auto get_height() const -> uint8_t;
    [[nodiscard]] inline auto get_goal() const -> bool;
    [[nodiscard]] inline auto get_goal_x() const -> uint8_t;
    [[nodiscard]] inline auto get_goal_y() const -> uint8_t;

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

    // Bitmap
    [[nodiscard]] auto blocks_bitmap() const -> uint64_t;
    [[nodiscard]] auto blocks_bitmap_h() const -> uint64_t;
    [[nodiscard]] auto blocks_bitmap_v() const -> uint64_t;
    static INLINE inline auto bitmap_clear_bit(uint64_t& bitmap, uint8_t w, uint8_t x, uint8_t y) -> void;
    static INLINE inline auto bitmap_set_bit(uint64_t& bitmap, uint8_t w, uint8_t x, uint8_t y) -> void;
    [[nodiscard]] static INLINE inline auto bitmap_get_bit(uint64_t bitmap, uint8_t w, uint8_t x, uint8_t y) -> bool;
    INLINE inline auto bitmap_clear_block(uint64_t& bitmap, block b) const -> void;
    INLINE inline auto bitmap_set_block(uint64_t& bitmap, block b) const -> void;
    [[nodiscard]] INLINE inline auto bitmap_is_empty(uint64_t bitmap) const -> bool;
    [[nodiscard]] INLINE inline auto bitmap_is_full(uint64_t bitmap) const -> bool;
    [[nodiscard]] INLINE inline auto bitmap_check_collision(uint64_t bitmap, block b) const -> bool;
    [[nodiscard]] INLINE inline auto bitmap_check_collision(uint64_t bitmap, block b, dir dir) const -> bool;
    [[nodiscard]] auto bitmap_find_first_empty(uint64_t bitmap, int& x, int& y) const -> bool;

    // Playing
    [[nodiscard]] auto try_move_block_at(uint8_t x, uint8_t y, dir dir) const -> std::optional<puzzle>;

    // Statespace
    [[nodiscard]] INLINE inline auto try_move_block_at_fast(uint64_t bitmap,
                                                            uint8_t block_idx,
                                                            dir dir,
                                                            bool check_collision = true) const -> std::optional<puzzle>;
    [[nodiscard]] static auto sorted_replace(std::array<uint16_t, MAX_BLOCKS> blocks,
                                             uint8_t idx,
                                             uint16_t new_val) -> std::array<uint16_t, MAX_BLOCKS>;

    template <typename F>
    // ReSharper disable once CppRedundantInlineSpecifier
    INLINE inline auto for_each_adjacent(F&& callback) const -> void
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
            for (const dir d : {nor, eas, sou, wes}) {
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

    // Determines to which cluster a puzzle belongs. Clusters are identified by the
    // state with the numerically smallest binary representation.
    [[nodiscard]] auto get_cluster_id_and_solution() const -> std::pair<puzzle, bool>;

    static auto generate_block_sequences(
        const boost::unordered_flat_set<block, block_hasher2, block_equal2>& permitted_blocks,
        block target_block,
        size_t max_blocks,
        std::vector<block>& current_sequence,
        int current_area,
        int board_area,
        const std::function<void(const std::vector<block>&)>& callback) -> void;

    static auto place_block_sequence(const puzzle& p,
                                     const uint64_t& bitmap,
                                     const std::tuple<uint8_t, uint8_t, uint8_t, uint8_t, bool, bool>& p_repr,
                                     const std::vector<block>& sequence,
                                     block target_block,
                                     const std::tuple<uint8_t, uint8_t, uint8_t, uint8_t>& target_block_pos_range,
                                     bool has_target,
                                     size_t index,
                                     const std::function<void(const puzzle&)>& callback) -> void;

    [[nodiscard]] auto explore_puzzle_space(
        const boost::unordered_flat_set<block, block_hasher2, block_equal2>& permitted_blocks,
        block target_block,
        const std::tuple<uint8_t, uint8_t, uint8_t, uint8_t>& target_block_pos_range,
        size_t max_blocks,
        std::optional<BS::thread_pool<>* const> thread_pool = std::nullopt) const -> boost::unordered_flat_set<
        puzzle, puzzle_hasher>;
};

// Inline functions definitions
#ifndef REGION_INLINE_DEFS

inline auto puzzle::block::set_x(const uint8_t x) const -> block
{
    #ifdef RUNTIME_CHECKS
    if (x > 7) {
        throw std::invalid_argument("Block x-position out of bounds");
    }
    #endif

    block b = *this;
    set_bits(b.repr, X_S, X_E, x);
    return b;
}

inline auto puzzle::block::set_y(const uint8_t y) const -> block
{
    #ifdef RUNTIME_CHECKS
    if (y > 7) {
        throw std::invalid_argument("Block y-position out of bounds");
    }
    #endif

    block b = *this;
    set_bits(b.repr, Y_S, Y_E, y);
    return b;
}

inline auto puzzle::block::set_width(const uint8_t width) const -> block
{
    #ifdef RUNTIME_CHECKS
    if (width - 1 > 7) {
        throw std::invalid_argument("Block width out of bounds");
    }
    #endif

    block b = *this;
    set_bits(b.repr, WIDTH_S, WIDTH_E, width - 1u);
    return b;
}

inline auto puzzle::block::set_height(const uint8_t height) const -> block
{
    #ifdef RUNTIME_CHECKS
    if (height - 1 > 7) {
        throw std::invalid_argument("Block height out of bounds");
    }
    #endif

    block b = *this;
    set_bits(b.repr, HEIGHT_S, HEIGHT_E, height - 1u);
    return b;
}

inline auto puzzle::block::set_target(const bool target) const -> block
{
    block b = *this;
    set_bits(b.repr, TARGET_S, TARGET_E, target);
    return b;
}

inline auto puzzle::block::set_immovable(const bool immovable) const -> block
{
    block b = *this;
    set_bits(b.repr, IMMOVABLE_S, IMMOVABLE_E, immovable);
    return b;
}

inline auto puzzle::block::get_x() const -> uint8_t
{
    return get_bits(repr, X_S, X_E);
}

inline auto puzzle::block::get_y() const -> uint8_t
{
    return get_bits(repr, Y_S, Y_E);
}

inline auto puzzle::block::get_width() const -> uint8_t
{
    return get_bits(repr, WIDTH_S, WIDTH_E) + 1u;
}

inline auto puzzle::block::get_height() const -> uint8_t
{
    return get_bits(repr, HEIGHT_S, HEIGHT_E) + 1u;
}

inline auto puzzle::block::get_target() const -> bool
{
    return get_bits(repr, TARGET_S, TARGET_E);
}

inline auto puzzle::block::get_immovable() const -> bool
{
    return get_bits(repr, IMMOVABLE_S, IMMOVABLE_E);
}

inline auto puzzle::set_restricted(const bool restricted) const -> puzzle
{
    uint16_t meta = repr.cooked.meta;
    set_bits(meta, RESTRICTED_S, RESTRICTED_E, restricted);
    return puzzle(meta, repr.cooked.blocks);
}

inline auto puzzle::set_width(const uint8_t width) const -> puzzle
{
    #ifdef RUNTIME_CHECKS
    if (width - 1 > MAX_WIDTH) {
        throw "Board width out of bounds";
    }
    #endif

    uint16_t meta = repr.cooked.meta;
    set_bits(meta, WIDTH_S, WIDTH_E, width - 1u);
    return puzzle(meta, repr.cooked.blocks);
}

inline auto puzzle::set_height(const uint8_t height) const -> puzzle
{
    #ifdef RUNTIME_CHECKS
    if (height - 1 > MAX_HEIGHT) {
        throw "Board height out of bounds";
    }
    #endif

    uint16_t meta = repr.cooked.meta;
    set_bits(meta, HEIGHT_S, HEIGHT_E, height - 1u);
    return puzzle(meta, repr.cooked.blocks);
}

inline auto puzzle::set_goal(const bool goal) const -> puzzle
{
    uint16_t meta = repr.cooked.meta;
    set_bits(meta, GOAL_S, GOAL_E, goal);
    return puzzle(meta, repr.cooked.blocks);
}

inline auto puzzle::set_goal_x(const uint8_t target_x) const -> puzzle
{
    #ifdef RUNTIME_CHECKS
    if (target_x >= MAX_WIDTH) {
        throw "Board target x out of bounds";
    }
    #endif

    uint16_t meta = repr.cooked.meta;
    set_bits(meta, GOAL_X_S, GOAL_X_E, target_x);
    return puzzle(meta, repr.cooked.blocks);
}

inline auto puzzle::set_goal_y(const uint8_t target_y) const -> puzzle
{
    #ifdef RUNTIME_CHECKS
    if (target_y >= MAX_HEIGHT) {
        throw "Board target y out of bounds";
    }
    #endif

    uint16_t meta = repr.cooked.meta;
    set_bits(meta, GOAL_Y_S, GOAL_Y_E, target_y);
    return puzzle(meta, repr.cooked.blocks);
}

inline auto puzzle::get_restricted() const -> bool
{
    return get_bits(repr.cooked.meta, RESTRICTED_S, RESTRICTED_E);
}

inline auto puzzle::get_width() const -> uint8_t
{
    return get_bits(repr.cooked.meta, WIDTH_S, WIDTH_E) + 1u;
}

inline auto puzzle::get_height() const -> uint8_t
{
    return get_bits(repr.cooked.meta, HEIGHT_S, HEIGHT_E) + 1u;
}

inline auto puzzle::get_goal() const -> bool
{
    return get_bits(repr.cooked.meta, GOAL_S, GOAL_E);
}

inline auto puzzle::get_goal_x() const -> uint8_t
{
    return get_bits(repr.cooked.meta, GOAL_X_S, GOAL_X_E);
}

inline auto puzzle::get_goal_y() const -> uint8_t
{
    return get_bits(repr.cooked.meta, GOAL_Y_S, GOAL_Y_E);
}

INLINE inline auto puzzle::bitmap_clear_bit(uint64_t& bitmap, const uint8_t w, const uint8_t x, const uint8_t y) -> void
{
    set_bits(bitmap, y * w + x, y * w + x, 0u);
}

INLINE inline auto puzzle::bitmap_set_bit(uint64_t& bitmap, const uint8_t w, const uint8_t x, const uint8_t y) -> void
{
    set_bits(bitmap, y * w + x, y * w + x, 1u);
}

INLINE inline auto puzzle::bitmap_get_bit(const uint64_t bitmap,
                                          const uint8_t w,
                                          const uint8_t x,
                                          const uint8_t y) -> bool
{
    return get_bits(bitmap, y * w + x, y * w + x);
}

INLINE inline auto puzzle::bitmap_clear_block(uint64_t& bitmap, const block b) const -> void
{
    const auto [x, y, w, h, t, i] = b.unpack_repr();
    const uint8_t width = get_width();

    for (int dy = 0; dy < h; ++dy) {
        for (int dx = 0; dx < w; ++dx) {
            bitmap_clear_bit(bitmap, width, x + dx, y + dy);
        }
    }
}

INLINE inline auto puzzle::bitmap_set_block(uint64_t& bitmap, const block b) const -> void
{
    const auto [x, y, w, h, t, i] = b.unpack_repr();
    const uint8_t width = get_width();

    for (int dy = 0; dy < h; ++dy) {
        for (int dx = 0; dx < w; ++dx) {
            bitmap_set_bit(bitmap, width, x + dx, y + dy);
        }
    }
}

INLINE inline auto puzzle::bitmap_is_empty(const uint64_t bitmap) const -> bool
{
    const uint8_t shift = 64 - get_width() * get_height();
    return bitmap << shift == 0;
}

INLINE inline auto puzzle::bitmap_is_full(const uint64_t bitmap) const -> bool
{
    const uint8_t shift = 64 - get_width() * get_height();
    return ((bitmap << shift) >> shift) == ((static_cast<uint64_t>(-1) << shift) >> shift);
}

INLINE inline auto puzzle::bitmap_check_collision(const uint64_t bitmap, const block b) const -> bool
{
    const auto [x, y, w, h, t, i] = b.unpack_repr();
    const uint8_t width = get_width();

    for (int dy = 0; dy < h; ++dy) {
        for (int dx = 0; dx < w; ++dx) {
            if (bitmap_get_bit(bitmap, width, x + dx, y + dy)) {
                return true; // collision
            }
        }
    }

    return false;
}

INLINE inline auto puzzle::bitmap_check_collision(const uint64_t bitmap,
                                                  const block b,
                                                  const dir dir) const -> bool
{
    const auto [x, y, w, h, t, i] = b.unpack_repr();
    const uint8_t width = get_width();

    switch (dir) {
    case nor: // Check the row above: (x...x+w-1, y-1)
        for (int dx = 0; dx < w; ++dx) {
            if (bitmap_get_bit(bitmap, width, x + dx, y - 1)) {
                return true;
            }
        }
        break;
    case sou: // Check the row below: (x...x+w-1, y+h)
        for (int dx = 0; dx < w; ++dx) {
            if (bitmap_get_bit(bitmap, width, x + dx, y + h)) {
                return true;
            }
        }
        break;
    case wes: // Check the column left: (x-1, y...y+h-1)
        for (int dy = 0; dy < h; ++dy) {
            if (bitmap_get_bit(bitmap, width, x - 1, y + dy)) {
                return true;
            }
        }
        break;
    case eas: // Check the column right: (x+w, y...y+h-1)
        for (int dy = 0; dy < h; ++dy) {
            if (bitmap_get_bit(bitmap, width, x + w, y + dy)) {
                return true;
            }
        }
        break;
    }
    return false;
}

INLINE inline auto puzzle::try_move_block_at_fast(uint64_t bitmap,
                                                  const uint8_t block_idx,
                                                  const dir dir,
                                                  const bool check_collision) const -> std::optional<puzzle>
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
    if (check_collision) {
        bitmap_clear_block(bitmap, b);
        if (bitmap_check_collision(bitmap, b, dir)) {
            return std::nullopt;
        }
    }

    // Replace block
    const std::array<uint16_t, MAX_BLOCKS> blocks = sorted_replace(repr.cooked.blocks,
                                                                   block_idx,
                                                                   block::create_repr(
                                                                       _target_x,
                                                                       _target_y,
                                                                       bw,
                                                                       bh,
                                                                       bt));

    // This constructor doesn't sort
    return puzzle(std::make_tuple(w, h, gx, gy, r, g), blocks);
}

#endif

// Hash functions for sets and maps.
// Declared after puzzle class to use puzzle::hash_combine
#ifndef REGION_HASHERS

struct block_hasher
{
    auto operator()(const puzzle::block& b) const noexcept -> size_t
    {
        return b.hash();
    }
};

struct block_hasher2
{
    auto operator()(const puzzle::block& b) const noexcept -> size_t
    {
        return b.position_independent_hash();
    }
};

struct block_equal2
{
    auto operator()(const puzzle::block& a, const puzzle::block& b) const noexcept -> bool
    {
        const auto [ax, ay, aw, ah, at, ai] = a.unpack_repr();
        const auto [bx, by, bw, bh, bt, bi] = b.unpack_repr();

        return aw == bw && ah == bh && at == bt && ai == bi;
    }
};

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

struct link_equal
{
    auto operator()(const std::pair<puzzle, puzzle>& a, const std::pair<puzzle, puzzle>& b) const noexcept -> bool
    {
        return (a.first == b.first && a.second == b.second) || (a.first == b.second && a.second == b.first);
    }
};

template <typename T, typename... Rest>
auto puzzle::hash_combine(size_t& seed, const T& v, const Rest&... rest) -> void
{
    auto hasher = []<typename HashedType>(const HashedType& val) -> std::size_t
    {
        if constexpr (std::is_same_v<std::decay_t<HashedType>, puzzle>) {
            return puzzle_hasher{}(val);
        } else if constexpr (std::is_same_v<std::decay_t<HashedType>, std::pair<puzzle, puzzle>>) {
            return link_hasher{}(val);
        } else {
            return std::hash<std::decay_t<HashedType>>{}(val);
        }
    };

    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    (hash_combine(seed, rest), ...);
}

#endif

#endif