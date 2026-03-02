#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <cstdint>

#include "util.hpp"

// =============================================================================
// Catch2
// =============================================================================
//
// 1. TEST_CASE(name, tags)
//    The basic unit of testing in Catch2. Each TEST_CASE is an independent test
//    function. The first argument is a descriptive name (must be unique), and
//    the second is a string of tags in square brackets (e.g. "[set_bits]")
//    used to filter and group tests when running.
//
// 2. SECTION(name)
//    Sections allow multiple subtests within a single TEST_CASE. Each SECTION
//    runs the TEST_CASE from the top, so any setup code before the SECTION is
//    re-executed fresh for every section. This gives each section an isolated
//    starting state without needing separate TEST_CASEs or explicit teardown.
//    Sections can also be nested.
//
// 3. REQUIRE(expression)
//    The primary assertion macro. If the expression evaluates to false, the
//    test fails immediately and Catch2 reports the actual values of both sides
//    of the comparison (e.g. "0xF5 == 0xF0" on failure). There is also
//    CHECK(), which records a failure but continues executing the rest of the
//    test; REQUIRE() aborts the current test on failure.
//
// 4. TEMPLATE_TEST_CASE(name, tags, Type1, Type2, ...)
//    A parameterised test that is instantiated once for each type listed.
//    Inside the test body, the alias `TestType` refers to the current type.
//    This avoids duplicating identical logic for uint8_t, uint16_t, uint32_t,
//    and uint64_t. Catch2 automatically appends the type name to the test name
//    in the output so you can see which instantiation failed.
//
// 5. Tags (e.g. "[create_mask]", "[round-trip]")
//    Tags let you selectively run subsets of tests from the command line.
//    For example:
//      ./tests "[set_bits]"        -- runs only tests tagged [set_bits]
//      ./tests "~[round-trip]"     -- runs everything except [round-trip]
//      ./tests "[get_bits],[set_bits]" -- runs tests matching either tag
//
// =============================================================================

// ---------------------------------------------------------------------------
// create_mask
// ---------------------------------------------------------------------------

TEMPLATE_TEST_CASE("create_mask produces correct masks", "[create_mask]",
                   uint8_t, uint16_t, uint32_t, uint64_t)
{
    SECTION("single bit mask at bit 0") {
        auto m = create_mask<TestType>(0, 0);
        REQUIRE(m == TestType{0b1});
    }

    SECTION("single bit mask at bit 3") {
        auto m = create_mask<TestType>(3, 3);
        REQUIRE(m == TestType{0b1000});
    }

    SECTION("mask spanning bits 0..7 gives 0xFF") {
        auto m = create_mask<TestType>(0, 7);
        REQUIRE(m == TestType{0xFF});
    }

    SECTION("mask spanning bits 4..7") {
        auto m = create_mask<TestType>(4, 7);
        REQUIRE(m == TestType{0xF0});
    }

    SECTION("full-width mask returns all ones") {
        constexpr uint8_t last = sizeof(TestType) * 8 - 1;
        auto m = create_mask<TestType>(0, last);
        REQUIRE(m == static_cast<TestType>(~TestType{0}));
    }
}

TEST_CASE("create_mask 32-bit specific cases", "[create_mask]") {
    REQUIRE(create_mask<uint32_t>(0, 15)  == 0x0000FFFF);
    REQUIRE(create_mask<uint32_t>(0, 31)  == 0xFFFFFFFF);
    REQUIRE(create_mask<uint32_t>(16, 31) == 0xFFFF0000);
}

// ---------------------------------------------------------------------------
// clear_bits
// ---------------------------------------------------------------------------

TEMPLATE_TEST_CASE("clear_bits zeroes the specified range", "[clear_bits]",
                   uint8_t, uint16_t, uint32_t, uint64_t)
{
    SECTION("clear all bits") {
        TestType val = static_cast<TestType>(~TestType{0});
        constexpr uint8_t last = sizeof(TestType) * 8 - 1;
        clear_bits(val, 0, last);
        REQUIRE(val == TestType{0});
    }

    SECTION("clear lower nibble") {
        TestType val = static_cast<TestType>(0xFF);
        clear_bits(val, 0, 3);
        REQUIRE(val == static_cast<TestType>(0xF0));
    }

    SECTION("clear upper nibble") {
        TestType val = static_cast<TestType>(0xFF);
        clear_bits(val, 4, 7);
        REQUIRE(val == static_cast<TestType>(0x0F));
    }

    SECTION("clear single bit") {
        TestType val = static_cast<TestType>(0xFF);
        clear_bits(val, 0, 0);
        REQUIRE(val == static_cast<TestType>(0xFE));
    }

    SECTION("clearing already-zero bits is a no-op") {
        TestType val = TestType{0};
        clear_bits(val, 0, 3);
        REQUIRE(val == TestType{0});
    }
}

// ---------------------------------------------------------------------------
// set_bits
// ---------------------------------------------------------------------------

TEMPLATE_TEST_CASE("set_bits writes value into the specified range", "[set_bits]",
                   uint8_t, uint16_t, uint32_t, uint64_t)
{
    SECTION("set lower nibble on zero") {
        TestType val = TestType{0};
        set_bits(val, uint8_t{0}, uint8_t{3}, static_cast<TestType>(0xA));
        REQUIRE(val == static_cast<TestType>(0x0A));
    }

    SECTION("set upper nibble on zero") {
        TestType val = TestType{0};
        set_bits(val, uint8_t{4}, uint8_t{7}, static_cast<TestType>(0xB));
        REQUIRE(val == static_cast<TestType>(0xB0));
    }

    SECTION("set_bits replaces existing bits") {
        TestType val = static_cast<TestType>(0xFF);
        set_bits(val, uint8_t{0}, uint8_t{3}, static_cast<TestType>(0x5));
        REQUIRE(val == static_cast<TestType>(0xF5));
    }

    SECTION("set single bit to 1") {
        TestType val = TestType{0};
        set_bits(val, uint8_t{3}, uint8_t{3}, static_cast<TestType>(1));
        REQUIRE(val == static_cast<TestType>(0x08));
    }

    SECTION("set single bit to 0") {
        TestType val = static_cast<TestType>(0xFF);
        set_bits(val, uint8_t{3}, uint8_t{3}, static_cast<TestType>(0));
        REQUIRE(val == static_cast<TestType>(0xF7));
    }

    SECTION("setting value 0 clears the range") {
        TestType val = static_cast<TestType>(0xFF);
        set_bits(val, uint8_t{0}, uint8_t{7}, static_cast<TestType>(0));
        REQUIRE(val == TestType{0});
    }
}

TEST_CASE("set_bits with different value type (U != T)", "[set_bits]") {
    uint32_t val = 0;
    constexpr uint8_t small_val = 0x3F;
    set_bits(val, uint8_t{8}, uint8_t{13}, small_val);
    REQUIRE(val == (uint32_t{0x3F} << 8));
}

TEST_CASE("set_bits preserves surrounding bits in 32-bit", "[set_bits]") {
    uint32_t val = 0xDEADBEEF;
    set_bits(val, uint8_t{8}, uint8_t{15}, uint32_t{0x42});
    REQUIRE(val == 0xDEAD42EF);
}

// ---------------------------------------------------------------------------
// get_bits
// ---------------------------------------------------------------------------

TEMPLATE_TEST_CASE("get_bits extracts the specified range", "[get_bits]",
                   uint8_t, uint16_t, uint32_t, uint64_t)
{
    SECTION("get lower nibble") {
        TestType val = static_cast<TestType>(0xAB);
        auto result = get_bits(val, uint8_t{0}, uint8_t{3});
        REQUIRE(result == TestType{0xB});
    }

    SECTION("get upper nibble") {
        TestType val = static_cast<TestType>(0xAB);
        auto result = get_bits(val, uint8_t{4}, uint8_t{7});
        REQUIRE(result == TestType{0xA});
    }

    SECTION("get single bit that is set") {
        TestType val = static_cast<TestType>(0x08);
        auto result = get_bits(val, uint8_t{3}, uint8_t{3});
        REQUIRE(result == TestType{1});
    }

    SECTION("get single bit that is clear") {
        TestType val = static_cast<TestType>(0xF7);
        auto result = get_bits(val, uint8_t{3}, uint8_t{3});
        REQUIRE(result == TestType{0});
    }

    SECTION("get all bits") {
        TestType val = static_cast<TestType>(~TestType{0});
        constexpr uint8_t last = sizeof(TestType) * 8 - 1;
        auto result = get_bits(val, uint8_t{0}, last);
        REQUIRE(result == val);
    }

    SECTION("get from zero returns zero") {
        TestType val = TestType{0};
        auto result = get_bits(val, uint8_t{0}, uint8_t{7});
        REQUIRE(result == TestType{0});
    }
}

TEST_CASE("get_bits 32-bit specific extractions", "[get_bits]") {
    constexpr uint32_t val = 0xDEADBEEF;

    REQUIRE(get_bits(val, uint8_t{0},  uint8_t{7})  == 0xEF);
    REQUIRE(get_bits(val, uint8_t{8},  uint8_t{15}) == 0xBE);
    REQUIRE(get_bits(val, uint8_t{16}, uint8_t{23}) == 0xAD);
    REQUIRE(get_bits(val, uint8_t{24}, uint8_t{31}) == 0xDE);
}

// ---------------------------------------------------------------------------
// Round-trip: set then get
// ---------------------------------------------------------------------------

TEST_CASE("set_bits then get_bits round-trips correctly", "[round-trip]") {
    uint32_t reg = 0;

    set_bits(reg, uint8_t{4}, uint8_t{11}, uint32_t{0xAB});
    REQUIRE(get_bits(reg, uint8_t{4}, uint8_t{11}) == 0xAB);

    REQUIRE(get_bits(reg, uint8_t{0},  uint8_t{3})  == 0x0);
    REQUIRE(get_bits(reg, uint8_t{12}, uint8_t{31}) == 0x0);
}

TEST_CASE("multiple set_bits on different ranges", "[round-trip]") {
    uint32_t reg = 0;

    set_bits(reg, uint8_t{0},  uint8_t{7},  uint32_t{0x01});
    set_bits(reg, uint8_t{8},  uint8_t{15}, uint32_t{0x02});
    set_bits(reg, uint8_t{16}, uint8_t{23}, uint32_t{0x03});
    set_bits(reg, uint8_t{24}, uint8_t{31}, uint32_t{0x04});

    REQUIRE(reg == 0x04030201);
}

TEST_CASE("64-bit round-trip", "[round-trip]") {
    uint64_t reg = 0;
    set_bits(reg, uint8_t{32}, uint8_t{63}, uint64_t{0xCAFEBABE});
    REQUIRE(get_bits(reg, uint8_t{32}, uint8_t{63}) == uint64_t{0xCAFEBABE});
    REQUIRE(get_bits(reg, uint8_t{0},  uint8_t{31}) == uint64_t{0});
}