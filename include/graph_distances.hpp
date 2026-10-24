#ifndef DISTANCE_HPP_
#define DISTANCE_HPP_

#include "cpu_spring_system.hpp"

#include <cstdint>
#include <vector>

class graph_distances
{
public:
    std::vector<int> distances;                 // distances[n] = distance from node n to target
    std::vector<std::uint32_t> parents;         // parents[n] = next node on the path to target
    std::vector<std::uint32_t> nearest_targets; // nearest_target[n] = closest target node to n

public:
    auto clear() -> void;
    [[nodiscard]] auto empty() const -> bool;

    auto calculate_distances(size_t node_count, const std::vector<spring>& edges,
                             const std::vector<size_t>& targets, bool directed = false) -> void;

    [[nodiscard]] auto get_shortest_path(size_t source) const -> std::vector<size_t>;
};

#endif
