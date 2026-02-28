#ifndef UTIL_HPP_
#define UTIL_HPP_

#include <iostream>
#include <raylib.h>
#include <raymath.h>

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

// https://en.cppreference.com/w/cpp/utility/variant/visit
template <class... Ts>
struct overloads : Ts...
{
    using Ts::operator()...;
};

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
    return std::format("\033[1;{}m", static_cast<int>(color));
}

inline auto ansi_reset() -> std::string
{
    return "\033[0m";
}

// std::println doesn't work with mingw
template <typename... Args>
auto infoln(std::format_string<Args...> fmt, Args&&... args) -> void
{
    std::cout << std::format("[{}INFO{}]: ", ansi_bold_fg(fg_blue), ansi_reset())
              << std::format(fmt, std::forward<Args>(args)...) << std::endl;
}

template <typename... Args>
auto warnln(std::format_string<Args...> fmt, Args&&... args) -> void
{
    std::cout << std::format("[{}WARNING{}]: ", ansi_bold_fg(fg_yellow), ansi_reset())
              << std::format(fmt, std::forward<Args>(args)...) << std::endl;
}

template <typename... Args>
auto errln(std::format_string<Args...> fmt, Args&&... args) -> void
{
    std::cout << std::format("[{}ERROR{}]: ", ansi_bold_fg(fg_red), ansi_reset())
              << std::format(fmt, std::forward<Args>(args)...) << std::endl;
}

#endif
