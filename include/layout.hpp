#ifndef LAYOUT_HPP_
#define LAYOUT_HPP_

#include "config.hpp"

#include <algorithm>
#include <raylib.h>

// Always-visible overlays in the panes. They must stay outside the interactive area checks in
// input_handler so clicks on them do not move blocks or manipulate the graph camera.
inline constexpr int OVERLAY_MARGIN = 8;
inline constexpr int OVERLAY_FONT_SIZE = 18;
inline constexpr int OVERLAY_LINE_HEIGHT = 21;

// The lower part of the window is split into a narrow left column (controls above the board) and a
// wide graph column. All pane geometry is derived from the current window size.
inline auto content_height() -> int
{
    return GetScreenHeight() - COMPACT_MENU_HEIGHT;
}

inline auto left_pane_width() -> int
{
    return std::max(1, GetScreenWidth() / 3);
}

// Font size that keeps roughly the same amount of text per control in the narrow left column.
inline auto pane_font_size() -> int
{
    return std::clamp(left_pane_width() / 22, 16, 24);
}

inline auto right_pane_x() -> int
{
    return left_pane_width();
}

inline auto right_pane_width() -> int
{
    return std::max(1, GetScreenWidth() - left_pane_width());
}

// The left column is split evenly between the always-visible controls and the puzzle board.
inline auto controls_pane_height() -> int
{
    return std::max(1, content_height() / 2);
}

inline auto controls_pane_bounds() -> Rectangle
{
    return Rectangle{0.0f, static_cast<float>(COMPACT_MENU_HEIGHT),
                     static_cast<float>(left_pane_width()),
                     static_cast<float>(controls_pane_height())};
}

inline auto board_pane_bounds() -> Rectangle
{
    const int top = COMPACT_MENU_HEIGHT + controls_pane_height();
    return Rectangle{0.0f, static_cast<float>(top), static_cast<float>(left_pane_width()),
                     static_cast<float>(GetScreenHeight() - top)};
}

// Board coordinates are local to the render texture, which starts at the top of the left column.
inline auto board_canvas_bounds() -> Rectangle
{
    return Rectangle{
        0.0f, static_cast<float>(controls_pane_height()), static_cast<float>(left_pane_width()),
        static_cast<float>(GetScreenHeight() - COMPACT_MENU_HEIGHT - controls_pane_height())};
}

inline auto graph_pane_bounds() -> Rectangle
{
    return Rectangle{static_cast<float>(right_pane_x()), static_cast<float>(COMPACT_MENU_HEIGHT),
                     static_cast<float>(right_pane_width()), static_cast<float>(content_height())};
}

inline auto performance_overlay_bounds() -> Rectangle
{
    const int pane_width = right_pane_width();
    const int pane_height = content_height();
    const int width = std::max(1, std::min(360, pane_width - 2 * OVERLAY_MARGIN));
    const int height = std::max(1, std::min(156, pane_height - 2 * OVERLAY_MARGIN));
    return Rectangle{static_cast<float>(GetScreenWidth() - OVERLAY_MARGIN - width),
                     static_cast<float>(COMPACT_MENU_HEIGHT + OVERLAY_MARGIN),
                     static_cast<float>(width), static_cast<float>(height)};
}

inline auto memory_overlay_bounds() -> Rectangle
{
    const int pane_width = right_pane_width();
    const int pane_height = content_height();
    const int width = std::max(1, std::min(340, pane_width - 2 * OVERLAY_MARGIN));
    const int height = std::max(1, std::min(330, pane_height - 2 * OVERLAY_MARGIN));
    return Rectangle{static_cast<float>(GetScreenWidth() - OVERLAY_MARGIN - width),
                     static_cast<float>(GetScreenHeight() - OVERLAY_MARGIN - height),
                     static_cast<float>(width), static_cast<float>(height)};
}

#endif
