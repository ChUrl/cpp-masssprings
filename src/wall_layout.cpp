#include "wall_layout.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <functional>
#include <tuple>
#include <unordered_map>
#include <utility>

wall_layout_space::wall_layout_space(const u8 width, const u8 height, const blockset2& shapes,
                                     const size_t min_walls, const size_t max_walls,
                                     const std::stop_token stop)
{
    generate(width, height, shapes, min_walls, max_walls, stop);
}

auto wall_layout_space::layouts() const -> const std::vector<wall_layout>&
{
    return layouts_;
}

auto wall_layout_space::error() const -> const std::string&
{
    return error_;
}

auto wall_layout_space::cancelled() const -> bool
{
    return cancelled_;
}

auto wall_layout_space::generate(const u8 width, const u8 height, const blockset2& shapes,
                                 const size_t min_walls, const size_t max_walls,
                                 const std::stop_token stop) -> void
{
    if (min_walls > max_walls) {
        error_ = "Minimum walls cannot exceed maximum walls";
        return;
    }

    const int cells = static_cast<int>(width) * height;
    const size_t cell_limit = static_cast<size_t>(cells);
    const size_t cap = std::min(max_walls, cell_limit);

    // Normalise and sort the shape list so layout order is deterministic.
    std::vector<block> wall_shapes;
    wall_shapes.reserve(shapes.size());
    for (const block shape : shapes) {
        if (shape.get_width() <= width && shape.get_height() <= height) {
            wall_shapes.push_back(block(0, 0, shape.get_width(), shape.get_height(), false, false));
        }
    }
    std::ranges::sort(wall_shapes,
                      [](const block a, const block b)
                      {
                          return std::tuple(a.get_height(), a.get_width()) <
                              std::tuple(b.get_height(), b.get_width());
                      });
    wall_shapes.erase(std::unique(wall_shapes.begin(), wall_shapes.end()), wall_shapes.end());

    if (wall_shapes.empty() && min_walls > 0) {
        error_ = "No wall shapes fit on the board";
        return;
    }

    struct shape_masks
    {
        block shape;
        std::array<u64, 64> masks{};
    };
    std::vector<shape_masks> masks(wall_shapes.size());
    for (size_t s = 0; s < wall_shapes.size(); ++s) {
        const block shape = wall_shapes[s];
        masks[s].shape = shape;
        for (int cell = 0; cell < cells; ++cell) {
            const int x = cell % width;
            const int y = cell / width;
            if (x + shape.get_width() > width || y + shape.get_height() > height) {
                continue;
            }
            u64 mask = 0;
            for (int dy = 0; dy < shape.get_height(); ++dy) {
                for (int dx = 0; dx < shape.get_width(); ++dx) {
                    mask |= u64{1} << ((y + dy) * width + x + dx);
                }
            }
            masks[s].masks[static_cast<size_t>(cell)] = mask;
        }
    }

    std::unordered_map<u64, size_t> seen;
    std::function<void(int, u64, size_t)> recurse =
        [&](const int pos, const u64 occupied, const size_t placed)
    {
        if (cancelled_) {
            return;
        }
        if (stop.stop_requested()) {
            cancelled_ = true;
            return;
        }

        if (placed >= min_walls && placed <= cap) {
            auto [it, inserted] = seen.try_emplace(occupied, layouts_.size());
            if (inserted) {
                wall_layout layout;
                layout.mask = occupied;
                layout.pieces = placed;
                layout.wall_cells = static_cast<int>(std::popcount(occupied));
                layout.free_cells = cells - layout.wall_cells;
                layouts_.push_back(layout);
            } else if (placed < layouts_[it->second].pieces) {
                layouts_[it->second].pieces = placed;
            }
        }

        if (placed >= cap) {
            return;
        }

        int cell = pos;
        while (cell < cells && ((occupied >> cell) & 1u) != 0) {
            ++cell;
        }
        if (cell >= cells) {
            return;
        }

        for (const shape_masks& candidate : masks) {
            const u64 mask = candidate.masks[static_cast<size_t>(cell)];
            if (mask == 0 || (mask & occupied) != 0) {
                continue;
            }
            recurse(cell + 1, occupied | mask, placed + 1);
        }

        // Leave this cell empty and try to place the remaining pieces later.
        recurse(cell + 1, occupied, placed);
    };

    recurse(0, 0, 0);
}
