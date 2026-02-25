#include "distance.hpp"
#include "config.hpp"

#include <cstddef>
#include <iostream>
#include <queue>

#ifdef TRACY
#include "tracy.hpp"
#include <tracy/Tracy.hpp>
#endif

auto DistanceResult::Clear() -> void {
  distances.clear();
  parents.clear();
  nearest_targets.clear();
}

auto DistanceResult::Empty() -> bool {
  return distances.empty() || parents.empty() || nearest_targets.empty();
}

auto CalculateDistances(
    std::size_t node_count,
    const std::vector<std::pair<std::size_t, std::size_t>> &edges,
    const std::vector<std::size_t> &targets) -> DistanceResult {

  // Build a list of adjacent nodes to speed up BFS
  std::vector<std::vector<std::size_t>> adjacency(node_count);
  for (const auto &[from, to] : edges) {
    adjacency[from].push_back(to);
    adjacency[to].push_back(from);
  }
  // for (size_t i = 0; i < adjacency.size(); ++i) {
  //   std::cout << "Node " << i << "'s neighbors: ";
  //   for (const auto &neighbor : adjacency[i]) {
  //     std::cout << neighbor;
  //   }
  //   std::cout << "\n";
  // }
  // std::cout << std::endl;

  std::vector<int> distances(node_count, -1);
  std::vector<std::size_t> parents(node_count, -1);
  std::vector<std::size_t> nearest_targets(node_count, -1);

  std::queue<std::size_t> queue;
  for (std::size_t target : targets) {
    distances[target] = 0;
    nearest_targets[target] = target;
    queue.push(target);
  }

  while (!queue.empty()) {
    std::size_t current = queue.front();
    queue.pop();

    for (std::size_t neighbor : adjacency[current]) {
      // std::cout << "Visiting edge (" << current << "->" << neighbor << ")."
      //           << std::endl;
      if (distances[neighbor] == -1) {
        // If distance is -1 we haven't visited the node yet
        distances[neighbor] = distances[current] + 1;
        parents[neighbor] = current;
        nearest_targets[neighbor] = nearest_targets[current];
        // std::cout << "- Distance: " << distances[neighbor]
        //           << ", Parent: " << parents[neighbor]
        //           << ", Nearest Target: " << nearest_targets[neighbor] << "."
        //           << std::endl;
        queue.push(neighbor);
      }
    }
  }

  return {distances, parents, nearest_targets};
}

auto GetPath(const DistanceResult &result, std::size_t source)
    -> std::vector<std::size_t> {
  if (result.distances[source] == -1) {
    // Unreachable
    return {};
  }

  std::vector<std::size_t> path;
  for (std::size_t n = source; n != static_cast<std::size_t>(-1);
       n = result.parents[n]) {
    path.push_back(n);
  }

  return path;
}
