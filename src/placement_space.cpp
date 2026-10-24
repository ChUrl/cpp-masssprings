#include "placement_space.hpp"

#include <algorithm>
#include <bit>
#include <cstring>
#include <functional>
#include <numeric>
#include <ranges>
#include <tuple>

namespace
{
inline auto load_u64(const u8* key, const size_t bytes) -> u64
{
    switch (bytes) {
    case 1:
        return key[0];
    case 2:
        {
            u16 value = 0;
            std::memcpy(&value, key, 2);
            return value;
        }
    case 3:
        {
            u16 value = 0;
            std::memcpy(&value, key, 2);
            return value | (u64(key[2]) << 16);
        }
    case 4:
        {
            u32 value = 0;
            std::memcpy(&value, key, 4);
            return value;
        }
    case 5:
        {
            u32 value = 0;
            std::memcpy(&value, key, 4);
            return value | (u64(key[4]) << 32);
        }
    case 6:
        {
            u32 value = 0;
            std::memcpy(&value, key, 4);
            return value | (u64(key[4]) << 32) | (u64(key[5]) << 40);
        }
    case 7:
        {
            u32 value = 0;
            std::memcpy(&value, key, 4);
            return value | (u64(key[4]) << 32) | (u64(key[5]) << 40) | (u64(key[6]) << 48);
        }
    case 8:
        {
            u64 value = 0;
            std::memcpy(&value, key, 8);
            return value;
        }
    default:
        {
            u64 value = 0;
            std::memcpy(&value, key, 8);
            return value;
        }
    }
}

inline auto store_u64(const u64 value, const size_t bytes, u8* key) -> void
{
    switch (bytes) {
    case 1:
        key[0] = static_cast<u8>(value);
        break;
    case 2:
        {
            const auto part = static_cast<u16>(value);
            std::memcpy(key, &part, 2);
            break;
        }
    case 3:
        {
            const auto part = static_cast<u16>(value);
            std::memcpy(key, &part, 2);
            key[2] = static_cast<u8>(value >> 16);
            break;
        }
    case 4:
        {
            const auto part = static_cast<u32>(value);
            std::memcpy(key, &part, 4);
            break;
        }
    case 5:
        {
            const auto part = static_cast<u32>(value);
            std::memcpy(key, &part, 4);
            key[4] = static_cast<u8>(value >> 32);
            break;
        }
    case 6:
        {
            const auto part = static_cast<u32>(value);
            std::memcpy(key, &part, 4);
            key[4] = static_cast<u8>(value >> 32);
            key[5] = static_cast<u8>(value >> 40);
            break;
        }
    case 7:
        {
            const auto part = static_cast<u32>(value);
            std::memcpy(key, &part, 4);
            key[4] = static_cast<u8>(value >> 32);
            key[5] = static_cast<u8>(value >> 40);
            key[6] = static_cast<u8>(value >> 48);
            break;
        }
    case 8:
    default:
        {
            std::memcpy(key, &value, 8);
            break;
        }
    }
}

struct key_words
{
    u64 lo = 0;
    u64 hi = 0;
};

inline auto load_key(const u8* key, const size_t bytes) -> key_words
{
    key_words words;
    std::memcpy(&words.lo, key, std::min<size_t>(8, bytes));
    if (bytes > 8) {
        std::memcpy(&words.hi, key + 8, bytes - 8);
    }
    return words;
}

inline auto store_key(const key_words& words, const size_t bytes, u8* key) -> void
{
    std::memcpy(key, &words.lo, std::min<size_t>(8, bytes));
    if (bytes > 8) {
        std::memcpy(key + 8, &words.hi, bytes - 8);
    }
}

inline auto get_cell(const key_words& words, const int bit) -> u8
{
    u64 value = bit < 64 ? words.lo >> bit : words.hi >> (bit - 64);
    if (bit < 64 && bit + 6 > 64) {
        value |= words.hi << (64 - bit);
    }
    return static_cast<u8>(value & 0x3f);
}

inline auto set_cell(key_words& words, const int bit, const u8 cell) -> void
{
    const u64 value = cell;
    if (bit < 64) {
        words.lo |= value << bit;
        if (bit + 6 > 64) {
            words.hi |= value >> (64 - bit);
        }
    } else {
        words.hi |= value << (bit - 64);
    }
}

constexpr u8 ALL_DIRS = nor | eas | sou | wes;

auto next_power_of_two(size_t value) -> size_t
{
    size_t result = 16;
    while (result < value) {
        result <<= 1;
    }
    return result;
}

auto floor_power_of_two(size_t value) -> size_t
{
    size_t result = 1;
    while (result <= value / 2) {
        result <<= 1;
    }
    return result;
}

auto mix64(u64 value) -> u64
{
    value += 0x9e3779b97f4a7c15ull;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ull;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebull;
    return value ^ (value >> 31);
}

auto repr_less(const std::array<u64, 4>& a, const std::array<u64, 4>& b) -> bool
{
    for (int i = 3; i >= 0; --i) {
        if (a[i] < b[i]) {
            return true;
        }
        if (a[i] > b[i]) {
            return false;
        }
    }
    return false;
}
} // namespace

// ---------------------------------------------------------------------------
// placement_table
// ---------------------------------------------------------------------------

placement_table::placement_table(const size_t expected, const size_t bytes)
    : bytes_(std::max<size_t>(1, bytes))
{
    keys_.reserve(expected * bytes_);
}

auto placement_table::hash_key(const u8* key, const u64 seed) const -> u64
{
    if (bytes_ <= 8) {
        return mix64(load_u64(key, bytes_) ^ seed);
    }

    u64 low = 0;
    u64 high = 0;
    std::memcpy(&low, key, 8);
    std::memcpy(&high, key + 8, bytes_ - 8);
    return mix64(low ^ mix64(high + seed));
}

auto placement_table::insert(const u8* key) -> u32
{
    const auto id = static_cast<u32>(size_);
    const size_t offset = keys_.size();
    keys_.resize(offset + bytes_);
    if (bytes_ <= 8) {
        store_u64(load_u64(key, bytes_), bytes_, keys_.data() + offset);
    } else {
        std::memcpy(keys_.data() + offset, key, bytes_);
    }
    ++size_;
    built_ = false;
    keys_released_ = false;
    return id;
}

namespace
{
// Polling interval for the long (single-threaded) perfect-hash construction.
constexpr size_t INDEX_CHECK_INTERVAL = 1 << 14;
constexpr size_t INDEX_REPORT_INTERVAL = 1 << 10;
// The reported progress is permille of the whole construction so that all four
// passes (bucketing, displacement placement, permutation check, key remap) form
// one bar. The bands follow their share of the runtime: placement and the final
// remap are the expensive ones, the check is in between.
constexpr size_t INDEX_PERMILLE_BUCKETS = 30;
constexpr size_t INDEX_PERMILLE_PLACEMENT = 370;
constexpr size_t INDEX_PERMILLE_CHECK = 200;

auto index_permille(const size_t base, const size_t span, const size_t done, const size_t total)
    -> size_t
{
    return base + (total == 0 ? span : span * std::min(done, total) / total);
}
} // namespace

auto placement_table::build_index(std::vector<u32>* ids_to_remap, const std::stop_token stop,
                                  const index_progress_fn report, void* context) -> bool
{
    built_ = false;
    keys_released_ = false;
    displacements_.clear();
    occupied_.clear();
    rank_prefix_.clear();

    const auto publish = [report, context](const size_t done, const size_t total)
    {
        if (report != nullptr) {
            report(context, done, total);
        }
    };

    const size_t count = size_;
    if (count == 0) {
        built_ = true;
        return true;
    }

    // CHD: hash into buckets, find a displacement per bucket, mark the slot
    // bits, then map occupied slots to dense ids with a rank index.
    const size_t bucket_count =
        std::max<size_t>(1, floor_power_of_two(std::max<size_t>(1, count / 4)));
    bucket_mask_ = bucket_count - 1;

    std::vector<u32> offsets(bucket_count + 1, 0);
    for (u32 id = 0; id < count; ++id) {
        if ((id & (INDEX_CHECK_INTERVAL - 1)) == 0) {
            if (stop.stop_requested()) {
                return false;
            }
            publish(index_permille(0, INDEX_PERMILLE_BUCKETS, id, count), 1000);
        }
        const size_t bucket = hash_key(key(id), seed_) & bucket_mask_;
        ++offsets[bucket + 1];
    }
    for (size_t i = 1; i < offsets.size(); ++i) {
        offsets[i] += offsets[i - 1];
    }
    std::vector<u32> cursor(offsets.begin(), offsets.end());
    std::vector<u32> order(count);
    for (u32 id = 0; id < count; ++id) {
        if ((id & (INDEX_CHECK_INTERVAL - 1)) == 0 && stop.stop_requested()) {
            return false;
        }
        const size_t bucket = hash_key(key(id), seed_) & bucket_mask_;
        order[cursor[bucket]++] = id;
    }
    // The counting-sort cursor is dead from here on. Releasing it before the
    // placement arrays are allocated keeps the construction peak flat.
    std::vector<u32>().swap(cursor);
    publish(INDEX_PERMILLE_BUCKETS, 1000);

    // Place the largest buckets first: they are the ones that fail to find a
    // displacement once the table fills up, so they have to go in while it is
    // still (nearly) empty. Counting sort by size keeps this linear because
    // bucket sizes are tiny.
    std::vector<u32> bucket_order(bucket_count);
    {
        size_t max_size = 0;
        for (size_t bucket = 0; bucket < bucket_count; ++bucket) {
            max_size = std::max(max_size, size_t(offsets[bucket + 1] - offsets[bucket]));
        }
        std::vector<size_t> start(max_size + 2, 0);
        for (size_t bucket = 0; bucket < bucket_count; ++bucket) {
            ++start[size_t(offsets[bucket + 1] - offsets[bucket]) + 1];
        }
        for (size_t size = 1; size < start.size(); ++size) {
            start[size] += start[size - 1];
        }
        for (size_t bucket = 0; bucket < bucket_count; ++bucket) {
            const size_t size = size_t(offsets[bucket + 1] - offsets[bucket]);
            bucket_order[start[size]++] = static_cast<u32>(bucket);
        }
    }

    // Load factor ~0.5 instead of ~0.87. The CHD displacement search is the most
    // expensive loop of the whole search: at 0.87 most displacement attempts fail
    // on the first key, and each failure is a dependent, cache-missing bitmap
    // probe (measured: -31% cycles, -64% branch misses, -26% single-thread time
    // for a 6.2M-placement space). Going to ~0.25 buys nothing more.
    size_t slots = next_power_of_two(std::max<size_t>(16, count * 2));
    // Placement can be restarted when a bucket cannot be placed. The reported
    // progress counts the restarts as work done, so the bar keeps moving instead
    // of freezing at the end of the previous attempt.
    size_t placement_round = 0;
    for (int grow = 0; grow < 4; ++grow, slots <<= 1) {
        slot_mask_ = slots - 1;
        for (int attempt = 0; attempt < 8; ++attempt, ++placement_round) {
            if (stop.stop_requested()) {
                return false;
            }
            slot_seed_ = seed_ + 0x9e3779b97f4a7c15ull * static_cast<u64>(attempt + 1);
            displacements_.assign(bucket_count, 0);
            occupied_.assign((slots + 63) / 64, 0);

            std::vector<u32> candidates;
            std::vector<u64> bases;
            std::vector<u64> offsets_per_key;
            bool failed = false;
            // Walk the buckets from largest to smallest.
            for (size_t step = 0; step < bucket_count && !failed; ++step) {
                const size_t bucket = bucket_order[bucket_count - 1 - step];
                if ((step & (INDEX_CHECK_INTERVAL - 1)) == 0) {
                    if (stop.stop_requested()) {
                        return false;
                    }
                }
                if ((step & (INDEX_REPORT_INTERVAL - 1)) == 0) {
                    // Work of the current and all previous placement rounds,
                    // assuming the current one finishes: monotone and it keeps
                    // advancing through retries.
                    const size_t placed = placement_round * bucket_count + step;
                    publish(index_permille(INDEX_PERMILLE_BUCKETS, INDEX_PERMILLE_PLACEMENT, placed,
                                           placed + bucket_count),
                            1000);
                }
                const u32 begin = offsets[bucket];
                const u32 end = offsets[bucket + 1];
                if (begin == end) {
                    continue;
                }
                const u32 bucket_size = end - begin;
                candidates.resize(bucket_size);
                bases.resize(bucket_size);
                offsets_per_key.resize(bucket_size);
                for (u32 j = 0; j < bucket_size; ++j) {
                    const u8* bucket_key = key(order[begin + j]);
                    const u64 hash = hash_key(bucket_key, seed_);
                    bases[j] = hash >> 32;
                    offsets_per_key[j] = hash_key(bucket_key, slot_seed_);
                }

                bool placed = false;
                for (u32 displacement = 0; displacement < 2048; ++displacement) {
                    if (stop.stop_requested()) {
                        return false;
                    }
                    bool ok = true;
                    for (u32 j = 0; j < bucket_size; ++j) {
                        const auto pos = static_cast<u32>(
                            (bases[j] + u64(displacement) * offsets_per_key[j]) & slot_mask_);
                        if (((occupied_[pos >> 6] >> (pos & 63)) & 1u) != 0) {
                            ok = false;
                            break;
                        }
                        for (u32 k = 0; k < j; ++k) {
                            if (candidates[k] == pos) {
                                ok = false;
                                break;
                            }
                        }
                        if (!ok) {
                            break;
                        }
                        candidates[j] = pos;
                    }

                    if (ok) {
                        for (u32 j = 0; j < bucket_size; ++j) {
                            occupied_[candidates[j] >> 6] |= u64(1) << (candidates[j] & 63);
                        }
                        displacements_[bucket] = displacement;
                        placed = true;
                        break;
                    }
                }
                if (!placed) {
                    failed = true;
                }
            }
            if (failed) {
                // A failed round keeps the progress where it is; the next round
                // starts from the work already spent and moves on from there.
                continue;
            }
            publish(INDEX_PERMILLE_BUCKETS + INDEX_PERMILLE_PLACEMENT, 1000);

            size_t total = 0;
            for (const u64 word : occupied_) {
                total += std::popcount(word);
            }
            if (total != count) {
                continue;
            }

            rank_prefix_.assign((slots + 63) / 64 + 1, 0);
            u32 running = 0;
            for (size_t word = 0; word < occupied_.size(); ++word) {
                rank_prefix_[word] = running;
                running += std::popcount(occupied_[word]);
            }
            rank_prefix_[occupied_.size()] = running;

            // The rank mapping must be a permutation of [0, count).
            std::vector<u64> seen((count + 63) / 64, 0);
            bool permutation = true;
            for (u32 id = 0; id < count; ++id) {
                if ((id & (INDEX_CHECK_INTERVAL - 1)) == 0) {
                    if (stop.stop_requested()) {
                        return false;
                    }
                    publish(index_permille(INDEX_PERMILLE_BUCKETS + INDEX_PERMILLE_PLACEMENT,
                                           INDEX_PERMILLE_CHECK, id, count),
                            1000);
                }
                const u32 target = lookup(key(id));
                if (target >= count || ((seen[target >> 6] >> (target & 63)) & 1u) != 0) {
                    permutation = false;
                    break;
                }
                seen[target >> 6] |= u64(1) << (target & 63);
            }
            if (!permutation) {
                continue;
            }

            // Remap goal ids from insertion order to rank order before the key
            // array is permuted.
            if (ids_to_remap != nullptr) {
                for (u32& id : *ids_to_remap) {
                    id = lookup(key(id));
                }
            }

            // Reorder the dense keys so that key(id) is the key with rank id.
            // Follow the permutation cycles in place. A partition is often one
            // huge cycle, so progress and cancellation are tracked inside the
            // walk, not per cycle start.
            std::vector<u64> visited((count + 63) / 64, 0);
            std::array<u8, 16> carry{};
            size_t moved = 0;
            for (u32 start = 0; start < count; ++start) {
                if (((visited[start >> 6] >> (start & 63)) & 1u) != 0) {
                    continue;
                }
                std::memcpy(carry.data(), key(start), bytes_);
                u32 current = start;
                while (true) {
                    if ((moved & (INDEX_CHECK_INTERVAL - 1)) == 0) {
                        if (stop.stop_requested()) {
                            return false;
                        }
                        publish(index_permille(INDEX_PERMILLE_BUCKETS + INDEX_PERMILLE_PLACEMENT +
                                                   INDEX_PERMILLE_CHECK,
                                               1000 - INDEX_PERMILLE_BUCKETS -
                                                   INDEX_PERMILLE_PLACEMENT - INDEX_PERMILLE_CHECK,
                                               moved, count),
                                1000);
                    }
                    ++moved;
                    visited[current >> 6] |= u64(1) << (current & 63);
                    const u32 destination = lookup(carry.data());
                    if (destination == start) {
                        std::memcpy(mutable_key(start), carry.data(), bytes_);
                        break;
                    }
                    std::array<u8, 16> next{};
                    std::memcpy(next.data(), key(destination), bytes_);
                    std::memcpy(mutable_key(destination), carry.data(), bytes_);
                    std::memcpy(carry.data(), next.data(), bytes_);
                    current = destination;
                }
            }

            publish(1000, 1000);
            built_ = true;
            return true;
        }
    }

    return false;
}

auto placement_table::rank(const size_t position) const -> u32
{
    const size_t word = position >> 6;
    const size_t bit = position & 63;
    return rank_prefix_[word] +
        static_cast<u32>(std::popcount(occupied_[word] & ((u64(1) << bit) - 1)));
}

auto placement_table::lookup(const u8* key) const -> u32
{
    const u64 hash = hash_key(key, seed_);
    const size_t bucket = hash & bucket_mask_;
    // The displacement is multiplied by a second, independent hash of the key.
    // Two keys with the same low hash bits would otherwise be inseparable inside
    // one bucket, no matter which displacement the bucket picks.
    const u64 position =
        ((hash >> 32) + displacements_[bucket] * hash_key(key, slot_seed_)) & slot_mask_;
    return rank(position);
}

auto placement_table::key_equal(const u8* a, const u8* b) const -> bool
{
    return bytes_ <= 8 ? load_u64(a, bytes_) == load_u64(b, bytes_)
                       : std::memcmp(a, b, bytes_) == 0;
}

auto placement_table::find(const u8* key) const -> std::optional<u32>
{
    if (!built_ || size_ == 0 || keys_released_) {
        return std::nullopt;
    }
    const u32 id = lookup(key);
    if (id < size_ && key_equal(key, this->key(id))) {
        return id;
    }
    return std::nullopt;
}

auto placement_table::key(const u32 id) const -> const u8*
{
    return keys_.data() + static_cast<size_t>(id) * bytes_;
}

auto placement_table::mutable_key(const u32 id) -> u8*
{
    return keys_.data() + static_cast<size_t>(id) * bytes_;
}

auto placement_table::size() const -> size_t
{
    return size_;
}

auto placement_table::bytes() const -> size_t
{
    return bytes_;
}

auto placement_table::index_built() const -> bool
{
    return built_;
}

auto placement_table::release_keys() -> void
{
    keys_.clear();
    keys_.shrink_to_fit();
    keys_released_ = true;
}

auto placement_table::keys_released() const -> bool
{
    return keys_released_;
}

// ---------------------------------------------------------------------------
// placement_space
// ---------------------------------------------------------------------------

placement_space::placement_space(const puzzle_space_config& config, const std::stop_token stop)
    : width_(config.width), height_(config.height),
      cells_(static_cast<int>(config.width) * config.height), restricted_(config.restricted),
      goal_x_(config.goal_x), goal_y_(config.goal_y),
      goal_cell_(static_cast<int>(config.goal_y) * config.width + config.goal_x),
      target_area_(config.target.get_width() * config.target.get_height())
{
    const u64 board_mask = cells_ >= 64 ? ~u64{0} : ((u64{1} << cells_) - 1);

    if (config.wall_inventory_enabled) {
        if (config.wall_inventory.empty()) {
            error_ = "No wall shapes were provided for the wall inventory";
            return;
        }
        if (config.walls != 0) {
            error_ = "Fixed walls and a wall inventory are mutually exclusive";
            return;
        }
        wall_layout_space wall_space(config.width, config.height, config.wall_inventory,
                                     config.min_walls, config.max_walls, stop);
        if (wall_space.cancelled()) {
            error_ = "Wall layout enumeration cancelled";
            return;
        }
        if (!wall_space.error().empty()) {
            error_ = wall_space.error();
            return;
        }
        layouts_ = wall_space.layouts();
    } else {
        wall_layout fixed;
        fixed.mask = config.walls & board_mask;
        layouts_.push_back(fixed);
    }

    // Shape 0 is the target, followed by the inventory in a deterministic order.
    placement_shape target_shape;
    target_shape.shape = config.target;
    target_shape.width = config.target.get_width();
    target_shape.height = config.target.get_height();
    target_shape.target = true;
    target_shape.dirs = shape_dirs(config.target);
    shapes_.push_back(target_shape);

    std::vector<block> inventory;
    inventory.reserve(config.inventory.size());
    for (const block b : config.inventory) {
        inventory.push_back(block(0, 0, b.get_width(), b.get_height(), false, false));
    }
    std::ranges::sort(inventory,
                      [](const block a, const block b)
                      {
                          return std::tuple(a.get_height(), a.get_width()) <
                              std::tuple(b.get_height(), b.get_width());
                      });
    // Remove duplicate shapes defensively; the blockset2 should already do this.
    inventory.erase(std::unique(inventory.begin(), inventory.end()), inventory.end());

    for (const block b : inventory) {
        placement_shape shape;
        shape.shape = b;
        shape.width = b.get_width();
        shape.height = b.get_height();
        shape.target = false;
        shape.dirs = shape_dirs(b);
        shapes_.push_back(shape);
    }

    // The target may be placed anywhere its movement rules can reach. This is
    // the same range make_puzzle_space_config() derives; recomputing it makes
    // the candidate set closed under moves even if target_positions were set
    // more narrowly by a caller.
    int x0 = 0;
    int y0 = 0;
    int x1 = width_ - target_shape.width;
    int y1 = height_ - target_shape.height;
    if (restricted_ && target_shape.width != target_shape.height) {
        if (target_shape.width > target_shape.height) {
            y0 = y1 = goal_y_;
        } else {
            x0 = x1 = goal_x_;
        }
    }
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            target_range_mask_ |= u64{1} << (y * width_ + x);
        }
    }

    for (wall_layout& layout : layouts_) {
        layout.mask &= board_mask;
        layout.wall_cells = static_cast<int>(std::popcount(layout.mask));
        layout.free_cells = cells_ - layout.wall_cells;
        layout.target_allowed = target_range_mask_ & ~layout.mask;
        layout.target_last_cell = layout.target_allowed == 0
            ? -1
            : 63 - static_cast<int>(std::countl_zero(layout.target_allowed));
    }

    build_masks();
    build_partitions(config);
}

auto placement_space::layouts() const -> const std::vector<wall_layout>&
{
    return layouts_;
}

auto placement_space::wall_layout_count() const -> size_t
{
    return layouts_.size();
}

auto placement_space::error() const -> const std::string&
{
    return error_;
}

auto placement_space::shape_dirs(const block& b) const -> u8
{
    if (!restricted_) {
        return ALL_DIRS;
    }
    if (b.get_width() > b.get_height()) {
        return eas | wes;
    }
    if (b.get_height() > b.get_width()) {
        return nor | sou;
    }
    return ALL_DIRS;
}

auto placement_space::build_masks() -> void
{
    for (placement_shape& shape : shapes_) {
        for (int cell = 0; cell < cells_; ++cell) {
            const int x = cell % width_;
            const int y = cell / width_;
            if (x + shape.width > width_ || y + shape.height > height_) {
                continue;
            }
            u64 mask = 0;
            for (int dy = 0; dy < shape.height; ++dy) {
                for (int dx = 0; dx < shape.width; ++dx) {
                    mask |= u64(1) << ((y + dy) * width_ + x + dx);
                }
            }
            shape.masks[static_cast<size_t>(cell)] = mask;

            // Fold the movement rules and the board bounds into the move list, so
            // for_each_move() needs neither x/y, nor a direction switch, nor an
            // illegal-move test.
            placement_moves& moves = shape.moves[static_cast<size_t>(cell)];
            for (int dir = 0; dir < 4; ++dir) {
                if ((shape.dirs & (1u << dir)) == 0) {
                    continue;
                }
                int target_x = x;
                int target_y = y;
                switch (dir) {
                case 0: // nor
                    --target_y;
                    break;
                case 1: // eas
                    ++target_x;
                    break;
                case 2: // sou
                    ++target_y;
                    break;
                default: // wes
                    --target_x;
                    break;
                }
                const bool fits = target_x >= 0 && target_y >= 0 &&
                    target_x + shape.width <= width_ && target_y + shape.height <= height_;
                if (fits) {
                    moves.target[moves.count++] =
                        static_cast<std::int8_t>(target_y * width_ + target_x);
                }
            }
        }
    }
}

auto placement_space::build_partitions(const puzzle_space_config& config) -> void
{
    const int max_other = static_cast<int>(config.max_blocks) - 1;
    const int min_other = std::max(1, static_cast<int>(std::max<size_t>(1, config.min_blocks)) - 1);
    if (max_other < min_other || shapes_.size() < 2) {
        return;
    }

    const int inventory_shapes = static_cast<int>(shapes_.size()) - 1;
    std::vector<u8> counts(static_cast<size_t>(inventory_shapes), 0);

    const std::function<void(int, int, int, size_t, int)> generate =
        [&](const int shape_index, const int used_blocks, const int used_area,
            const size_t layout_index, const int available_area)
    {
        if (shape_index == inventory_shapes) {
            if (used_blocks >= min_other && used_blocks <= max_other) {
                placement_partition partition;
                partition.counts = counts;
                partition.wall_layout = static_cast<u32>(layout_index);
                partition.wall_pieces = layouts_[layout_index].pieces;
                partition.blocks = static_cast<size_t>(used_blocks) + 1;
                partition.area = static_cast<size_t>(used_area) + target_area_;
                partitions_.push_back(std::move(partition));
            }
            return;
        }

        const placement_shape& shape = shapes_[static_cast<size_t>(shape_index) + 1];
        const int max_count = std::min(
            max_other - used_blocks, (available_area - used_area) / static_cast<int>(shape.area()));
        for (int count = 0; count <= max_count; ++count) {
            counts[static_cast<size_t>(shape_index)] = static_cast<u8>(count);
            generate(shape_index + 1, used_blocks + count,
                     used_area + count * static_cast<int>(shape.area()), layout_index,
                     available_area);
        }
        counts[static_cast<size_t>(shape_index)] = 0;
    };

    for (size_t layout_index = 0; layout_index < layouts_.size(); ++layout_index) {
        const wall_layout& layout = layouts_[layout_index];
        if (layout.target_allowed == 0) {
            continue;
        }
        const int available_area = layout.free_cells - target_area_;
        if (available_area < 0) {
            continue;
        }
        generate(0, 0, 0, layout_index, available_area);
    }
}

auto placement_space::partitions() const -> const std::vector<placement_partition>&
{
    return partitions_;
}

auto placement_space::partitions() -> std::vector<placement_partition>&
{
    return partitions_;
}

auto placement_space::partition(const size_t index) const -> const placement_partition&
{
    return partitions_.at(index);
}

auto placement_space::partition(const size_t index) -> placement_partition&
{
    return partitions_.at(index);
}

auto placement_space::partition_count() const -> size_t
{
    return partitions_.size();
}

auto placement_space::width() const -> int
{
    return width_;
}

auto placement_space::height() const -> int
{
    return height_;
}

auto placement_space::key_bytes(const size_t blocks) const -> size_t
{
    return std::clamp<size_t>((6 * blocks + 7) / 8, 1, MAX_KEY_BYTES);
}

auto placement_space::free_cells(const placement_partition& partition) const -> size_t
{
    return static_cast<size_t>(layouts_[partition.wall_layout].free_cells);
}

auto placement_space::free_cells() const -> size_t
{
    return layouts_.empty() ? 0 : static_cast<size_t>(layouts_.front().free_cells);
}

auto placement_space::fixed_bytes() const -> size_t
{
    size_t bytes = partitions_.size() * sizeof(placement_partition);
    for (const placement_partition& partition : partitions_) {
        bytes += partition.counts.size();
    }
    bytes += shapes_.size() * sizeof(placement_shape);
    bytes += layouts_.size() * sizeof(wall_layout);
    return bytes;
}

auto placement_space::pack_key(const enumeration_state& state, u8* key) const -> void
{
    int total = 0;
    for (const u8 count : state.counts) {
        total += count;
    }
    const int bits = 6 * total;

    if (bits <= 64) {
        u64 word = 0;
        int bit = 0;
        for (size_t shape = 0; shape < shapes_.size(); ++shape) {
            for (u8 i = 0; i < state.counts[shape]; ++i) {
                word |= u64(state.cells[shape][i]) << bit;
                bit += 6;
            }
        }
        store_u64(word, static_cast<size_t>((bits + 7) / 8), key);
        return;
    }

    key_words words;
    int bit = 0;
    for (size_t shape = 0; shape < shapes_.size(); ++shape) {
        for (u8 i = 0; i < state.counts[shape]; ++i) {
            set_cell(words, bit, state.cells[shape][i]);
            bit += 6;
        }
    }
    store_key(words, static_cast<size_t>((bit + 7) / 8), key);
}

auto placement_space::decode_key(const placement_partition& partition, const u8* key,
                                 std::array<std::array<u8, puzzle::MAX_BLOCKS>, MAX_SHAPES>& cells,
                                 std::array<u8, MAX_SHAPES>& counts) const -> void
{
    counts.fill(0);
    int bit = 0;

    if (partition.blocks <= 10) {
        const u64 word = load_u64(key, key_bytes(partition.blocks));
        for (size_t shape = 0; shape < shapes_.size(); ++shape) {
            counts[shape] = shape == 0 ? 1 : partition.counts[shape - 1];
            for (u8 i = 0; i < counts[shape]; ++i) {
                cells[shape][i] = static_cast<u8>((word >> bit) & 0x3f);
                bit += 6;
            }
        }
        return;
    }

    const key_words words = load_key(key, key_bytes(partition.blocks));
    for (size_t shape = 0; shape < shapes_.size(); ++shape) {
        counts[shape] = shape == 0 ? 1 : partition.counts[shape - 1];
        for (u8 i = 0; i < counts[shape]; ++i) {
            cells[shape][i] = get_cell(words, bit);
            bit += 6;
        }
    }
}

auto placement_space::emit_state(const enumeration_state& state, emit_fn emit, void* context) const
    -> void
{
    u8 key[MAX_KEY_BYTES];
    pack_key(state, key);
    emit(context, key, state.cells[0][0] == goal_cell_);
}

auto placement_space::recurse(const placement_partition& partition, const wall_layout& layout,
                              const int pos, const u64 occupied, const int occupied_cells,
                              const int skipped, const int remaining_blocks,
                              const int remaining_area, enumeration_state& state,
                              const std::stop_token& stop, bool& cancelled, const bool pack_keys,
                              emit_fn emit, void* context) const -> void
{
    if (cancelled || stop.stop_requested()) {
        cancelled = true;
        return;
    }

    int cell = pos;
    while (cell < cells_ && ((occupied >> cell) & 1u) != 0) {
        ++cell;
    }

    // Once the last possible target cell has been passed, the target can never
    // be placed anymore.
    if (state.target_cell < 0 && cell > layout.target_last_cell) {
        return;
    }

    const int total_other = static_cast<int>(partition.blocks) - 1;
    if (state.target_cell >= 0 && state.placed_other == total_other) {
        if (pack_keys) {
            emit_state(state, emit, context);
        } else {
            emit(context, nullptr, state.cells[0][0] == goal_cell_);
        }
        return;
    }

    if (cell >= cells_) {
        return;
    }

    const int remaining_cells = cells_ - occupied_cells - skipped;
    if (remaining_blocks > remaining_cells || remaining_area > remaining_cells) {
        return;
    }

    // Place the target.
    if (state.target_cell < 0 && ((layout.target_allowed >> cell) & 1u) != 0) {
        const u64 mask = shapes_[0].masks[static_cast<size_t>(cell)];
        if (mask != 0 && (mask & occupied) == 0) {
            state.target_cell = cell;
            state.cells[0][0] = static_cast<u8>(cell);
            state.counts[0] = 1;
            recurse(partition, layout, cell + 1, occupied | mask,
                    occupied_cells + std::popcount(mask), skipped, remaining_blocks - 1,
                    remaining_area - target_area_, state, stop, cancelled, pack_keys, emit,
                    context);
            state.counts[0] = 0;
            state.target_cell = -1;
        }
    }

    // Place one of the remaining inventory blocks.
    for (size_t group = 0; group < partition.counts.size(); ++group) {
        if (state.counts[group + 1] >= partition.counts[group]) {
            continue;
        }
        const placement_shape& shape = shapes_[group + 1];
        const u64 mask = shape.masks[static_cast<size_t>(cell)];
        if (mask == 0 || (mask & occupied) != 0) {
            continue;
        }
        state.cells[group + 1][state.counts[group + 1]] = static_cast<u8>(cell);
        ++state.counts[group + 1];
        ++state.placed_other;
        recurse(partition, layout, cell + 1, occupied | mask, occupied_cells + std::popcount(mask),
                skipped, remaining_blocks - 1, remaining_area - static_cast<int>(shape.area()),
                state, stop, cancelled, pack_keys, emit, context);
        --state.placed_other;
        --state.counts[group + 1];
    }

    // Leave this cell empty and continue with the next one.
    recurse(partition, layout, cell + 1, occupied, occupied_cells, skipped + 1, remaining_blocks,
            remaining_area, state, stop, cancelled, pack_keys, emit, context);
}

auto placement_space::enumerate(const size_t index, const std::stop_token stop, emit_fn emit,
                                void* context, bool& cancelled, const bool pack_keys) const
    -> size_t
{
    const placement_partition& partition = partitions_.at(index);
    if (partition.blocks == 0) {
        return 0;
    }

    struct forward_context
    {
        emit_fn emit = nullptr;
        void* context = nullptr;
        size_t count = 0;
    };
    forward_context forward{emit, context, 0};

    const auto forward_emit = [](void* raw, const u8* key, const bool goal)
    {
        auto* ctx = static_cast<forward_context*>(raw);
        ++ctx->count;
        ctx->emit(ctx->context, key, goal);
    };

    const wall_layout& layout = layouts_.at(partition.wall_layout);
    enumeration_state state;
    state.target_cell = -1;
    recurse(partition, layout, 0, layout.mask, layout.wall_cells, 0,
            static_cast<int>(partition.blocks), static_cast<int>(partition.area), state, stop,
            cancelled, pack_keys, forward_emit, &forward);
    return forward.count;
}

auto placement_space::count(const size_t index, const std::stop_token stop, bool& cancelled) const
    -> std::pair<size_t, bool>
{
    struct count_context
    {
        bool goal = false;
    };
    count_context context;

    const auto count_emit = [](void* raw, const u8*, const bool goal)
    {
        if (goal) {
            static_cast<count_context*>(raw)->goal = true;
        }
    };

    // Counting only needs the number of placements and whether any of them is a
    // goal, so the packed key is skipped.
    const size_t count = enumerate(index, stop, count_emit, &context, cancelled, false);
    return {count, context.goal};
}

auto placement_space::for_each_move(const placement_partition& partition, const u8* key,
                                    move_fn callback, void* context) const -> void
{
    // decode_key() fills every entry that is read below, so no zero-init is needed.
    std::array<std::array<u8, puzzle::MAX_BLOCKS>, MAX_SHAPES> cells;
    std::array<u8, MAX_SHAPES> counts;
    decode_key(partition, key, cells, counts);

    u64 occupied = layouts_.at(partition.wall_layout).mask;
    for (size_t shape = 0; shape < shapes_.size(); ++shape) {
        for (u8 i = 0; i < counts[shape]; ++i) {
            occupied |= shapes_[shape].masks[cells[shape][i]];
        }
    }

    // Bit offset of every shape group inside the packed key. A group's cells are
    // sorted and occupy one contiguous 6-bit-per-cell range, so a move can splice
    // that range instead of repacking the whole key.
    std::array<size_t, MAX_SHAPES> group_bit{};
    size_t bit = 0;
    for (size_t shape = 0; shape < shapes_.size(); ++shape) {
        group_bit[shape] = bit;
        bit += 6 * counts[shape];
    }

    const size_t bytes = key_bytes(partition.blocks);
    // Keys up to 64 bits use one word and plain 64-bit splicing; wider keys use
    // 128 bits, because a single group (up to 15 cells = 90 bits) may straddle
    // the 64-bit boundary.
    const u64 word = bytes <= 8 ? load_u64(key, bytes) : 0;
    unsigned __int128 wide = 0;
    if (bytes > 8) {
        const key_words words = load_key(key, bytes);
        wide = (static_cast<unsigned __int128>(words.hi) << 64) | words.lo;
    }

    u8 moved_key[MAX_KEY_BYTES];
    std::array<u8, puzzle::MAX_BLOCKS> group{};
    for (size_t shape = 0; shape < shapes_.size(); ++shape) {
        const placement_shape& block_shape = shapes_[shape];
        const u8 count = counts[shape];
        const size_t off = group_bit[shape];
        for (u8 i = 0; i < count; ++i) {
            const int cell = cells[shape][i];
            const u64 rest = occupied & ~block_shape.masks[static_cast<size_t>(cell)];
            const placement_moves& moves = block_shape.moves[static_cast<size_t>(cell)];

            // The trip count is constant for a given (shape, cell), so neither the
            // count nor the legality of a direction is data dependent.
            for (u8 move = 0; move < moves.count; ++move) {
                const int target = moves.target[move];
                // The current anchor is a legal placement, so an in-board move of
                // one cell always fits; only the collision with the other blocks
                // (and the walls, which are part of `occupied`) has to be tested.
                if ((block_shape.masks[static_cast<size_t>(target)] & rest) != 0) {
                    continue;
                }

                // Single-cell groups are the common case and need no sorted-group
                // handling at all. The splice is repeated here on purpose: keeping the
                // whole path branch-free for `count == 1` is ~5% of the generator.
                if (count == 1) {
                    if (bytes <= 8) {
                        const u64 mask = ((u64{1} << 6) - 1) << off;
                        const u64 moved = (word & ~mask) | (static_cast<u64>(target) << off);
                        std::memcpy(moved_key, &moved, sizeof(moved));
                    } else {
                        const unsigned __int128 mask =
                            ((static_cast<unsigned __int128>(1) << 6) - 1) << off;
                        const unsigned __int128 moved =
                            (wide & ~mask) | (static_cast<unsigned __int128>(target) << off);
                        key_words words;
                        words.lo = static_cast<u64>(moved);
                        words.hi = static_cast<u64>(moved >> 64);
                        store_key(words, bytes, moved_key);
                    }
                    callback(context, moved_key);
                    continue;
                }

                // The group is already sorted and exactly one cell changes, so one
                // element is removed and inserted in place instead of sorting.
                int written = 0;
                for (u8 k = 0; k < count; ++k) {
                    if (k != i) {
                        group[static_cast<size_t>(written++)] = cells[shape][k];
                    }
                }
                int at = 0;
                while (at < written && group[static_cast<size_t>(at)] < target) {
                    ++at;
                }
                for (int k = written; k > at; --k) {
                    group[static_cast<size_t>(k)] = group[static_cast<size_t>(k - 1)];
                }
                group[static_cast<size_t>(at)] = static_cast<u8>(target);

                if (bytes <= 8) {
                    u64 group_bits = 0;
                    for (u8 k = 0; k < count; ++k) {
                        group_bits |= u64(group[k]) << (6 * k);
                    }
                    // 6 * count <= 64 in this branch, so the mask cannot overflow.
                    const u64 mask = ((u64{1} << (6 * count)) - 1) << off;
                    const u64 moved = (word & ~mask) | (group_bits << off);
                    std::memcpy(moved_key, &moved, sizeof(moved));
                } else {
                    unsigned __int128 group_bits = 0;
                    for (u8 k = 0; k < count; ++k) {
                        group_bits |= static_cast<unsigned __int128>(group[k]) << (6 * k);
                    }
                    const unsigned __int128 mask =
                        ((static_cast<unsigned __int128>(1) << (6 * count)) - 1) << off;
                    const unsigned __int128 moved = (wide & ~mask) | (group_bits << off);
                    key_words words;
                    words.lo = static_cast<u64>(moved);
                    words.hi = static_cast<u64>(moved >> 64);
                    store_key(words, bytes, moved_key);
                }

                callback(context, moved_key);
            }
        }
    }
}

auto placement_space::is_goal(const u8* key) const -> bool
{
    return static_cast<int>(key[0] & 0x3f) == goal_cell_;
}

auto placement_space::to_puzzle(const placement_partition& partition, const u8* key) const -> puzzle
{
    std::array<std::array<u8, puzzle::MAX_BLOCKS>, MAX_SHAPES> cells{};
    std::array<u8, MAX_SHAPES> counts{};
    decode_key(partition, key, cells, counts);

    std::array<u16, puzzle::MAX_BLOCKS> blocks{};
    blocks.fill(0x8000);
    size_t index = 0;
    for (size_t shape = 0; shape < shapes_.size(); ++shape) {
        const placement_shape& block_shape = shapes_[shape];
        for (u8 i = 0; i < counts[shape]; ++i) {
            const int cell = cells[shape][i];
            const int x = cell % width_;
            const int y = cell / width_;
            blocks[index++] =
                static_cast<u16>((shape == 0 ? 2 : 0) | ((block_shape.width - 1) << 2) |
                                 ((block_shape.height - 1) << 5) | (x << 8) | (y << 11));
        }
    }

    return puzzle(static_cast<u8>(width_), static_cast<u8>(height_), static_cast<u8>(goal_x_),
                  static_cast<u8>(goal_y_), restricted_, true, blocks,
                  layouts_.at(partition.wall_layout).mask);
}

auto placement_space::state_repr(const placement_partition& partition, const u8* key) const
    -> std::array<u64, 4>
{
    std::array<std::array<u8, puzzle::MAX_BLOCKS>, MAX_SHAPES> cells{};
    std::array<u8, MAX_SHAPES> counts{};
    decode_key(partition, key, cells, counts);

    std::array<u16, puzzle::MAX_BLOCKS> blocks{};
    blocks.fill(0x8000);
    size_t index = 0;
    for (size_t shape = 0; shape < shapes_.size(); ++shape) {
        const placement_shape& block_shape = shapes_[shape];
        for (u8 i = 0; i < counts[shape]; ++i) {
            const int cell = cells[shape][i];
            const int x = cell % width_;
            const int y = cell / width_;
            blocks[index++] =
                static_cast<u16>((shape == 0 ? 2 : 0) | ((block_shape.width - 1) << 2) |
                                 ((block_shape.height - 1) << 5) | (x << 8) | (y << 11));
        }
    }
    std::sort(blocks.begin(), blocks.end());

    const u16 meta = static_cast<u16>(((width_ - 1) << 7) | ((height_ - 1) << 10) | (goal_x_ << 1) |
                                      (goal_y_ << 4) | (restricted_ ? 1 : 0) | (1 << 13));

    std::array<u64, 4> raw{};
    raw[0] = u64(meta) | (u64(blocks[0]) << 16) | (u64(blocks[1]) << 32) | (u64(blocks[2]) << 48);
    raw[1] =
        u64(blocks[3]) | (u64(blocks[4]) << 16) | (u64(blocks[5]) << 32) | (u64(blocks[6]) << 48);
    raw[2] =
        u64(blocks[7]) | (u64(blocks[8]) << 16) | (u64(blocks[9]) << 32) | (u64(blocks[10]) << 48);
    raw[3] = u64(blocks[11]) | (u64(blocks[12]) << 16) | (u64(blocks[13]) << 32) |
        (u64(blocks[14]) << 48);
    return raw;
}

auto placement_repr_less(const std::array<u64, 4>& a, const std::array<u64, 4>& b) -> bool
{
    return repr_less(a, b);
}
