#ifndef UTIL_HPP_
#define UTIL_HPP_

#include <atomic>
#include <cmath>
#include <cstddef>
#include <format>
#include <iostream>
#include <raylib.h>
#include <string>
#include <vector>

#define INLINE __attribute__((always_inline))
#define PACKED __attribute__((packed))

#define STARTTIME const auto start = std::chrono::high_resolution_clock::now()
#define ENDTIME(msg, cast, unit)                                                                   \
    const auto end = std::chrono::high_resolution_clock::now();                                    \
    infoln("{}. Took {}{}.", msg, std::chrono::duration_cast<cast>(end - start).count(), unit)

#define COMMENT if (false)

#define NO_COPY_NO_MOVE(typename)                                                                  \
    typename(const typename& copy) = delete;                                                       \
    auto operator=(const typename& copy)->typename& = delete;                                      \
    typename(typename && move) = delete;                                                           \
    auto operator=(typename&& move)->typename& = delete;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

// https://en.cppreference.com/w/cpp/utility/variant/visit
template <class... Ts>
struct overloads : Ts...
{
    using Ts::operator()...;
};

inline auto binom(const int n, const int k) -> int
{
    std::vector<int> solutions(k);
    solutions[0] = n - k + 1;

    for (int i = 1; i < k; ++i) {
        solutions[i] = solutions[i - 1] * (n - k + 1 + i) / (i + 1);
    }

    return solutions[k - 1];
}

// Enums

enum dir : u8
{
    nor = 1 << 0,
    eas = 1 << 1,
    sou = 1 << 2,
    wes = 1 << 3,
};

// Ansi

enum class ctrl : u8
{
    reset = 0,
    bold_bright = 1,
    underline = 4,
    inverse = 7,
    bold_bright_off = 21,
    underline_off = 24,
    inverse_off = 27
};

enum class fg : u8
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

enum class bg : u8
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

// Log level: 0 = off (warnings/errors still print), 1 = info, 2 = detailed (trace).
extern std::atomic<int> g_log_level;

inline auto log_enabled(const int level) -> bool
{
    return g_log_level.load(std::memory_order_relaxed) >= level;
}

inline auto format_count(const size_t value) -> std::string
{
    std::string text = std::to_string(value);
    for (int i = static_cast<int>(text.size()) - 3; i > 0; i -= 3) {
        text.insert(static_cast<size_t>(i), 1, ',');
    }
    return text;
}

inline auto format_duration(const double seconds) -> std::string
{
    if (!std::isfinite(seconds) || seconds < 0.0) {
        return "--";
    }
    const auto total = static_cast<size_t>(std::round(seconds));
    if (total < 60) {
        return std::format("{}s", total);
    }
    const size_t minutes = total / 60;
    const size_t remaining = total % 60;
    if (minutes < 60) {
        return std::format("{}m {:02}s", minutes, remaining);
    }
    const size_t hours = minutes / 60;
    return std::format("{}h {:02}m", hours, minutes % 60);
}

inline auto format_gb(const size_t bytes) -> std::string
{
    constexpr double bytes_per_gb = 1024.0 * 1024.0 * 1024.0;
    return std::format("{:.2f} GB", static_cast<double>(bytes) / bytes_per_gb);
}

inline auto format_memory(const size_t bytes) -> std::string
{
    constexpr double bytes_per_kb = 1024.0;
    constexpr double bytes_per_mb = 1024.0 * 1024.0;
    constexpr double bytes_per_gb = 1024.0 * 1024.0 * 1024.0;
    if (bytes >= static_cast<size_t>(bytes_per_gb)) {
        return std::format("{:.2f} GB", static_cast<double>(bytes) / bytes_per_gb);
    }
    if (bytes >= static_cast<size_t>(bytes_per_mb)) {
        return std::format("{:.1f} MB", static_cast<double>(bytes) / bytes_per_mb);
    }
    if (bytes >= static_cast<size_t>(bytes_per_kb)) {
        return std::format("{:.0f} KB", static_cast<double>(bytes) / bytes_per_kb);
    }
    return std::format("{} B", bytes);
}

// std::println doesn't work with mingw
template <typename... Args>
auto traceln(std::format_string<Args...> fmt, Args&&... args) -> void
{
    if (!log_enabled(2)) {
        return;
    }
    std::cout << std::format("[{}TRACE{}]: ", ansi_bold_fg(fg::cyan), ansi_reset())
              << std::format(fmt, std::forward<Args>(args)...) << std::endl;
}

template <typename... Args>
auto infoln(std::format_string<Args...> fmt, Args&&... args) -> void
{
    if (!log_enabled(1)) {
        return;
    }
    std::cout << std::format("[{}INFO{}]: ", ansi_bold_fg(fg::blue), ansi_reset())
              << std::format(fmt, std::forward<Args>(args)...) << std::endl;
}

template <typename... Args>
auto warnln(std::format_string<Args...> fmt, Args&&... args) -> void
{
    std::cout << std::format("[{}WARNING{}]: ", ansi_bold_fg(fg::yellow), ansi_reset())
              << std::format(fmt, std::forward<Args>(args)...) << std::endl;
}

template <typename... Args>
auto errln(std::format_string<Args...> fmt, Args&&... args) -> void
{
    std::cout << std::format("[{}ERROR{}]: ", ansi_bold_fg(fg::red), ansi_reset())
              << std::format(fmt, std::forward<Args>(args)...) << std::endl;
}

#endif
