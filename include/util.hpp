#ifndef UTIL_HPP_
#define UTIL_HPP_

#include <iostream>
#include <raylib.h>

#define INLINE __attribute__((always_inline))
#define PACKED __attribute__((packed))

// std::variant visitor

// https://en.cppreference.com/w/cpp/utility/variant/visit
template <class... Ts>
struct overloads : Ts...
{
    using Ts::operator()...;
};

// Enums

enum dir : uint8_t
{
    nor = 1 << 0,
    eas = 1 << 1,
    sou = 1 << 2,
    wes = 1 << 3,
};

// Ansi

enum class ctrl : uint8_t
{
    reset = 0,
    bold_bright = 1,
    underline = 4,
    inverse = 7,
    bold_bright_off = 21,
    underline_off = 24,
    inverse_off = 27
};

enum class fg : uint8_t
{
    black = 30,
    red = 31,
    green = 32,
    yellow = 33,
    blue = 34,
    magenta = 35,
    cyan = 36,
    white = 37
};

enum class bg : uint8_t
{
    black = 40,
    red = 41,
    green = 42,
    yellow = 43,
    blue = 44,
    magenta = 45,
    cyan = 46,
    white = 47
};

inline auto ansi_bold_fg(const fg color) -> std::string
{
    return std::format("\033[{};{}m", static_cast<int>(ctrl::bold_bright), static_cast<int>(color));
}

inline auto ansi_reset() -> std::string
{
    return std::format("\033[{}m", static_cast<int>(ctrl::reset));
}

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
    std::cout << std::format("[{}TRACE{}]: ", ansi_bold_fg(fg::cyan), ansi_reset()) << std::format(
        fmt, std::forward<Args>(args)...) << std::endl;
}

template <typename... Args>
auto infoln(std::format_string<Args...> fmt, Args&&... args) -> void
{
    std::cout << std::format("[{}INFO{}]: ", ansi_bold_fg(fg::blue), ansi_reset()) << std::format(
        fmt, std::forward<Args>(args)...) << std::endl;
}

template <typename... Args>
auto warnln(std::format_string<Args...> fmt, Args&&... args) -> void
{
    std::cout << std::format("[{}WARNING{}]: ", ansi_bold_fg(fg::yellow), ansi_reset()) << std::format(
        fmt, std::forward<Args>(args)...) << std::endl;
}

template <typename... Args>
auto errln(std::format_string<Args...> fmt, Args&&... args) -> void
{
    std::cout << std::format("[{}ERROR{}]: ", ansi_bold_fg(fg::red), ansi_reset()) << std::format(
        fmt, std::forward<Args>(args)...) << std::endl;
}

#endif