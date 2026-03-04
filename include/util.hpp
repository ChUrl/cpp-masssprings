#ifndef UTIL_HPP_
#define UTIL_HPP_

#include <iostream>
#include <raylib.h>

#define INLINE __attribute__((always_inline))
#define PACKED __attribute__((packed))

enum ctrl
{
    reset = 0,
    bold_bright = 1,
    underline = 4,
    inverse = 7,
    bold_bright_off = 21,
    underline_off = 24,
    inverse_off = 27
};

enum fg
{
    fg_black = 30,
    fg_red = 31,
    fg_green = 32,
    fg_yellow = 33,
    fg_blue = 34,
    fg_magenta = 35,
    fg_cyan = 36,
    fg_white = 37
};

enum bg
{
    bg_black = 40,
    bg_red = 41,
    bg_green = 42,
    bg_yellow = 43,
    bg_blue = 44,
    bg_magenta = 45,
    bg_cyan = 46,
    bg_white = 47
};

inline auto ansi_bold_fg(const fg color) -> std::string
{
    return std::format("\033[{};{}m", static_cast<int>(bold_bright), static_cast<int>(color));
}

inline auto ansi_reset() -> std::string
{
    return std::format("\033[{}m", static_cast<int>(reset));
}

// Bit shifting + masking

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

// std::variant visitor

// https://en.cppreference.com/w/cpp/utility/variant/visit
template <class... Ts>
struct overloads : Ts...
{
    using Ts::operator()...;
};

// Enums

enum direction
{
    nor = 1 << 0,
    eas = 1 << 1,
    sou = 1 << 2,
    wes = 1 << 3,
};

// Output

inline auto operator<<(std::ostream& os, const Vector2& v) -> std::ostream&
{
    os << "(" << v.x << ", " << v.y << ")";
    return os;
}

inline auto operator<<(std::ostream& os, const Vector3& v) -> std::ostream&
{
    os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
    return os;
}

// std::println doesn't work with mingw
template <typename... Args>
auto traceln(std::format_string<Args...> fmt, Args&&... args) -> void
{
    std::cout << std::format("[{}TRACE{}]: ", ansi_bold_fg(fg_cyan), ansi_reset()) << std::format(
        fmt, std::forward<Args>(args)...) << std::endl;
}

template <typename... Args>
auto infoln(std::format_string<Args...> fmt, Args&&... args) -> void
{
    std::cout << std::format("[{}INFO{}]: ", ansi_bold_fg(fg_blue), ansi_reset()) << std::format(
        fmt, std::forward<Args>(args)...) << std::endl;
}

template <typename... Args>
auto warnln(std::format_string<Args...> fmt, Args&&... args) -> void
{
    std::cout << std::format("[{}WARNING{}]: ", ansi_bold_fg(fg_yellow), ansi_reset()) << std::format(
        fmt, std::forward<Args>(args)...) << std::endl;
}

template <typename... Args>
auto errln(std::format_string<Args...> fmt, Args&&... args) -> void
{
    std::cout << std::format("[{}ERROR{}]: ", ansi_bold_fg(fg_red), ansi_reset()) << std::format(
        fmt, std::forward<Args>(args)...) << std::endl;
}

inline auto print_bitmap(const uint64_t bitmap, const uint8_t w, const uint8_t h, const std::string& title) -> void
{
    traceln("{}:", title);
    traceln("{}", std::string(2 * w - 1, '='));
    for (size_t y = 0; y < w; ++y) {
        std::cout << "         ";
        for (size_t x = 0; x < h; ++x) {
            std::cout << static_cast<int>(get_bits(bitmap, y * w + x, y * h + x)) << " ";
        }
        std::cout << "\n";
    }
    std::cout << std::flush;
    traceln("{}", std::string(2 * w - 1, '='));
}

#endif