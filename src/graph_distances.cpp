#include "graph_distances.hpp"

#include <limits>

namespace
{
constexpr std::uint32_t INVALID_NODE = std::numeric_limits<std::uint32_t>::max();
}

auto graph_distances::clear() -> void
{
    distances.clear();
    parents.clear();
    nearest_targets.clear();
}

auto graph_distances::empty() const -> bool
{
    return distances.empty() || parents.empty() || nearest_targets.empty();
}

auto graph_distances::calculate_distances(const size_t node_count, const std::vector<spring>& edges,
                                          const std::vector<size_t>& targets, const bool directed)
    -> void
{
    if (node_count == 0) {
        clear();
        return;
    }

    // Build a compact CSR adjacency list. The previous vector-of-vectors representation created
    // one heap allocation per node and was the dominant cost for large graphs.
    //
    // For directed graphs we build the reverse adjacency and BFS from the targets. That yields the
    // distance from every node *to* a target, which is what the puzzle UI needs.
    std::vector<std::uint32_t> offsets(node_count + 1, 0);
    if (directed) {
        for (const auto& [from, to] : edges) {
            ++offsets[to + 1];
        }
    } else {
        for (const auto& [from, to] : edges) {
            ++offsets[from + 1];
            ++offsets[to + 1];
        }
    }
    for (size_t i = 0; i < node_count; ++i) {
        offsets[i + 1] += offsets[i];
    }

    std::vector<std::uint32_t> neighbors(offsets[node_count]);
    std::vector<std::uint32_t> cursor(offsets);
    if (directed) {
        for (const auto& [from, to] : edges) {
            neighbors[cursor[to]++] = static_cast<std::uint32_t>(from);
        }
    } else {
        for (const auto& [from, to] : edges) {
            neighbors[cursor[from]++] = static_cast<std::uint32_t>(to);
            neighbors[cursor[to]++] = static_cast<std::uint32_t>(from);
        }
    }

    distances.assign(node_count, -1);
    parents.assign(node_count, INVALID_NODE);
    nearest_targets.assign(node_count, INVALID_NODE);

    // Preallocated FIFO queue. Every node is enqueued at most once.
    std::vector<std::uint32_t> queue;
    queue.reserve(node_count);
    for (const size_t target : targets) {
        if (target >= node_count) {
            continue;
        }
        const auto node = static_cast<std::uint32_t>(target);
        distances[node] = 0;
        nearest_targets[node] = node;
        queue.push_back(node);
    }

    size_t head = 0;
    while (head < queue.size()) {
        const std::uint32_t current = queue[head++];
        const int next_distance = distances[current] + 1;

        for (std::uint32_t i = offsets[current]; i < offsets[current + 1]; ++i) {
            const std::uint32_t neighbor = neighbors[i];
            if (distances[neighbor] == -1) {
                distances[neighbor] = next_distance;
                parents[neighbor] = current;
                nearest_targets[neighbor] = nearest_targets[current];
                queue.push_back(neighbor);
            }
        }
    }
}

auto graph_distances::get_shortest_path(const size_t source) const -> std::vector<size_t>
{
    if (empty() || source >= distances.size() || distances[source] == -1) {
        // Unreachable
        return {};
    }

    std::vector<size_t> path;
    for (std::uint32_t n = static_cast<std::uint32_t>(source); n != INVALID_NODE; n = parents[n]) {
        path.push_back(n);
    }

    return path;
}
