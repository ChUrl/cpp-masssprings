#ifndef BITS_HPP_
#define BITS_HPP_

#include "util.hpp"

#include <concepts>

template <class T>
    requires std::unsigned_integral<T>
// ReSharper disable once CppRedundantInlineSpecifier
INLINE inline auto create_mask(const uint8_t first, const uint8_t last) -> T
{
    // If the mask width is equal the type width return all 1s instead of shifting
    // as shifting by type-width is undefined behavior.
    if (static_cast<size_t>(last - first + 1) >= sizeof(T) * 8) {
        return ~T{0};
    }

    // Example: first=4, last=7, 7-4+1=4
    //           1 << 4 = 0b00010000
    //          32  - 1 = 0b00001111
    //          31 << 4 = 0b11110000
    // Subtracting 1 generates a consecutive mask.
    return ((T{1} << (last - first + 1)) - 1) << first;
}

template <class T>
    requires std::unsigned_integral<T>
// ReSharper disable once CppRedundantInlineSpecifier
INLINE inline auto clear_bits(T& bits, const uint8_t first, const uint8_t last) -> void
{
    const T mask = create_mask<T>(first, last);

    bits = bits & ~mask;
}

template <class T, class U>
    requires std::unsigned_integral<T> && std::unsigned_integral<U>
// ReSharper disable once CppRedundantInlineSpecifier
INLINE inline auto set_bits(T& bits, const uint8_t first, const uint8_t last, const U value) -> void
{
    const T mask = create_mask<T>(first, last);

    // Example: first=4, last=6, value=0b1110,   bits = 0b 01111110
    //          mask                                  = 0b 01110000
    //          bits & ~mask                          = 0b 00001110
    //          value << 4                            = 0b 11100000
    //          (value << 4) & mask                   = 0b 01100000
    //          (bits & ~mask) | (value << 4) & mask  = 0b 01101110
    //                                 Insert position:     ^^^
    // First clear the bits, then | with the value positioned at the insertion point.
    // The value may be larger than [first, last], extra bits are ignored.
    bits = (bits & ~mask) | ((static_cast<T>(value) << first) & mask);
}

template <class T>
    requires std::unsigned_integral<T>
// ReSharper disable once CppRedundantInlineSpecifier
INLINE inline auto get_bits(const T bits, const uint8_t first, const uint8_t last) -> T
{
    const T mask = create_mask<T>(first, last);

    // We can >> without sign extension because T is unsigned_integral
    return (bits & mask) >> first;
}

auto print_bitmap(uint64_t bitmap, uint8_t w, uint8_t h, const std::string& title) -> void;

#endif