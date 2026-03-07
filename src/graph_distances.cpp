#include "graph_distances.hpp"

#include <queue>

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

auto graph_distances::calculate_distances(const size_t node_count,
                                          const std::vector<spring>& edges,
                                          const std::vector<size_t>& targets) -> void
{
    // Build a list of adjacent nodes to speed up BFS
    std::vector<std::vector<size_t>> adjacency(node_count);
    for (const auto& [from, to] : edges) {
        adjacency[from].push_back(to);
        adjacency[to].push_back(from);
    }

    distances = std::vector<int>(node_count, -1);
    parents = std::vector<size_t>(node_count, -1);
    nearest_targets = std::vector<size_t>(node_count, -1);

    std::queue<size_t> queue;
    for (size_t target : targets) {
        distances[target] = 0;
        nearest_targets[target] = target;
        queue.push(target);
    }

    while (!queue.empty()) {
        const size_t current = queue.front();
        queue.pop();

        for (size_t neighbor : adjacency[current]) {
            if (distances[neighbor] == -1) {
                // If distance is -1 we haven't visited the node yet
                distances[neighbor] = distances[current] + 1;
                parents[neighbor] = current;
                nearest_targets[neighbor] = nearest_targets[current];

                queue.push(neighbor);
            }
        }
    }
}

auto graph_distances::get_shortest_path(const size_t source) const -> std::vector<size_t>
{
    if (empty() || distances[source] == -1) {
        // Unreachable
        return {};
    }

    std::vector<size_t> path;
    for (size_t n = source; n != static_cast<size_t>(-1); n = parents[n]) {
        path.push_back(n);
    }

    return path;
}