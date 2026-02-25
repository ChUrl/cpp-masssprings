#ifndef __DISTANCE_HPP_
#define __DISTANCE_HPP_

#include "config.hpp"

#include <cstddef>
#include <vector>

struct DistanceResult {
  // distances[n] = distance from n to target
  std::vector<int> distances;

  // parents[n] = next node on the path from n to target
  std::vector<std::size_t> parents;

  // nearest_target[n] = closest target node to n
  std::vector<std::size_t> nearest_targets;

  auto Clear() -> void;

  auto Empty() -> bool;
};

auto CalculateDistances(
    std::size_t node_count,
    const std::vector<std::pair<std::size_t, std::size_t>> &edges,
    const std::vector<std::size_t> &targets) -> DistanceResult;

auto GetPath(const DistanceResult &result, std::size_t source)
    -> std::vector<std::size_t>;

#endif
