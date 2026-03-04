#ifndef DISTANCE_HPP_
#define DISTANCE_HPP_

#include <cstddef>
#include <vector>

class graph_distances
{
public:
    std::vector<int> distances;          // distances[n] = distance from node n to target
    std::vector<size_t> parents;         // parents[n] = next node on the path from node n to target
    std::vector<size_t> nearest_targets; // nearest_target[n] = closest target node to  node n

public:
    auto clear() -> void;
    [[nodiscard]] auto empty() const -> bool;

    auto calculate_distances(size_t node_count, const std::vector<std::pair<size_t, size_t>>& edges,
                             const std::vector<size_t>& targets) -> void;

    [[nodiscard]] auto get_shortest_path(size_t source) const -> std::vector<size_t>;
};

#endif