#include "placement_space.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <set>
#include <stop_token>
#include <string>

namespace
{
auto rush_hour_board() -> puzzle
{
    return puzzle(
        std::string("S:[6x6] G:[4,2] M:[R] B:[{3x1 _ _ _ _ 1x3} {_ _ _ _ _ _} {_ _ 1x2 2X1 _ _} "
                    "{_ _ _ 1x2 2x1 _} {1x2 _ 1x2 _ 2x1 _} {_ _ _ 3x1 _ _}]"));
}

auto klotski_board() -> puzzle
{
    return puzzle(std::string("S:[4x5] G:[1,3] M:[F] B:[{1x2 2X2 _ 1x2} {_ _ _ _} {1x2 2x1 _ "
                              "1x2} {_ 1x1 1x1 _} {1x1 _ _ 1x1}]"));
}

auto make_config(const puzzle& board, const size_t max_blocks, const int min_moves,
                 const size_t min_blocks = 2, const wall_search_options& walls = {})
    -> puzzle_space_config
{
    const puzzle_space_config_result built =
        make_puzzle_space_config(board, max_blocks, min_moves, min_blocks, walls);
    REQUIRE(built.config.has_value());
    return *built.config;
}

auto key_to_puzzle(const placement_space& space, const placement_partition& partition,
                   const u8* key) -> puzzle
{
    return space.to_puzzle(partition, key);
}

auto walled_board() -> puzzle
{
    return puzzle(
        std::string("S:[5x5] G:[4,2] M:[F] B:[{1X1 1x1 _ _ _} {_ 1*1 _ _ _} {_ _ 1*1 _ _} "
                    "{_ _ _ _ _} {_ _ _ _ _}]"));
}

// 11 one-cell blocks on a 3x4 board: 66-bit keys, so a group straddles the
// 64-bit boundary of the packed key.
auto wide_key_board() -> puzzle
{
    const puzzle empty(3, 4, 0, 0, false, true);
    std::optional<puzzle> board = empty.try_add_block(block(0, 0, 1, 1, true, false));
    REQUIRE(board);
    board = board->try_add_block(block(1, 0, 1, 1, false, false));
    REQUIRE(board);
    return *board;
}

struct move_checksum
{
    size_t states = 0;
    size_t moves = 0;
    u64 hash = 0xcbf29ce484222325ull;
};

auto fnv1a(const u8* data, const size_t size) -> u64
{
    u64 hash = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= 0x100000001b3ull;
    }
    return hash;
}

auto mix_hash(u64 value) -> u64
{
    value += 0x9e3779b97f4a7c15ull;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ull;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebull;
    return value ^ (value >> 31);
}

struct checksum_context
{
    u64 sum = 0;
    size_t moves = 0;
    size_t bytes = 0;
};

auto checksum_move(void* raw, const u8* moved) -> void
{
    auto* context = static_cast<checksum_context*>(raw);
    context->sum += fnv1a(moved, context->bytes);
    ++context->moves;
}

/**
 * Order-independent checksum of the complete move set of one configuration. The
 * FNV-1a hashes of a state's moves are summed (so the direction/block iteration
 * order does not matter) and combined with that state's own key, then all states
 * are summed (so the rank-id order of the index does not matter either). This
 * pins exactly the (state -> move multiset) relation, independently of how the
 * perfect-hash index happens to lay the states out.
 */
auto checksum_moves(const puzzle_space_config& config) -> move_checksum
{
    move_checksum result;
    const placement_space space(config);

    for (size_t index = 0; index < space.partition_count(); ++index) {
        const placement_partition& partition = space.partition(index);
        const size_t bytes = space.key_bytes(partition.blocks);

        placement_table table(0, bytes);
        struct emit_context
        {
            placement_table* table = nullptr;
        } emit_ctx{&table};
        const auto emit = [](void* raw, const u8* key, bool)
        { static_cast<emit_context*>(raw)->table->insert(key); };

        bool cancelled = false;
        space.enumerate(index, {}, emit, &emit_ctx, cancelled);
        REQUIRE_FALSE(cancelled);
        if (table.size() == 0) {
            continue;
        }
        table.build_index();
        REQUIRE(table.index_built());

        for (u32 id = 0; id < table.size(); ++id) {
            const u8* key = table.key(id);
            checksum_context moves{0, 0, bytes};
            space.for_each_move(partition, key, checksum_move, &moves);
            result.hash += mix_hash(moves.sum ^ mix_hash(fnv1a(key, bytes)));
            result.states += 1;
            result.moves += moves.moves;
        }
    }

    return result;
}
} // namespace

TEST_CASE("placement table grows when the size estimate is too low", "[placement_space]")
{
    placement_table table(0, 2);
    std::set<std::array<u8, 16>> inserted;
    for (u64 i = 0; i < 2'000; ++i) {
        u8 key[16] = {};
        key[0] = static_cast<u8>(i & 0xff);
        key[1] = static_cast<u8>((i >> 8) & 0xff);
        CHECK(table.insert(key) == static_cast<u32>(i));
        std::array<u8, 16> copy{};
        std::memcpy(copy.data(), key, 2);
        inserted.insert(copy);
    }
    CHECK(table.size() == 2'000);
    table.build_index();
    REQUIRE(table.index_built());

    std::set<std::array<u8, 16>> stored;
    for (u32 i = 0; i < 2'000; ++i) {
        std::array<u8, 16> copy{};
        std::memcpy(copy.data(), table.key(i), 2);
        stored.insert(copy);
    }
    CHECK(stored == inserted);

    for (u64 i = 0; i < 2'000; ++i) {
        u8 key[16] = {};
        key[0] = static_cast<u8>(i & 0xff);
        key[1] = static_cast<u8>((i >> 8) & 0xff);
        INFO("i=" << i);
        const std::optional<u32> found = table.find(key);
        REQUIRE(found.has_value());
        CHECK(*found < 2'000);
        CHECK(std::memcmp(table.key(*found), key, 2) == 0);
    }

    u8 missing[16] = {};
    missing[0] = static_cast<u8>(9'999 & 0xff);
    missing[1] = static_cast<u8>((9'999 >> 8) & 0xff);
    CHECK_FALSE(table.find(missing).has_value());
}

TEST_CASE("placement-space counts match the known state counts", "[placement_space]")
{
    struct case_t
    {
        size_t max_blocks;
        size_t expected;
    };

    // Exact placement counts (target plus 1..max_blocks-1 other blocks) for the
    // 6x6 Rush Hour ruleset. The 4-block count (430,696) is covered by the
    // opt-in integration test; keep the default unit test fast.
    for (const case_t test : {case_t{2, 463}, case_t{3, 18'823}}) {
        const puzzle_space_config config = make_config(rush_hour_board(), test.max_blocks, 0);
        const placement_space space(config);

        size_t total = 0;
        for (size_t i = 0; i < space.partition_count(); ++i) {
            bool cancelled = false;
            const auto [count, has_goal] = space.count(i, {}, cancelled);
            REQUIRE_FALSE(cancelled);
            CHECK(count > 0);
            CHECK(has_goal);
            total += count;
        }
        INFO("max_blocks=" << test.max_blocks);
        CHECK(total == test.expected);
    }
}

TEST_CASE("placement-space partitions respect the block range", "[placement_space]")
{
    const placement_space all(make_config(rush_hour_board(), 4, 0, 2));
    const placement_space filtered(make_config(rush_hour_board(), 4, 0, 3));

    REQUIRE(filtered.partition_count() > 0);
    REQUIRE(filtered.partition_count() < all.partition_count());
    for (size_t i = 0; i < filtered.partition_count(); ++i) {
        INFO("partition " << i);
        CHECK(filtered.partition(i).blocks >= 3);
        CHECK(filtered.partition(i).blocks <= 4);
    }
}

TEST_CASE("placement-space enumeration is unique and every move stays in the partition",
          "[placement_space]")
{
    const puzzle_space_config config = make_config(rush_hour_board(), 3, 0);
    const placement_space space(config);
    REQUIRE(space.partition_count() > 0);

    for (size_t index = 0; index < space.partition_count(); ++index) {
        const placement_partition& partition = space.partition(index);
        INFO("partition " << index << " blocks=" << partition.blocks);

        bool count_cancelled = false;
        const auto [expected, expected_goal] = space.count(index, {}, count_cancelled);
        REQUIRE_FALSE(count_cancelled);
        REQUIRE(expected_goal);
        REQUIRE(expected > 0);

        placement_table table(expected, space.key_bytes(partition.blocks));
        struct emit_context
        {
            placement_table* table = nullptr;
            size_t count = 0;
            size_t goal_count = 0;
        } context{&table, 0, 0};

        const auto emit = [](void* raw, const u8* key, const bool goal)
        {
            auto* ctx = static_cast<emit_context*>(raw);
            ctx->table->insert(key);
            ++ctx->count;
            if (goal) {
                ++ctx->goal_count;
            }
        };

        bool cancelled = false;
        const size_t emitted = space.enumerate(index, {}, emit, &context, cancelled);
        REQUIRE_FALSE(cancelled);
        CHECK(emitted == expected);
        CHECK(context.count == expected);
        CHECK(context.goal_count > 0);
        table.build_index();
        REQUIRE(table.index_built());

        // Sample states to keep the test fast, including the first and last.
        const size_t sample_count = std::min<size_t>(expected, 16);
        for (size_t sample = 0; sample < sample_count; ++sample) {
            const size_t id = sample * (expected - 1) / std::max<size_t>(1, sample_count - 1);
            const u8* key = table.key(static_cast<u32>(id));
            const std::optional<u32> resolved = table.find(key);
            REQUIRE(resolved.has_value());
            CHECK(*resolved == static_cast<u32>(id));

            std::set<std::array<u8, 16>> move_keys;
            struct move_capture
            {
                std::set<std::array<u8, 16>>* keys = nullptr;
                size_t bytes = 0;
            } capture{&move_keys, space.key_bytes(partition.blocks)};
            const auto collect_move = [](void* raw, const u8* moved)
            {
                auto* ctx = static_cast<move_capture*>(raw);
                std::array<u8, 16> copy{};
                std::memcpy(copy.data(), moved, ctx->bytes);
                ctx->keys->insert(copy);
            };
            space.for_each_move(partition, key, collect_move, &capture);

            for (const auto& moved : move_keys) {
                INFO("state " << id);
                CHECK(table.find(moved.data()).has_value());
            }

            // Compare with the production move rules.
            puzzleset expected_neighbors;
            key_to_puzzle(space, partition, key)
                .for_each_adjacent([&](const puzzle& next) { expected_neighbors.insert(next); });

            puzzleset actual_neighbors;
            for (const auto& moved : move_keys) {
                actual_neighbors.insert(key_to_puzzle(space, partition, moved.data()));
            }

            INFO("state " << id << " expected=" << expected_neighbors.size()
                          << " actual=" << actual_neighbors.size());
            CHECK(actual_neighbors.size() == expected_neighbors.size());
            for (const puzzle& neighbor : expected_neighbors) {
                CHECK(actual_neighbors.contains(neighbor));
            }

            const puzzle state = key_to_puzzle(space, partition, key);
            CHECK_FALSE(state.try_get_invalid_reason().has_value());
            CHECK(state.block_count() == partition.blocks);
            CHECK(state.goal_reached() == space.is_goal(key));
        }
    }
}

TEST_CASE("placement-space handles keys wider than 64 bits", "[placement_space]")
{
    // 11 one-cell blocks on a 3x4 board: the key is 66 bits, so the cell at
    // bit 60 straddles the 64-bit boundary.
    const puzzle empty(3, 4, 0, 0, false, true);
    std::optional<puzzle> board = empty.try_add_block(block(0, 0, 1, 1, true, false));
    REQUIRE(board);
    board = board->try_add_block(block(1, 0, 1, 1, false, false));
    REQUIRE(board);

    const puzzle_space_config config = make_config(*board, 11, 0);
    const placement_space space(config);
    CHECK(space.key_bytes(11) == 9);

    size_t index = SIZE_MAX;
    for (size_t i = 0; i < space.partition_count(); ++i) {
        if (space.partition(i).blocks == 11) {
            index = i;
            break;
        }
    }
    REQUIRE(index != SIZE_MAX);

    bool cancelled = false;
    const auto [count, has_goal] = space.count(index, {}, cancelled);
    REQUIRE_FALSE(cancelled);
    REQUIRE(count > 0);
    CHECK(has_goal);

    placement_table table(count, space.key_bytes(11));
    struct emit_context
    {
        placement_table* table = nullptr;
        size_t inserted = 0;
    } context{&table, 0};

    const auto emit = [](void* raw, const u8* key, const bool /*goal*/)
    {
        auto* ctx = static_cast<emit_context*>(raw);
        ctx->table->insert(key);
        ++ctx->inserted;
    };

    const size_t emitted = space.enumerate(index, {}, emit, &context, cancelled);
    REQUIRE_FALSE(cancelled);
    CHECK(emitted == count);
    CHECK(context.inserted == count);
    table.build_index();
    REQUIRE(table.index_built());
    for (size_t id = 0; id < count; ++id) {
        const std::optional<u32> found = table.find(table.key(static_cast<u32>(id)));
        REQUIRE(found.has_value());
        CHECK(*found == static_cast<u32>(id));
    }

    const placement_partition& partition = space.partition(index);
    const size_t sample_count = std::min<size_t>(count, 16);
    for (size_t sample = 0; sample < sample_count; ++sample) {
        const size_t id = sample * (count - 1) / std::max<size_t>(1, sample_count - 1);
        const u8* key = table.key(static_cast<u32>(id));

        std::set<std::array<u8, 16>> move_keys;
        struct move_capture
        {
            std::set<std::array<u8, 16>>* keys = nullptr;
            size_t bytes = 0;
        } capture{&move_keys, space.key_bytes(partition.blocks)};
        const auto collect = [](void* raw, const u8* moved)
        {
            auto* ctx = static_cast<move_capture*>(raw);
            std::array<u8, 16> copy{};
            std::memcpy(copy.data(), moved, ctx->bytes);
            ctx->keys->insert(copy);
        };
        space.for_each_move(partition, key, collect, &capture);

        puzzleset expected_neighbors;
        key_to_puzzle(space, partition, key)
            .for_each_adjacent([&](const puzzle& next) { expected_neighbors.insert(next); });
        puzzleset actual_neighbors;
        for (const auto& moved : move_keys) {
            actual_neighbors.insert(key_to_puzzle(space, partition, moved.data()));
        }
        CHECK(actual_neighbors.size() == expected_neighbors.size());
        for (const puzzle& neighbor : expected_neighbors) {
            CHECK(actual_neighbors.contains(neighbor));
        }

        const puzzle state = key_to_puzzle(space, partition, key);
        CHECK_FALSE(state.try_get_invalid_reason().has_value());
        CHECK(state.block_count() == partition.blocks);
        CHECK(state.goal_reached() == space.is_goal(key));
    }
}

TEST_CASE("walls are stored separately and prune placements", "[placement_space]")
{
    const puzzle walled(
        std::string("S:[5x5] G:[4,2] M:[F] B:[{1X1 1x1 _ _ _} {_ 1*1 _ _ _} {_ _ 1*1 _ _} "
                    "{_ _ _ _ _} {_ _ _ _ _}]"));
    REQUIRE(walled.block_count() == 2);
    CHECK(walled.wall_count() == 2);
    CHECK(walled.wall_at(1, 1));
    CHECK(walled.wall_at(2, 2));

    const puzzle roundtrip(walled.string_repr());
    CHECK(roundtrip == walled);
    CHECK(roundtrip.wall_count() == 2);
    CHECK(roundtrip.block_count() == 2);

    const puzzle free(
        std::string("S:[5x5] G:[4,2] M:[F] B:[{1X1 1x1 _ _ _} {_ _ _ _ _} {_ _ _ _ _} "
                    "{_ _ _ _ _} {_ _ _ _ _}]"));

    const puzzle_space_config walled_config = make_config(walled, 3, 0);
    const puzzle_space_config free_config = make_config(free, 3, 0);
    CHECK(walled_config.walls != 0);
    CHECK(free_config.walls == 0);

    const placement_space walled_space(walled_config);
    const placement_space free_space(free_config);
    size_t walled_total = 0;
    size_t free_total = 0;
    for (size_t i = 0; i < walled_space.partition_count(); ++i) {
        bool cancelled = false;
        walled_total += walled_space.count(i, {}, cancelled).first;
        REQUIRE_FALSE(cancelled);
    }
    for (size_t i = 0; i < free_space.partition_count(); ++i) {
        bool cancelled = false;
        free_total += free_space.count(i, {}, cancelled).first;
        REQUIRE_FALSE(cancelled);
    }
    CHECK(walled_total > 0);
    CHECK(walled_total < free_total);

    for (size_t i = 0; i < walled_space.partition_count(); ++i) {
        const placement_partition& partition = walled_space.partition(i);
        bool count_cancelled = false;
        const size_t expected = walled_space.count(i, {}, count_cancelled).first;
        REQUIRE_FALSE(count_cancelled);
        const size_t bytes = walled_space.key_bytes(partition.blocks);
        placement_table table(expected, bytes);
        struct context_t
        {
            const placement_space* space = nullptr;
            const placement_partition* partition = nullptr;
            const puzzle_space_config* config = nullptr;
            size_t checked = 0;
        } context{&walled_space, &partition, &walled_config, 0};

        const auto emit = [](void* raw, const u8* key, bool)
        {
            auto* ctx = static_cast<context_t*>(raw);
            const puzzle state = ctx->space->to_puzzle(*ctx->partition, key);
            CHECK(state.wall_mask() == ctx->config->walls);
            CHECK_FALSE(state.try_get_invalid_reason().has_value());
            ++ctx->checked;
        };
        bool cancelled = false;
        walled_space.enumerate(i, {}, emit, &context, cancelled);
        REQUIRE_FALSE(cancelled);
        CHECK(context.checked == expected);
    }
}

TEST_CASE("wall inventory replaces fixed walls in the puzzle-space configuration", "[puzzle_space]")
{
    const puzzle board(std::string("S:[4x4] G:[3,3] M:[F] B:[{1X1 1x1 _ _} {2*1 _ _ _} {_ _ _ 1*1} "
                                   "{_ _ _ _}]"));
    const wall_search_options options{true, 0, 2};
    const puzzle_space_config config = make_config(board, 3, 0, 2, options);
    CHECK(config.wall_inventory_enabled);
    CHECK(config.walls == 0);
    CHECK(config.wall_inventory.size() == 2);
    CHECK(config.min_walls == 0);
    CHECK(config.max_walls == 2);

    const placement_space space(config);
    CHECK(space.error().empty());
    CHECK(space.wall_layout_count() > 1);
    for (size_t i = 0; i < space.partition_count(); ++i) {
        const placement_partition& partition = space.partition(i);
        const wall_layout& layout = space.layouts()[partition.wall_layout];
        CHECK(partition.wall_pieces == layout.pieces);
        CHECK(partition.wall_pieces <= 2);
    }
}

TEST_CASE("wall blocks can be placed directly", "[puzzle]")
{
    const puzzle p(4, 4, 0, 0, false, false);
    const std::optional<puzzle> walled = p.try_add_wall_block(block(0, 0, 2, 2, false, false));
    REQUIRE(walled);
    CHECK(walled->block_count() == 0);
    CHECK(walled->wall_count() == 4);
    CHECK(walled->wall_at(0, 0));
    CHECK(walled->wall_at(1, 1));

    const std::vector<block> shapes = walled->wall_rectangles();
    REQUIRE(shapes.size() == 1);
    CHECK(shapes[0].get_width() == 2);
    CHECK(shapes[0].get_height() == 2);
}

TEST_CASE("wall rectangles reconstruct rectangular wall shapes", "[puzzle]")
{
    const puzzle p(std::string("S:[4x4] G:[3,3] M:[F] B:[{1X1 1x1 _ _} {2*1 _ _ _} {_ _ _ 1*1} "
                               "{_ _ _ _}]"));
    const std::vector<block> shapes = p.wall_rectangles();
    REQUIRE(shapes.size() == 2);

    bool found_horizontal = false;
    bool found_single = false;
    for (const block shape : shapes) {
        found_horizontal |= shape.get_width() == 2 && shape.get_height() == 1;
        found_single |= shape.get_width() == 1 && shape.get_height() == 1;
        if (shape.get_width() == 2) {
            CHECK(shape.get_x() == 0);
            CHECK(shape.get_y() == 1);
        } else {
            CHECK(shape.get_x() == 3);
            CHECK(shape.get_y() == 2);
        }
    }
    CHECK(found_horizontal);
    CHECK(found_single);
}

TEST_CASE("wall layout enumeration covers wall placements and deduplicates masks",
          "[placement_space]")
{
    blockset2 single;
    single.insert(block(0, 0, 1, 1, false, false));

    const wall_layout_space one_piece(3, 3, single, 1, 1);
    CHECK(one_piece.error().empty());
    CHECK(one_piece.layouts().size() == 9);

    const wall_layout_space two_pieces(3, 3, single, 2, 2);
    CHECK(two_pieces.error().empty());
    CHECK(two_pieces.layouts().size() == 36);

    const wall_layout_space empty(3, 3, single, 0, 0);
    REQUIRE(empty.layouts().size() == 1);
    CHECK(empty.layouts()[0].mask == 0);

    blockset2 mixed;
    mixed.insert(block(0, 0, 1, 1, false, false));
    mixed.insert(block(0, 0, 2, 1, false, false));
    const wall_layout_space dedup(3, 3, mixed, 2, 2);
    std::set<u64> masks;
    for (const wall_layout& layout : dedup.layouts()) {
        CHECK(masks.insert(layout.mask).second);
    }
}

TEST_CASE("wall layer does not consume block slots", "[puzzle]")
{
    puzzle p(4, 4, 0, 0, false, false);
    for (u8 i = 0; i < 8; ++i) {
        const std::optional<puzzle> next = p.try_add_block(block(i % 4, i / 4, 1, 1, false, false));
        REQUIRE(next);
        p = *next;
    }
    for (u8 i = 8; i < 16; ++i) {
        const std::optional<puzzle> next = p.try_add_wall(i % 4, i / 4);
        REQUIRE(next);
        p = *next;
    }
    CHECK(p.block_count() == 8);
    CHECK(p.wall_count() == 8);

    const puzzle parsed(p.string_repr());
    CHECK(parsed == p);
    CHECK(parsed.block_count() == 8);
    CHECK(parsed.wall_count() == 8);
}

TEST_CASE("toggling a block into a wall removes the block", "[puzzle]")
{
    puzzle p(4, 4, 0, 0, false, false);
    const std::optional<puzzle> with_block = p.try_add_block(block(0, 0, 2, 1, false, false));
    REQUIRE(with_block);

    const std::optional<puzzle> with_wall = with_block->try_toggle_wall(0, 0);
    REQUIRE(with_wall);
    CHECK(with_wall->block_count() == 0);
    CHECK(with_wall->wall_count() == 2);
    CHECK(with_wall->wall_at(0, 0));
    CHECK(with_wall->wall_at(1, 0));
    const std::vector<block> shapes = with_wall->wall_rectangles();
    REQUIRE(shapes.size() == 1);
    CHECK(shapes[0].get_width() == 2);
    CHECK(shapes[0].get_height() == 1);

    const std::optional<puzzle> removed = with_wall->try_remove_wall(1, 0);
    REQUIRE(removed);
    CHECK(removed->wall_count() == 0);
    CHECK(removed->block_count() == 0);

    const std::optional<puzzle> back = with_wall->try_toggle_wall(0, 0);
    REQUIRE(back);
    CHECK(back->wall_count() == 0);
    CHECK(back->block_count() == 1);
    const std::optional<block> restored = back->try_get_block(0, 0);
    REQUIRE(restored);
    CHECK(restored->get_width() == 2);
    CHECK(restored->get_height() == 1);
    CHECK_FALSE(restored->get_target());
}

TEST_CASE("placement-space handles free movement and square targets", "[placement_space]")
{
    const puzzle_space_config config = make_config(klotski_board(), 3, 0);
    const placement_space space(config);

    size_t total = 0;
    for (size_t index = 0; index < space.partition_count(); ++index) {
        bool cancelled = false;
        const auto [count, has_goal] = space.count(index, {}, cancelled);
        REQUIRE_FALSE(cancelled);
        CHECK(count > 0);
        CHECK(has_goal);
        total += count;
    }
    CHECK(total > 0);

    // Free movement: the target must appear on several rows, not just the goal row.
    std::set<int> target_rows;
    for (size_t index = 0; index < space.partition_count(); ++index) {
        const placement_partition& partition = space.partition(index);
        bool count_cancelled = false;
        const auto [expected, expected_goal] = space.count(index, {}, count_cancelled);
        REQUIRE_FALSE(count_cancelled);
        REQUIRE(expected_goal);
        REQUIRE(expected > 0);

        placement_table table(expected, space.key_bytes(partition.blocks));
        struct context_t
        {
            placement_table* table = nullptr;
            std::set<int>* rows = nullptr;
        } context{&table, &target_rows};

        const auto emit = [](void* raw, const u8* key, const bool /*goal*/)
        {
            auto* ctx = static_cast<context_t*>(raw);
            ctx->table->insert(key);
            const int cell = static_cast<int>(key[0] & 0x3f);
            ctx->rows->insert(cell / 4); // the Klotski board is 4 cells wide
        };

        bool cancelled = false;
        space.enumerate(index, {}, emit, &context, cancelled);
        REQUIRE_FALSE(cancelled);
    }
    CHECK(target_rows.size() > 1);
}

namespace
{
struct index_progress_probe
{
    size_t calls = 0;
    size_t last_done = 0;
    size_t last_total = 0;
    std::stop_source* stop = nullptr;
    size_t stop_at_call = 0;
};

auto record_index_progress(void* raw, const size_t done, const size_t total) -> void
{
    auto* probe = static_cast<index_progress_probe*>(raw);
    ++probe->calls;
    probe->last_done = done;
    probe->last_total = total;
    if (probe->stop != nullptr && probe->stop_at_call != 0 && probe->calls >= probe->stop_at_call) {
        probe->stop->request_stop();
    }
}

auto fill_index_table(placement_table& table, const size_t count) -> void
{
    for (u64 i = 0; i < count; ++i) {
        u8 key[16] = {};
        std::memcpy(key, &i, sizeof(i));
        table.insert(key);
    }
}
} // namespace

TEST_CASE("placement-table index construction reports progress", "[placement_space]")
{
    placement_table table(0, 8);
    fill_index_table(table, 100'000);

    index_progress_probe probe;
    CHECK(table.build_index(nullptr, {}, record_index_progress, &probe));
    REQUIRE(table.index_built());
    CHECK(probe.calls > 0);
    CHECK(probe.last_total > 0);
    CHECK(probe.last_done <= probe.last_total);
}

TEST_CASE("placement-table index construction stops when cancelled", "[placement_space]")
{
    placement_table table(0, 8);
    fill_index_table(table, 200'000);

    SECTION("already cancelled")
    {
        std::stop_source stop;
        stop.request_stop();

        index_progress_probe probe;
        CHECK_FALSE(table.build_index(nullptr, stop.get_token(), record_index_progress, &probe));
        CHECK_FALSE(table.index_built());
    }

    SECTION("cancelled while building")
    {
        std::stop_source stop;
        index_progress_probe probe;
        probe.stop = &stop;
        probe.stop_at_call = 2;

        CHECK_FALSE(table.build_index(nullptr, stop.get_token(), record_index_progress, &probe));
        CHECK_FALSE(table.index_built());
        CHECK(probe.calls >= 2);
    }
}

TEST_CASE("placement-space move set of the reference configurations is unchanged",
          "[placement_space]")
{
    struct case_t
    {
        const char* name;
        puzzle_space_config config;
        size_t states;
        size_t moves;
        u64 hash;
    };

    // Golden values recorded from the implementation before the neighbour
    // generator was optimised (identical move multiset required). states/moves
    // cover every placement of every partition, including unwinnable ones.
    const std::array<case_t, 4> cases{{
        {"rush hour 3 blocks", make_config(rush_hour_board(), 3, 0), 18'823, 76'554,
         0x96002ff9650affe8ull},
        {"klotski 4 blocks", make_config(klotski_board(), 4, 0), 72'256, 532'512,
         0x480cbddc56e027f9ull},
        {"walled 5x5 3 blocks", make_config(walled_board(), 3, 0), 5'819, 43'008,
         0xfcf22b8dcb48abebull},
        {"wide keys 3x4 11 blocks", make_config(wide_key_board(), 11, 0), 24'552, 208'862,
         0xfaaaa33aeada0587ull},
    }};

    for (const case_t& test : cases) {
        INFO(test.name);
        const move_checksum sum = checksum_moves(test.config);
        CHECK(sum.states == test.states);
        CHECK(sum.moves == test.moves);
        CHECK(sum.hash == test.hash);
    }
}
