#ifndef PLACEMENT_SPACE_HPP_
#define PLACEMENT_SPACE_HPP_

#include "puzzle_space.hpp"
#include "wall_layout.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

/**
 * The legal moves of one shape anchored at one cell. `count` is constant for a
 * given (shape, cell), so the loop over `target` has a stable trip count and needs
 * no "is this direction legal" test.
 */
struct placement_moves
{
    u8 count = 0;
    std::array<std::int8_t, 4> target{};
};

/**
 * One block shape in a puzzle-space partition. The target is always shape 0.
 * masks[cell] is the board coverage bitmap of the shape anchored at cell, or 0
 * if the shape does not fit there. moves[cell] lists the anchor cells the shape
 * reaches when it moves one cell, precomputed with the board bounds and the
 * movement rules. The tables are per shape and independent of the partition, so a
 * move needs no division and no bounds switch.
 */
struct placement_shape
{
    block shape;
    u8 width = 1;
    u8 height = 1;
    bool target = false;
    u8 dirs = 0;
    std::array<u64, 64> masks{};
    std::array<placement_moves, 64> moves{};

    [[nodiscard]] auto area() const -> size_t
    {
        return static_cast<size_t>(width) * height;
    }
};

/**
 * An exact block multiset: the target plus counts[i] copies of inventory shape i.
 * A move never changes the multiset, so components never cross partitions.
 */
struct placement_partition
{
    std::vector<u8> counts;
    u32 wall_layout = 0;
    size_t wall_pieces = 0;
    size_t blocks = 0; // including the target
    size_t area = 0;   // including the target
    size_t placements = 0;
    bool has_goal = false;
};

/**
 * Dense state table with a CHD-style minimal perfect hash index.
 *
 * Keys are stored densely (id = insertion order). build_index() constructs the
 * MPHF once; after that find()/lookup() map a key to its dense id without a
 * separate slot array. lookup() is unchecked and is used by the search where
 * every generated neighbour is guaranteed to be a member of the key set.
 *
 * The displacement is multiplied by a second, independent hash of the key, so
 * two keys that share the hash bits used for the bucket can still be separated.
 */
class placement_table
{
public:
    // Reports the progress of the current construction step. done/total is the
    // finished fraction of the step; the index build reports permille (total is
    // 1000), so callers can show one continuous bar for the whole build.
    using index_progress_fn = void (*)(void* context, size_t done, size_t total);

    placement_table(size_t expected, size_t bytes);

    // Appends a key that is not present yet and returns its dense id.
    auto insert(const u8* key) -> u32;

    /**
     * Builds the minimal perfect hash. Must be called after all inserts and
     * before find()/lookup(). ids_to_remap is updated from insertion ids to the
     * new dense ids (used for the goal states collected during enumeration).
     *
     * Construction can take seconds for large partitions, so it polls stop and
     * returns false when the search was cancelled (the index is left unbuilt
     * then) or when no displacement table could be found. report (optional) is
     * called periodically with the stage progress.
     */
    auto build_index(std::vector<u32>* ids_to_remap = nullptr, std::stop_token stop = {},
                     index_progress_fn report = nullptr, void* context = nullptr) -> bool;

    [[nodiscard]] auto find(const u8* key) const -> std::optional<u32>;
    [[nodiscard]] auto lookup(const u8* key) const -> u32;
    [[nodiscard]] auto key(u32 id) const -> const u8*;
    [[nodiscard]] auto size() const -> size_t;
    [[nodiscard]] auto bytes() const -> size_t;
    [[nodiscard]] auto index_built() const -> bool;

    // Frees the dense key array. lookup() keeps working; key()/find() must not
    // be used afterwards. Used by the batch path once the sweep carries keys
    // per frontier.
    auto release_keys() -> void;
    [[nodiscard]] auto keys_released() const -> bool;

private:
    size_t bytes_ = 1;
    size_t size_ = 0;
    std::vector<u8> keys_;
    std::vector<u32> displacements_;
    std::vector<u64> occupied_;
    std::vector<u32> rank_prefix_;
    size_t bucket_mask_ = 0;
    size_t slot_mask_ = 0;
    u64 seed_ = 0x9e3779b97f4a7c15ull;
    u64 slot_seed_ = 0xbf58476d1ce4e5b9ull;
    bool built_ = false;
    bool keys_released_ = false;

    [[nodiscard]] auto hash_key(const u8* key, u64 seed) const -> u64;
    [[nodiscard]] auto rank(size_t position) const -> u32;
    [[nodiscard]] auto key_equal(const u8* a, const u8* b) const -> bool;
    [[nodiscard]] auto mutable_key(u32 id) -> u8*;
};

/**
 * Exact-multiset placement space for one puzzle-space configuration.
 *
 * States are encoded as a sequence of 6-bit board cells: the target cell first,
 * then the cells of every inventory shape group in ascending order. Because the
 * partition fixes the shape multiset, the shape ids need not be stored.
 */
class placement_space
{
public:
    using emit_fn = void (*)(void* context, const u8* key, bool goal);
    using move_fn = void (*)(void* context, const u8* key);

    explicit placement_space(const puzzle_space_config& config, std::stop_token stop = {});

    [[nodiscard]] auto layouts() const -> const std::vector<wall_layout>&;
    [[nodiscard]] auto wall_layout_count() const -> size_t;
    [[nodiscard]] auto error() const -> const std::string&;

    [[nodiscard]] auto partitions() const -> const std::vector<placement_partition>&;
    auto partitions() -> std::vector<placement_partition>&;
    [[nodiscard]] auto partition(size_t index) const -> const placement_partition&;
    auto partition(size_t index) -> placement_partition&;
    [[nodiscard]] auto partition_count() const -> size_t;
    [[nodiscard]] auto width() const -> int;
    [[nodiscard]] auto height() const -> int;
    [[nodiscard]] auto key_bytes(size_t blocks) const -> size_t;
    [[nodiscard]] auto fixed_bytes() const -> size_t;
    // Cells that are not walls for a specific partition. A partition whose area
    // equals this fills the whole board, so no move is possible at all.
    [[nodiscard]] auto free_cells(const placement_partition& partition) const -> size_t;
    // Compatibility accessor for single-layout users/tests.
    [[nodiscard]] auto free_cells() const -> size_t;

    /**
     * Counts the placements of one partition and reports whether any of them
     * puts the target on the goal cell. touched is set when the stop token was
     * observed.
     */
    auto count(size_t index, std::stop_token stop, bool& cancelled) const
        -> std::pair<size_t, bool>;

    /**
     * Enumerates every placement of one partition exactly once. Returns the
     * number of emitted states (which equals count(index, ...) unless the search
     * was cancelled). With pack_keys == false the key passed to emit is null:
     * callers that only count placements (and check the goal flag) do not have to
     * pay for packing it.
     */
    auto enumerate(size_t index, std::stop_token stop, emit_fn emit, void* context, bool& cancelled,
                   bool pack_keys = true) const -> size_t;

    /**
     * Calls callback for every legal move of a state. Every generated key is a
     * member of the same partition (the placement set is closed under moves).
     */
    auto for_each_move(const placement_partition& partition, const u8* key, move_fn callback,
                       void* context) const -> void;

    [[nodiscard]] auto is_goal(const u8* key) const -> bool;

    // Converts a state into the canonical puzzle representation.
    [[nodiscard]] auto to_puzzle(const placement_partition& partition, const u8* key) const
        -> puzzle;

    // The exact 32-byte puzzle ordering words (repr.raw[0..3]).
    [[nodiscard]] auto state_repr(const placement_partition& partition, const u8* key) const
        -> std::array<u64, 4>;

private:
    static constexpr size_t MAX_SHAPES = 16;
    static constexpr size_t MAX_KEY_BYTES = 16;

    struct enumeration_state
    {
        std::array<std::array<u8, puzzle::MAX_BLOCKS>, MAX_SHAPES> cells{};
        std::array<u8, MAX_SHAPES> counts{};
        int target_cell = -1;
        int placed_other = 0;
    };

    std::vector<placement_shape> shapes_;
    std::vector<placement_partition> partitions_;
    std::vector<wall_layout> layouts_;

    int width_ = 0;
    int height_ = 0;
    int cells_ = 0;
    bool restricted_ = false;
    int goal_x_ = 0;
    int goal_y_ = 0;
    int goal_cell_ = 0;
    int target_area_ = 0;
    u64 target_range_mask_ = 0;
    std::string error_;

    [[nodiscard]] auto shape_dirs(const block& b) const -> u8;
    auto build_masks() -> void;
    auto build_partitions(const puzzle_space_config& config) -> void;
    auto pack_key(const enumeration_state& state, u8* key) const -> void;
    auto decode_key(const placement_partition& partition, const u8* key,
                    std::array<std::array<u8, puzzle::MAX_BLOCKS>, MAX_SHAPES>& cells,
                    std::array<u8, MAX_SHAPES>& counts) const -> void;
    auto recurse(const placement_partition& partition, const wall_layout& layout, int pos,
                 u64 occupied, int occupied_cells, int skipped, int remaining_blocks,
                 int remaining_area, enumeration_state& state, const std::stop_token& stop,
                 bool& cancelled, bool pack_keys, emit_fn emit, void* context) const -> void;
    auto emit_state(const enumeration_state& state, emit_fn emit, void* context) const -> void;
};

// Compares two state_repr() results with the exact puzzle::operator< ordering.
[[nodiscard]] auto placement_repr_less(const std::array<u64, 4>& a, const std::array<u64, 4>& b)
    -> bool;

#endif
