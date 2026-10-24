#ifndef WALL_LAYOUT_HPP_
#define WALL_LAYOUT_HPP_

#include "puzzle.hpp"

#include <cstddef>
#include <cstdint>
#include <stop_token>
#include <string>
#include <vector>

/**
 * One fixed wall arrangement produced by a wall inventory. The mask contains one bit per wall
 * cell. `pieces` is the number of wall shapes used by the canonical decomposition that produced
 * the mask; several decompositions can produce the same mask, in which case the smallest piece
 * count is kept.
 */
struct wall_layout
{
    u64 mask = 0;
    size_t pieces = 0;

    // Filled by placement_space once the target movement range is known.
    u64 target_allowed = 0;
    int target_last_cell = -1;
    int wall_cells = 0;
    int free_cells = 0;
};

/**
 * Enumerates all non-overlapping placements of the permitted rectangular wall shapes whose total
 * piece count is in [min_walls, max_walls]. There is intentionally no arbitrary layout cap: the
 * caller is responsible for choosing a sane wall count. Enumeration is cancellable.
 */
class wall_layout_space
{
public:
    wall_layout_space(u8 width, u8 height, const blockset2& shapes, size_t min_walls,
                      size_t max_walls, std::stop_token stop = {});

    [[nodiscard]] auto layouts() const -> const std::vector<wall_layout>&;
    [[nodiscard]] auto error() const -> const std::string&;
    [[nodiscard]] auto cancelled() const -> bool;

private:
    std::vector<wall_layout> layouts_;
    std::string error_;
    bool cancelled_ = false;

    auto generate(u8 width, u8 height, const blockset2& shapes, size_t min_walls, size_t max_walls,
                  std::stop_token stop) -> void;
};

#endif
