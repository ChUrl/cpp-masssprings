// ReSharper disable CppLocalVariableMayBeConst
// ReSharper disable CppVariableCanBeMadeConstexpr
#include "puzzle.hpp"

#include <catch2/catch_test_macros.hpp>
#include <set>
#include <algorithm>

// ============================================================================
// Block basics
// ============================================================================

TEST_CASE("Block creation and field access", "[block]")
{
    block b(1, 2, 3, 4, true, false);

    CHECK(b.get_x() == 1);
    CHECK(b.get_y() == 2);
    CHECK(b.get_width() == 3);
    CHECK(b.get_height() == 4);
    CHECK(b.get_target() == true);
    CHECK(b.get_immovable() == false);
    CHECK(b.valid());
}

TEST_CASE("Block invalid by default", "[block]")
{
    block b;
    CHECK_FALSE(b.valid());
}

TEST_CASE("Block comparison ordering", "[block]")
{
    // Row-major: (0,0) < (1,0) < (0,1)
    block a(0, 0, 1, 1);
    block b(1, 0, 1, 1);
    block c(0, 1, 1, 1);

    CHECK(a < b);
    CHECK(b < c);
    CHECK(a < c);
}

TEST_CASE("Block principal_dirs for horizontal block", "[block]")
{
    block b(0, 0, 3, 1); // wider than tall
    CHECK((b.principal_dirs() & eas));
    CHECK((b.principal_dirs() & wes));
    CHECK_FALSE((b.principal_dirs() & nor));
    CHECK_FALSE((b.principal_dirs() & sou));
}

TEST_CASE("Block principal_dirs for vertical block", "[block]")
{
    block b(0, 0, 1, 3); // taller than wide
    CHECK((b.principal_dirs() & nor));
    CHECK((b.principal_dirs() & sou));
    CHECK_FALSE((b.principal_dirs() & eas));
    CHECK_FALSE((b.principal_dirs() & wes));
}

TEST_CASE("Block principal_dirs for square block", "[block]")
{
    block b(0, 0, 2, 2);
    CHECK((b.principal_dirs() & nor));
    CHECK((b.principal_dirs() & sou));
    CHECK((b.principal_dirs() & eas));
    CHECK((b.principal_dirs() & wes));
}

TEST_CASE("Block covers", "[block]")
{
    block b(1, 2, 3, 2);
    // Covers (1,2) to (3,3)
    CHECK(b.covers(1, 2));
    CHECK(b.covers(3, 3));
    CHECK(b.covers(2, 2));
    CHECK_FALSE(b.covers(0, 2));
    CHECK_FALSE(b.covers(4, 2));
    CHECK_FALSE(b.covers(1, 1));
    CHECK_FALSE(b.covers(1, 4));
}

TEST_CASE("Block collides", "[block]")
{
    block a(0, 0, 2, 2);
    block b(1, 1, 2, 2);
    block c(2, 2, 1, 1);
    block d(3, 0, 1, 1);

    CHECK(a.collides(b));
    CHECK(b.collides(a));
    CHECK_FALSE(a.collides(c));
    CHECK_FALSE(a.collides(d));
}

// ============================================================================
// Puzzle basics
// ============================================================================

TEST_CASE("Puzzle creation and meta access", "[puzzle]")
{
    puzzle p(6, 6, 0, 0, true, true);

    CHECK(p.get_width() == 6);
    CHECK(p.get_height() == 6);
    CHECK(p.get_goal_x() == 0);
    CHECK(p.get_goal_y() == 0);
    CHECK(p.get_restricted() == true);
    CHECK(p.get_goal() == true);
    CHECK(p.valid());
}

TEST_CASE("Puzzle add and remove block", "[puzzle]")
{
    puzzle p(4, 4, 0, 0, false, false);
    block b(0, 0, 2, 1);

    auto p2 = p.try_add_block(b);
    REQUIRE(p2.has_value());
    CHECK(p2->block_count() == 1);

    auto p3 = p2->try_remove_block(0, 0);
    REQUIRE(p3.has_value());
    CHECK(p3->block_count() == 0);
}

TEST_CASE("Puzzle meta roundtrip for all valid sizes", "[puzzle]")
{
    for (u8 w = 3; w <= 8; ++w) {
        for (u8 h = 3; h <= 8; ++h) {
            puzzle p(w, h, 0, 0, true, false);
            CHECK(p.get_width() == w);
            CHECK(p.get_height() == h);
        }
    }
}

TEST_CASE("Puzzle string repr roundtrip", "[puzzle]")
{
    const std::string s = "S:[4x4] M:[R] B:[{1x1 _ _ _} {_ _ _ _} {_ _ _ _} {_ _ _ _}]";
    puzzle p(s);
    CHECK(p.valid());
    CHECK(p.get_width() == 4);
    CHECK(p.get_height() == 4);
    CHECK(p.block_count() == 1);
}

TEST_CASE("Puzzle block_count", "[puzzle]")
{
    puzzle p(4, 4, 0, 0, false, false);
    CHECK(p.block_count() == 0);

    auto p2 = p.try_add_block(block(0, 0, 1, 1));
    REQUIRE(p2);
    CHECK(p2->block_count() == 1);

    auto p3 = p2->try_add_block(block(2, 2, 1, 1));
    REQUIRE(p3);
    CHECK(p3->block_count() == 2);
}

TEST_CASE("Puzzle cannot add overlapping block", "[puzzle]")
{
    puzzle p(4, 4, 0, 0, false, false);
    auto p2 = p.try_add_block(block(0, 0, 2, 2));
    REQUIRE(p2);

    // Overlapping
    auto p3 = p2->try_add_block(block(1, 1, 2, 2));
    CHECK_FALSE(p3.has_value());
}

TEST_CASE("Puzzle cannot add block outside board", "[puzzle]")
{
    puzzle p(4, 4, 0, 0, false, false);
    auto p2 = p.try_add_block(block(3, 3, 2, 2));
    CHECK_FALSE(p2.has_value());
}

// ============================================================================
// Bitmap basics
// ============================================================================

TEST_CASE("bitmap_set_bit and bitmap_get_bit roundtrip", "[bitmap]")
{
    for (u8 w = 3; w <= 8; ++w) {
        u64 bm = 0;
        // Set every cell on a w x w board
        for (u8 y = 0; y < w; ++y) {
            for (u8 x = 0; x < w; ++x) {
                puzzle::bitmap_set_bit(bm, w, x, y);
            }
        }
        // Verify all set
        for (u8 y = 0; y < w; ++y) {
            for (u8 x = 0; x < w; ++x) {
                CHECK(puzzle::bitmap_get_bit(bm, w, x, y));
            }
        }
    }
}

TEST_CASE("bitmap_set_bit sets only the target bit", "[bitmap]")
{
    for (u8 w = 3; w <= 8; ++w) {
        for (u8 y = 0; y < w; ++y) {
            for (u8 x = 0; x < w; ++x) {
                u64 bm = 0;
                puzzle::bitmap_set_bit(bm, w, x, y);
                CHECK(bm == (u64(1) << (y * w + x)));
            }
        }
    }
}

TEST_CASE("bitmap_clear_bit clears only the target bit", "[bitmap]")
{
    u8 w = 6;
    u64 bm = 0;
    puzzle::bitmap_set_bit(bm, w, 2, 3);
    puzzle::bitmap_set_bit(bm, w, 4, 1);
    u64 before = bm;

    puzzle::bitmap_clear_bit(bm, w, 2, 3);
    CHECK_FALSE(puzzle::bitmap_get_bit(bm, w, 2, 3));
    CHECK(puzzle::bitmap_get_bit(bm, w, 4, 1)); // other bit untouched
}

TEST_CASE("blocks_bitmap for single block", "[bitmap]")
{
    puzzle p(6, 6, 0, 0, false, false);
    auto p2 = p.try_add_block(block(1, 2, 3, 1));
    REQUIRE(p2);

    u64 bm = p2->blocks_bitmap();
    // Block at (1,2) width 3 height 1 on a 6-wide board
    // Bits: row 2, cols 1,2,3 -> positions 2*6+1=13, 14, 15
    CHECK(puzzle::bitmap_get_bit(bm, 6, 1, 2));
    CHECK(puzzle::bitmap_get_bit(bm, 6, 2, 2));
    CHECK(puzzle::bitmap_get_bit(bm, 6, 3, 2));
    CHECK_FALSE(puzzle::bitmap_get_bit(bm, 6, 0, 2));
    CHECK_FALSE(puzzle::bitmap_get_bit(bm, 6, 4, 2));
    CHECK_FALSE(puzzle::bitmap_get_bit(bm, 6, 1, 1));
    CHECK_FALSE(puzzle::bitmap_get_bit(bm, 6, 1, 3));
}

TEST_CASE("blocks_bitmap for 2x2 block", "[bitmap]")
{
    puzzle p(5, 5, 0, 0, false, false);
    auto p2 = p.try_add_block(block(1, 1, 2, 2));
    REQUIRE(p2);

    u64 bm = p2->blocks_bitmap();
    CHECK(puzzle::bitmap_get_bit(bm, 5, 1, 1));
    CHECK(puzzle::bitmap_get_bit(bm, 5, 2, 1));
    CHECK(puzzle::bitmap_get_bit(bm, 5, 1, 2));
    CHECK(puzzle::bitmap_get_bit(bm, 5, 2, 2));
    CHECK_FALSE(puzzle::bitmap_get_bit(bm, 5, 0, 0));
    CHECK_FALSE(puzzle::bitmap_get_bit(bm, 5, 3, 1));
}

TEST_CASE("blocks_bitmap_h only includes horizontal/square blocks", "[bitmap]")
{
    puzzle p(6, 6, 0, 0, true, false);
    // Horizontal block (2x1) -> principal_dirs has eas
    auto p2 = p.try_add_block(block(0, 0, 2, 1));
    REQUIRE(p2);
    // Vertical block (1x2) -> principal_dirs has sou, not eas
    auto p3 = p2->try_add_block(block(4, 0, 1, 2));
    REQUIRE(p3);

    u64 bm_h = p3->blocks_bitmap_h();
    // Horizontal block should be in bitmap_h
    CHECK(puzzle::bitmap_get_bit(bm_h, 6, 0, 0));
    CHECK(puzzle::bitmap_get_bit(bm_h, 6, 1, 0));
    // Vertical block should NOT be in bitmap_h
    CHECK_FALSE(puzzle::bitmap_get_bit(bm_h, 6, 4, 0));
    CHECK_FALSE(puzzle::bitmap_get_bit(bm_h, 6, 4, 1));
}

TEST_CASE("blocks_bitmap_v only includes vertical/square blocks", "[bitmap]")
{
    puzzle p(6, 6, 0, 0, true, false);
    // Vertical block (1x2)
    auto p2 = p.try_add_block(block(0, 0, 1, 2));
    REQUIRE(p2);
    // Horizontal block (2x1)
    auto p3 = p2->try_add_block(block(4, 0, 2, 1));
    REQUIRE(p3);

    u64 bm_v = p3->blocks_bitmap_v();
    // Vertical block should be in bitmap_v
    CHECK(puzzle::bitmap_get_bit(bm_v, 6, 0, 0));
    CHECK(puzzle::bitmap_get_bit(bm_v, 6, 0, 1));
    // Horizontal block should NOT be in bitmap_v
    CHECK_FALSE(puzzle::bitmap_get_bit(bm_v, 6, 4, 0));
    CHECK_FALSE(puzzle::bitmap_get_bit(bm_v, 6, 5, 0));
}

TEST_CASE("blocks_bitmap_h and blocks_bitmap_v both include square blocks", "[bitmap]")
{
    puzzle p(6, 6, 0, 0, true, false);
    auto p2 = p.try_add_block(block(1, 1, 2, 2));
    REQUIRE(p2);

    u64 bm_h = p2->blocks_bitmap_h();
    u64 bm_v = p2->blocks_bitmap_v();

    // Square block should appear in both
    CHECK(puzzle::bitmap_get_bit(bm_h, 6, 1, 1));
    CHECK(puzzle::bitmap_get_bit(bm_v, 6, 1, 1));
}

// ============================================================================
// bitmap_newly_occupied_after_move
// ============================================================================

TEST_CASE("bitmap_newly_occupied north - single vertical block with space above", "[bitmap_move]")
{
    // 4x4 board, vertical 1x2 block at (1,2)
    puzzle p(4, 4, 0, 0, true, false);
    auto p2 = p.try_add_block(block(1, 2, 1, 2));
    REQUIRE(p2);

    u64 combined = p2->blocks_bitmap();
    u64 vert = p2->blocks_bitmap_v();
    u64 bm = combined;

    p2->bitmap_newly_occupied_after_move(bm, vert, nor);

    // Moving north: the block at (1,2)-(1,3) shifts to (1,1)-(1,2)
    // Newly occupied cell is (1,1)
    CHECK(puzzle::bitmap_get_bit(bm, 4, 1, 1));
    // (1,2) was already occupied, so not "newly" occupied
    CHECK_FALSE(puzzle::bitmap_get_bit(bm, 4, 1, 2));
    // (1,3) is vacated, not newly occupied
    CHECK_FALSE(puzzle::bitmap_get_bit(bm, 4, 1, 3));

    // Count total set bits - should be exactly 1
    int count = __builtin_popcountll(bm);
    CHECK(count == 1);
}

TEST_CASE("bitmap_newly_occupied south - single vertical block with space below", "[bitmap_move]")
{
    // 4x4 board, vertical 1x2 block at (1,0)
    puzzle p(4, 4, 0, 0, true, false);
    auto p2 = p.try_add_block(block(1, 0, 1, 2));
    REQUIRE(p2);

    u64 combined = p2->blocks_bitmap();
    u64 vert = p2->blocks_bitmap_v();
    u64 bm = combined;

    p2->bitmap_newly_occupied_after_move(bm, vert, sou);

    // Moving south: block at (1,0)-(1,1) shifts to (1,1)-(1,2)
    // Newly occupied cell is (1,2)
    CHECK(puzzle::bitmap_get_bit(bm, 4, 1, 2));
    int count = __builtin_popcountll(bm);
    CHECK(count == 1);
}

TEST_CASE("bitmap_newly_occupied east - single horizontal block with space right", "[bitmap_move]")
{
    // 5x4 board, horizontal 2x1 block at (0,1)
    puzzle p(5, 4, 0, 0, true, false);
    auto p2 = p.try_add_block(block(0, 1, 2, 1));
    REQUIRE(p2);

    u64 combined = p2->blocks_bitmap();
    u64 horiz = p2->blocks_bitmap_h();
    u64 bm = combined;

    p2->bitmap_newly_occupied_after_move(bm, horiz, eas);

    // Moving east: block at (0,1)-(1,1) shifts to (1,1)-(2,1)
    // Newly occupied cell is (2,1)
    CHECK(puzzle::bitmap_get_bit(bm, 5, 2, 1));
    int count = __builtin_popcountll(bm);
    CHECK(count == 1);
}

TEST_CASE("bitmap_newly_occupied west - single horizontal block with space left", "[bitmap_move]")
{
    // 5x4 board, horizontal 2x1 block at (2,1)
    puzzle p(5, 4, 0, 0, true, false);
    auto p2 = p.try_add_block(block(2, 1, 2, 1));
    REQUIRE(p2);

    u64 combined = p2->blocks_bitmap();
    u64 horiz = p2->blocks_bitmap_h();
    u64 bm = combined;

    p2->bitmap_newly_occupied_after_move(bm, horiz, wes);

    // Moving west: block at (2,1)-(3,1) shifts to (1,1)-(2,1)
    // Newly occupied cell is (1,1)
    CHECK(puzzle::bitmap_get_bit(bm, 5, 1, 1));
    int count = __builtin_popcountll(bm);
    CHECK(count == 1);
}

TEST_CASE("bitmap_newly_occupied does not wrap east across rows", "[bitmap_move]")
{
    // 4x4 board, horizontal 2x1 block at (2,0) - rightmost position
    puzzle p(4, 4, 0, 0, true, false);
    auto p2 = p.try_add_block(block(2, 0, 2, 1));
    REQUIRE(p2);

    u64 combined = p2->blocks_bitmap();
    u64 horiz = p2->blocks_bitmap_h();
    u64 bm = combined;

    p2->bitmap_newly_occupied_after_move(bm, horiz, eas);

    // Block is at right edge, east move should not wrap to next row
    // No newly occupied cells should exist (or at least not on row 1)
    CHECK_FALSE(puzzle::bitmap_get_bit(bm, 4, 0, 1));
}

TEST_CASE("bitmap_newly_occupied does not wrap west across rows", "[bitmap_move]")
{
    // 4x4 board, horizontal 2x1 block at (0,1)
    puzzle p(4, 4, 0, 0, true, false);
    auto p2 = p.try_add_block(block(0, 1, 2, 1));
    REQUIRE(p2);

    u64 combined = p2->blocks_bitmap();
    u64 horiz = p2->blocks_bitmap_h();
    u64 bm = combined;

    p2->bitmap_newly_occupied_after_move(bm, horiz, wes);

    // Block is at left edge, west move should not wrap to previous row
    CHECK_FALSE(puzzle::bitmap_get_bit(bm, 4, 3, 0));
}

TEST_CASE("bitmap_newly_occupied blocked by another block", "[bitmap_move]")
{
    // 6x4 board, two horizontal blocks side by side
    puzzle p(6, 4, 0, 0, true, false);
    auto p2 = p.try_add_block(block(0, 0, 2, 1));
    REQUIRE(p2);
    auto p3 = p2->try_add_block(block(2, 0, 2, 1));
    REQUIRE(p3);

    u64 combined = p3->blocks_bitmap();
    u64 horiz = p3->blocks_bitmap_h();
    u64 bm = combined;

    p3->bitmap_newly_occupied_after_move(bm, horiz, eas);

    // Block at (0,0)-(1,0) tries to move east to (2,0) but that's occupied
    // -> no newly occupied cell at (2,0) since it's already in combined bitmap
    // Block at (2,0)-(3,0) tries to move east to (4,0) which is free
    CHECK(puzzle::bitmap_get_bit(bm, 6, 4, 0));
    // (2,0) should NOT be newly occupied (already occupied by second block)
    CHECK_FALSE(puzzle::bitmap_get_bit(bm, 6, 2, 0));
}

// ============================================================================
// restricted_bitmap_get_moves
// ============================================================================

TEST_CASE("restricted_bitmap_get_moves north - returns correct source cell", "[get_moves]")
{
    // 4x4 board, vertical 1x2 block at (1,2)
    puzzle p(4, 4, 0, 0, true, false);
    auto p2 = p.try_add_block(block(1, 2, 1, 2));
    REQUIRE(p2);

    u64 combined = p2->blocks_bitmap();
    u64 vert = p2->blocks_bitmap_v();

    auto moves = p2->restricted_bitmap_get_moves(combined, vert, nor);

    // Should find one move. The newly occupied cell is (1,1).
    // The source cell (block's top-left lookup) should be (1,2) = bit index 1 + 2*4 = 9
    REQUIRE(moves[0] != 0xFF);
    CHECK(moves[0] == 1 + 2 * 4); // = 9
    CHECK(moves[1] == 0xFF);      // only one move
}

TEST_CASE("restricted_bitmap_get_moves south - returns correct source cell", "[get_moves]")
{
    // 4x4 board, vertical 1x2 block at (1,0)
    puzzle p(4, 4, 0, 0, true, false);
    auto p2 = p.try_add_block(block(1, 0, 1, 2));
    REQUIRE(p2);

    u64 combined = p2->blocks_bitmap();
    u64 vert = p2->blocks_bitmap_v();

    auto moves = p2->restricted_bitmap_get_moves(combined, vert, sou);

    // Newly occupied cell is (1,2). Source cell should be (1,0) = bit 1 + 0*4 = 1?
    // Actually: for south, the block was one row above the newly occupied cell.
    // Newly occupied = (1,2) = bit 9. Source = bit 9 - width = 9 - 4 = 5 = (1,1).
    // Wait - the source should be a cell the block currently occupies.
    // The block occupies (1,0) and (1,1). The newly occupied cell after south move is (1,2).
    // To find the block, we go opposite direction (north) from newly occupied: (1,2-1) = (1,1) = bit 5.
    // bitmap_block_indices[5] should map to the block.
    REQUIRE(moves[0] != 0xFF);
    CHECK(moves[0] == 1 + 1 * 4); // = 5, which is (1,1)
    CHECK(moves[1] == 0xFF);
}

TEST_CASE("restricted_bitmap_get_moves east - returns correct source cell", "[get_moves]")
{
    // 5x4 board, horizontal 2x1 block at (0,1)
    puzzle p(5, 4, 0, 0, true, false);
    auto p2 = p.try_add_block(block(0, 1, 2, 1));
    REQUIRE(p2);

    u64 combined = p2->blocks_bitmap();
    u64 horiz = p2->blocks_bitmap_h();

    auto moves = p2->restricted_bitmap_get_moves(combined, horiz, eas);

    // Newly occupied cell is (2,1) = bit 2 + 1*5 = 7.
    // Source = go opposite (west): (1,1) = bit 1 + 1*5 = 6.
    REQUIRE(moves[0] != 0xFF);
    CHECK(moves[0] == 1 + 1 * 5); // = 6
    CHECK(moves[1] == 0xFF);
}

TEST_CASE("restricted_bitmap_get_moves west - returns correct source cell", "[get_moves]")
{
    // 5x4 board, horizontal 2x1 block at (2,1)
    puzzle p(5, 4, 0, 0, true, false);
    auto p2 = p.try_add_block(block(2, 1, 2, 1));
    REQUIRE(p2);

    u64 combined = p2->blocks_bitmap();
    u64 horiz = p2->blocks_bitmap_h();

    auto moves = p2->restricted_bitmap_get_moves(combined, horiz, wes);

    // Newly occupied cell is (1,1) = bit 1 + 1*5 = 6.
    // Source = go opposite (east): (2,1) = bit 2 + 1*5 = 7.
    REQUIRE(moves[0] != 0xFF);
    CHECK(moves[0] == 2 + 1 * 5); // = 7
    CHECK(moves[1] == 0xFF);
}

TEST_CASE("restricted_bitmap_get_moves returns empty when blocked", "[get_moves]")
{
    // 4x4 board, vertical 1x2 block at (1,0) - can't move north (at top edge)
    puzzle p(4, 4, 0, 0, true, false);
    auto p2 = p.try_add_block(block(1, 0, 1, 2));
    REQUIRE(p2);

    u64 combined = p2->blocks_bitmap();
    u64 vert = p2->blocks_bitmap_v();

    auto moves = p2->restricted_bitmap_get_moves(combined, vert, nor);

    // Block is at top edge. Shifting north puts bits outside the board (shifted out).
    // The newly occupied bitmap should be empty.
    // But wait - the shift just moves bits to lower positions. Bit at (1,0) = bit 1
    // shifted right by 4 = 0 (shifted out). Bit at (1,1) = bit 5 shifted right by 4 = bit 1.
    // bit 1 is (1,0) which is already occupied -> newly occupied = 0.
    CHECK(moves[0] == 0xFF);
}

TEST_CASE("restricted_bitmap_get_moves with width 6 board", "[get_moves]")
{
    // 6x6 board, vertical 1x2 block at (2,3)
    puzzle p(6, 6, 0, 0, true, false);
    auto p2 = p.try_add_block(block(2, 3, 1, 2));
    REQUIRE(p2);

    u64 combined = p2->blocks_bitmap();
    u64 vert = p2->blocks_bitmap_v();

    auto moves = p2->restricted_bitmap_get_moves(combined, vert, nor);

    // Block at (2,3)-(2,4). Moving north: newly occupied = (2,2) = bit 2+2*6 = 14.
    // Source = 14 + 6 = 20 = (2,3).
    REQUIRE(moves[0] != 0xFF);
    CHECK(moves[0] == 2 + 3 * 6); // = 20
    CHECK(moves[1] == 0xFF);
}

TEST_CASE("restricted_bitmap_get_moves multiple blocks can move", "[get_moves]")
{
    // 6x6 board, two vertical blocks with space above
    puzzle p(6, 6, 0, 0, true, false);
    auto p2 = p.try_add_block(block(1, 2, 1, 2));
    REQUIRE(p2);
    auto p3 = p2->try_add_block(block(4, 3, 1, 2));
    REQUIRE(p3);

    u64 combined = p3->blocks_bitmap();
    u64 vert = p3->blocks_bitmap_v();

    auto moves = p3->restricted_bitmap_get_moves(combined, vert, nor);

    // Both blocks can move north
    // Block 1: newly occupied (1,1), source (1,2) = 1+2*6 = 13
    // Block 2: newly occupied (4,2), source (4,3) = 4+3*6 = 22
    std::set<u8> move_set;
    for (int i = 0; i < puzzle::MAX_BLOCKS && moves[i] != 0xFF; ++i) {
        move_set.insert(moves[i]);
    }
    CHECK(move_set.size() == 2);
    CHECK(move_set.count(13) == 1);
    CHECK(move_set.count(22) == 1);
}

// ============================================================================
// try_move_block_at (reference implementation) vs try_move_block_at_fast
// ============================================================================

TEST_CASE("try_move_block_at basic moves", "[move]")
{
    puzzle p(4, 4, 0, 0, false, false);
    auto p2 = p.try_add_block(block(1, 1, 1, 1));
    REQUIRE(p2);

    auto north = p2->try_move_block_at(1, 1, nor);
    REQUIRE(north);
    CHECK(north->try_get_block(1, 0).has_value());

    auto south = p2->try_move_block_at(1, 1, sou);
    REQUIRE(south);
    CHECK(south->try_get_block(1, 2).has_value());

    auto east = p2->try_move_block_at(1, 1, eas);
    REQUIRE(east);
    CHECK(east->try_get_block(2, 1).has_value());

    auto west = p2->try_move_block_at(1, 1, wes);
    REQUIRE(west);
    CHECK(west->try_get_block(0, 1).has_value());
}

TEST_CASE("try_move_block_at blocked by edge", "[move]")
{
    puzzle p(4, 4, 0, 0, false, false);
    auto p2 = p.try_add_block(block(0, 0, 1, 1));
    REQUIRE(p2);

    CHECK_FALSE(p2->try_move_block_at(0, 0, nor).has_value());
    CHECK_FALSE(p2->try_move_block_at(0, 0, wes).has_value());
}

TEST_CASE("try_move_block_at blocked by collision", "[move]")
{
    puzzle p(4, 4, 0, 0, false, false);
    auto p2 = p.try_add_block(block(0, 0, 1, 1));
    REQUIRE(p2);
    auto p3 = p2->try_add_block(block(1, 0, 1, 1));
    REQUIRE(p3);

    CHECK_FALSE(p3->try_move_block_at(0, 0, eas).has_value());
}

TEST_CASE("try_move_block_at_fast matches try_move_block_at", "[move]")
{
    puzzle p(5, 5, 0, 0, false, false);
    auto p2 = p.try_add_block(block(1, 1, 2, 1));
    REQUIRE(p2);
    auto p3 = p2->try_add_block(block(0, 3, 1, 2));
    REQUIRE(p3);

    u64 bm = p3->blocks_bitmap();

    for (const dir d : {nor, eas, sou, wes}) {
        // Block 0 is the first in sorted order
        auto slow = p3->try_move_block_at(1, 1, d);
        auto fast = p3->try_move_block_at_fast(bm, 0, d);

        if (slow.has_value()) {
            REQUIRE(fast.has_value());
            CHECK(*slow == *fast);
        } else {
            CHECK_FALSE(fast.has_value());
        }
    }
}

TEST_CASE("try_move_block_at restricted mode respects principal dirs", "[move]")
{
    puzzle p(5, 5, 0, 0, true, false); // restricted
    // Horizontal block 2x1 at (1,2)
    auto p2 = p.try_add_block(block(1, 2, 2, 1));
    REQUIRE(p2);

    // In restricted mode, horizontal block can only move east/west
    CHECK(p2->try_move_block_at(1, 2, eas).has_value());
    CHECK(p2->try_move_block_at(1, 2, wes).has_value());
    CHECK_FALSE(p2->try_move_block_at(1, 2, nor).has_value());
    CHECK_FALSE(p2->try_move_block_at(1, 2, sou).has_value());
}

// ============================================================================
// sorted_replace
// ============================================================================

TEST_CASE("sorted_replace maintains sort order", "[sorted_replace]")
{
    auto blocks = puzzle::sorted_replace(
        {100, 200, 300, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000},
        1,  // replace index 1 (value 200)
        150 // new value
    );

    CHECK(blocks[0] == 100);
    CHECK(blocks[1] == 150);
    CHECK(blocks[2] == 300);
    CHECK(blocks[3] == 0x8000);
}

TEST_CASE("sorted_replace move to end", "[sorted_replace]")
{
    auto blocks = puzzle::sorted_replace(
        {100, 200, 300, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000},
        0,  // replace index 0 (value 100)
        400 // new value, goes to end
    );

    CHECK(blocks[0] == 200);
    CHECK(blocks[1] == 300);
    CHECK(blocks[2] == 400);
    CHECK(blocks[3] == 0x8000);
}

TEST_CASE("sorted_replace move to beginning", "[sorted_replace]")
{
    auto blocks = puzzle::sorted_replace(
        {100, 200, 300, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000},
        2, // replace index 2 (value 300)
        50 // new value, goes to beginning
    );

    CHECK(blocks[0] == 50);
    CHECK(blocks[1] == 100);
    CHECK(blocks[2] == 200);
    CHECK(blocks[3] == 0x8000);
}

// ============================================================================
// for_each_adjacent vs for_each_adjacent_restricted cross-validation
// ============================================================================

// Helper: collect all adjacent states using for_each_adjacent (the simple reference impl)
static auto collect_adjacent_simple(const puzzle& p) -> std::set<size_t>
{
    std::set<size_t> result;
    p.for_each_adjacent([&](const puzzle& adj)
    {
        result.insert(adj.hash());
    });
    return result;
}

// Helper: collect all adjacent states using for_each_adjacent_restricted
static auto collect_adjacent_restricted(const puzzle& p) -> std::set<size_t>
{
    const u8 w = p.get_width();
    std::array<u8, 64> bitmap_block_indices{};
    for (size_t i = 0; i < puzzle::MAX_BLOCKS; ++i) {
        const block b(p.repr_view().data()[i]);
        if (i >= static_cast<size_t>(p.block_count())) {
            break;
        }
        const auto [bx, by, bw, bh, bt, bi] = b.unpack_repr();
        for (u8 x = bx; x < bx + bw; ++x) {
            for (u8 y = by; y < by + bh; ++y) {
                bitmap_block_indices[y * w + x] = i;
            }
        }
    }

    std::set<size_t> result;
    p.for_each_adjacent_restricted([&](const puzzle& adj)
    {
        result.insert(adj.hash());
    }, bitmap_block_indices);
    return result;
}

TEST_CASE("for_each_adjacent_restricted matches for_each_adjacent - single block", "[cross_validate]")
{
    puzzle p(4, 4, 0, 0, true, false);
    auto p2 = p.try_add_block(block(1, 1, 1, 1));
    REQUIRE(p2);

    auto simple = collect_adjacent_simple(*p2);
    auto restricted = collect_adjacent_restricted(*p2);

    CHECK(simple == restricted);
}

TEST_CASE("for_each_adjacent_restricted matches for_each_adjacent - horizontal block", "[cross_validate]")
{
    puzzle p(5, 4, 0, 0, true, false);
    auto p2 = p.try_add_block(block(1, 1, 2, 1));
    REQUIRE(p2);

    auto simple = collect_adjacent_simple(*p2);
    auto restricted = collect_adjacent_restricted(*p2);

    CHECK(simple == restricted);
}

TEST_CASE("for_each_adjacent_restricted matches for_each_adjacent - vertical block", "[cross_validate]")
{
    puzzle p(4, 5, 0, 0, true, false);
    auto p2 = p.try_add_block(block(1, 1, 1, 2));
    REQUIRE(p2);

    auto simple = collect_adjacent_simple(*p2);
    auto restricted = collect_adjacent_restricted(*p2);

    CHECK(simple == restricted);
}

TEST_CASE("for_each_adjacent_restricted matches for_each_adjacent - two blocks", "[cross_validate]")
{
    puzzle p(5, 5, 0, 0, true, false);
    auto p2 = p.try_add_block(block(0, 0, 2, 1)); // horizontal
    REQUIRE(p2);
    auto p3 = p2->try_add_block(block(4, 1, 1, 2)); // vertical
    REQUIRE(p3);

    auto simple = collect_adjacent_simple(*p3);
    auto restricted = collect_adjacent_restricted(*p3);

    CHECK(simple == restricted);
}

TEST_CASE("for_each_adjacent_restricted matches for_each_adjacent - crowded board", "[cross_validate]")
{
    puzzle p(5, 5, 0, 0, true, false);
    auto p2 = p.try_add_block(block(0, 0, 2, 1));
    REQUIRE(p2);
    auto p3 = p2->try_add_block(block(3, 0, 1, 2));
    REQUIRE(p3);
    auto p4 = p3->try_add_block(block(0, 2, 1, 3));
    REQUIRE(p4);
    auto p5 = p4->try_add_block(block(2, 3, 2, 1));
    REQUIRE(p5);

    auto simple = collect_adjacent_simple(*p5);
    auto restricted = collect_adjacent_restricted(*p5);

    CHECK(simple == restricted);
}

TEST_CASE("for_each_adjacent_restricted matches for_each_adjacent - 6x6 board", "[cross_validate]")
{
    puzzle p(6, 6, 0, 0, true, false);
    auto p2 = p.try_add_block(block(0, 0, 3, 1)); // horizontal
    REQUIRE(p2);
    auto p3 = p2->try_add_block(block(5, 0, 1, 2)); // vertical
    REQUIRE(p3);
    auto p4 = p3->try_add_block(block(2, 2, 1, 2)); // vertical
    REQUIRE(p4);
    auto p5 = p4->try_add_block(block(3, 2, 2, 1)); // horizontal
    REQUIRE(p5);

    auto simple = collect_adjacent_simple(*p5);
    auto restricted = collect_adjacent_restricted(*p5);

    CHECK(simple == restricted);
}

TEST_CASE("for_each_adjacent_restricted matches for_each_adjacent - the main puzzle", "[cross_validate]")
{
    const puzzle p(
        "S:[6x6] G:[0,0] M:[R] B:[{1x3 _ _ _ _ 1x2} {_ _ _ _ _ 1x2} {_ _ 1x2 2x1 _ 1x2} {_ _ 1x2 _ 2x1 _} {1x2 _ 1x2 2x1 _ _} {1x2 _ 1x2 _ 3x1 _}]");
    REQUIRE(p.valid());

    auto simple = collect_adjacent_simple(p);
    auto restricted = collect_adjacent_restricted(p);

    CHECK(simple.size() > 0);
    CHECK(simple == restricted);
}

// ============================================================================
// explore_state_space cross-validation
// ============================================================================

TEST_CASE("explore_state_space simple case - single 1x1 block", "[explore]")
{
    puzzle p(3, 3, 0, 0, false, false);
    auto p2 = p.try_add_block(block(1, 1, 1, 1));
    REQUIRE(p2);

    auto [states, transitions] = p2->explore_state_space();

    // A 1x1 block on a 3x3 board can reach all 9 positions
    CHECK(states.size() == 9);
}

TEST_CASE("explore_state_space restricted - vertical block", "[explore]")
{
    puzzle p(3, 4, 0, 0, true, false);
    auto p2 = p.try_add_block(block(1, 0, 1, 2));
    REQUIRE(p2);

    auto [states, transitions] = p2->explore_state_space();

    // Restricted vertical 1x2 on 3x4 board: can only move north/south
    // Positions: (1,0), (1,1), (1,2) -> 3 states
    CHECK(states.size() == 3);
}

TEST_CASE("explore_state_space restricted - horizontal block", "[explore]")
{
    puzzle p(4, 3, 0, 0, true, false);
    auto p2 = p.try_add_block(block(0, 1, 2, 1));
    REQUIRE(p2);

    auto [states, transitions] = p2->explore_state_space();

    // Restricted horizontal 2x1 on 4x3 board: can only move east/west
    // Positions: (0,1), (1,1), (2,1) -> 3 states
    CHECK(states.size() == 3);
}

TEST_CASE("explore_state_space restricted - square block", "[explore]")
{
    puzzle p(3, 3, 0, 0, true, false);
    auto p2 = p.try_add_block(block(0, 0, 1, 1));
    REQUIRE(p2);

    auto [states, transitions] = p2->explore_state_space();

    // Restricted 1x1 (square) can move all directions -> 9 positions on 3x3
    CHECK(states.size() == 9);
}

TEST_CASE("explore_state_space preserves board metadata", "[explore]")
{
    puzzle p(5, 4, 2, 1, true, true);
    auto p2 = p.try_add_block(block(0, 0, 1, 1, true));
    REQUIRE(p2);

    auto [states, transitions] = p2->explore_state_space();

    for (const auto& s : states) {
        CHECK(s.get_width() == 5);
        CHECK(s.get_height() == 4);
        CHECK(s.get_restricted() == true);
        CHECK(s.get_goal() == true);
        CHECK(s.get_goal_x() == 2);
        CHECK(s.get_goal_y() == 1);
    }
}

TEST_CASE("explore_state_space two blocks restricted", "[explore]")
{
    // 4x4 board, restricted
    // Horizontal 2x1 at (0,0) and vertical 1x2 at (3,0)
    puzzle p(4, 4, 0, 0, true, false);
    auto p2 = p.try_add_block(block(0, 0, 2, 1));
    REQUIRE(p2);
    auto p3 = p2->try_add_block(block(3, 0, 1, 2));
    REQUIRE(p3);

    auto [states, transitions] = p3->explore_state_space();

    // Horizontal block: can be at x=0,1,2 (y=0 fixed, can't go to x=2 if vertical is at x=3 blocking? no, 2x1 at x=2 occupies cols 2,3 which collides with vertical at x=3)
    // Actually horizontal at x=2 occupies (2,0)(3,0), vertical occupies (3,0)(3,1) -> collision at (3,0)
    // So horizontal can be at x=0 or x=1 when vertical is at (3,y)
    // Vertical block: can be at y=0,1,2 (x=3 fixed)
    // Horizontal at x=0: vertical at y=0,1,2 -> 3 states
    // Horizontal at x=1: vertical at y=0,1,2 -> 3 states  (1x1 at (1,0)(2,0), vertical at (3,y) - no collision)
    // Wait, horizontal 2x1 at x=1 occupies (1,0)(2,0). Vertical 1x2 at (3,0) occupies (3,0)(3,1). No collision.
    // Horizontal at x=2: occupies (2,0)(3,0). Vertical at (3,0) occupies (3,0)(3,1). Collision at (3,0).
    // Vertical at (3,1) occupies (3,1)(3,2). No collision with horizontal at (2,0)(3,0)... wait (3,0) is in horizontal.
    // Hmm, horizontal at x=2 occupies cols 2,3 row 0. Vertical at y=1 occupies (3,1)(3,2). No overlap. OK.
    // Vertical at y=2 occupies (3,2)(3,3). No overlap with horizontal at row 0. OK.
    // So horizontal at x=2: vertical at y=1,2 -> 2 states (not y=0 due to collision at (3,0))
    // Total: 3 + 3 + 2 = 8 states
    CHECK(states.size() == 8);
}

// ============================================================================
// Column mask generation test (implicit in bitmap_newly_occupied)
// ============================================================================

TEST_CASE("East move does not wrap for various board widths", "[bitmap_move]")
{
    for (u8 w = 3; w <= 8; ++w) {
        puzzle p(w, 3, 0, 0, true, false);
        // Place a 1x1 block at rightmost column, row 0
        auto p2 = p.try_add_block(block(w - 1, 0, 1, 1));
        REQUIRE(p2);

        u64 combined = p2->blocks_bitmap();
        u64 horiz = p2->blocks_bitmap_h(); // 1x1 is square, so in both h and v
        u64 bm = combined;

        p2->bitmap_newly_occupied_after_move(bm, horiz, eas);

        // Should not wrap to column 0 of next row
        CHECK_FALSE(puzzle::bitmap_get_bit(bm, w, 0, 1));
        // Should be empty (block at right edge can't move east)
        CHECK(bm == 0);
    }
}

TEST_CASE("West move does not wrap for various board widths", "[bitmap_move]")
{
    for (u8 w = 3; w <= 8; ++w) {
        puzzle p(w, 3, 0, 0, true, false);
        // Place a 1x1 block at leftmost column, row 1
        auto p2 = p.try_add_block(block(0, 1, 1, 1));
        REQUIRE(p2);

        u64 combined = p2->blocks_bitmap();
        u64 horiz = p2->blocks_bitmap_h();
        u64 bm = combined;

        p2->bitmap_newly_occupied_after_move(bm, horiz, wes);

        // Should not wrap to last column of previous row
        CHECK_FALSE(puzzle::bitmap_get_bit(bm, w, w - 1, 0));
        CHECK(bm == 0);
    }
}

// ============================================================================
// bitmap_check_collision
// ============================================================================

TEST_CASE("bitmap_check_collision detects collision", "[collision]")
{
    puzzle p(5, 5, 0, 0, false, false);
    auto p2 = p.try_add_block(block(2, 2, 1, 1));
    REQUIRE(p2);
    auto p3 = p2->try_add_block(block(0, 0, 2, 1));
    REQUIRE(p3);

    u64 bm = p3->blocks_bitmap();

    // Check if a hypothetical block at (1,2) 2x1 collides
    block hyp(1, 2, 2, 1);
    CHECK(p3->bitmap_check_collision(bm, hyp)); // overlaps with (2,2)

    // Check if a hypothetical block at (3,3) 1x1 collides
    block hyp2(3, 3, 1, 1);
    CHECK_FALSE(p3->bitmap_check_collision(bm, hyp2));
}

TEST_CASE("bitmap_check_collision directional", "[collision]")
{
    puzzle p(5, 5, 0, 0, false, false);
    auto p2 = p.try_add_block(block(2, 0, 1, 1));
    REQUIRE(p2);
    auto p3 = p2->try_add_block(block(2, 1, 1, 1));
    REQUIRE(p3);

    u64 bm = p3->blocks_bitmap();

    // Clear block at (2,1) from bitmap to check if (2,1) can move north
    block b(2, 1, 1, 1);
    p3->bitmap_clear_block(bm, b);

    // (2,1) moving north -> check (2,0) which has a block
    CHECK(p3->bitmap_check_collision(bm, b, nor));

    // (2,1) moving south -> check (2,2) which is empty
    CHECK_FALSE(p3->bitmap_check_collision(bm, b, sou));
}

// ============================================================================
// Regression: the main puzzle from main.cpp
// ============================================================================

TEST_CASE("Main puzzle state space has many states", "[explore][regression]")
{
    const puzzle p(
        "S:[6x6] G:[0,0] M:[R] B:[{1x3 _ _ _ _ 1x2} {_ _ _ _ _ 1x2} {_ _ 1x2 2x1 _ 1x2} {_ _ 1x2 _ 2x1 _} {1x2 _ 1x2 2x1 _ _} {1x2 _ 1x2 _ 3x1 _}]");
    REQUIRE(p.valid());
    REQUIRE(p.get_width() == 6);
    REQUIRE(p.get_height() == 6);

    // First verify the reference implementation finds many adjacents
    auto simple = collect_adjacent_simple(p);
    CHECK(simple.size() > 3); // should be more than the 3 the buggy version found

    auto restricted = collect_adjacent_restricted(p);
    CHECK(simple == restricted);
}