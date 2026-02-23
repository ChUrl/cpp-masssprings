#include "physics.hpp"
#include "config.hpp"
#include "tracy.hpp"

#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <raylib.h>
#include <raymath.h>
#include <tracy/Tracy.hpp>
#include <unordered_map>
#include <utility>
#include <vector>

#ifndef BARNES_HUT
#include <numeric>
#endif

auto Mass::ClearForce() -> void { force = Vector3Zero(); }

auto Mass::CalculateVelocity(const float delta_time) -> void {
  if (fixed) {
    return;
  }

  Vector3 acceleration;
  Vector3 temp;

  acceleration = Vector3Scale(force, 1.0 / mass);
  temp = Vector3Scale(acceleration, delta_time);
  velocity = Vector3Add(velocity, temp);
}

auto Mass::CalculatePosition(const float delta_time) -> void {
  if (fixed) {
    return;
  }

  previous_position = position;

  Vector3 temp;

  temp = Vector3Scale(velocity, delta_time);
  position = Vector3Add(position, temp);
}

auto Mass::VerletUpdate(const float delta_time) -> void {
  if (fixed) {
    return;
  }

  Vector3 acceleration = Vector3Scale(force, 1.0 / mass);
  Vector3 temp_position = position;

  Vector3 displacement = Vector3Subtract(position, previous_position);
  Vector3 accel_term = Vector3Scale(acceleration, delta_time * delta_time);

  // Minimal dampening
  displacement = Vector3Scale(displacement, 1.0 - VERLET_DAMPENING);

  position = Vector3Add(Vector3Add(position, displacement), accel_term);
  previous_position = temp_position;
}

auto Spring::CalculateSpringForce() const -> void {
  Vector3 delta_position = Vector3Subtract(massA.position, massB.position);
  float current_length = Vector3Length(delta_position);
  float inv_current_length = 1.0 / current_length;
  Vector3 delta_velocity = Vector3Subtract(massA.velocity, massB.velocity);

  float hooke = spring_constant * (current_length - rest_length);
  float dampening = dampening_constant *
                    Vector3DotProduct(delta_velocity, delta_position) *
                    inv_current_length;

  Vector3 force_a =
      Vector3Scale(delta_position, -(hooke + dampening) * inv_current_length);
  Vector3 force_b = Vector3Scale(force_a, -1.0);

  massA.force = Vector3Add(massA.force, force_a);
  massB.force = Vector3Add(massB.force, force_b);
}

auto MassSpringSystem::AddMass(float mass, bool fixed, const State &state)
    -> void {
  if (!masses.contains(state)) {
    masses.insert(
        std::make_pair(state.state, Mass(mass, Vector3Zero(), fixed)));
  }
}

auto MassSpringSystem::GetMass(const State &state) -> Mass & {
  return masses.at(state);
}

auto MassSpringSystem::GetMass(const State &state) const -> const Mass & {
  return masses.at(state);
}

auto MassSpringSystem::AddSpring(const State &massA, const State &massB,
                                 float spring_constant,
                                 float dampening_constant, float rest_length)
    -> void {
  std::pair<State, State> key = std::make_pair(massA, massB);
  if (!springs.contains(key)) {
    Mass &a = GetMass(massA);
    Mass &b = GetMass(massB);

    Vector3 position = a.position;
    Vector3 offset = Vector3(static_cast<float>(GetRandomValue(-100, 100)),
                             static_cast<float>(GetRandomValue(-100, 100)),
                             static_cast<float>(GetRandomValue(-100, 100)));
    offset = Vector3Scale(Vector3Normalize(offset), REST_LENGTH);

    if (b.position == Vector3Zero()) {
      b.position = Vector3Add(position, offset);
    }

    springs.insert(std::make_pair(
        key, Spring(a, b, spring_constant, dampening_constant, rest_length)));
  }
}

auto MassSpringSystem::Clear() -> void {
  masses.clear();
  springs.clear();
#ifndef BARNES_HUT
  InvalidateGrid();
#endif
}

auto MassSpringSystem::ClearForces() -> void {
  ZoneScoped;

  for (auto &[state, mass] : masses) {
    mass.ClearForce();
  }
}

auto MassSpringSystem::CalculateSpringForces() -> void {
  ZoneScoped;

  for (auto &[states, spring] : springs) {
    spring.CalculateSpringForce();
  }

  // spring_pointers.clear();
  // spring_pointers.reserve(springs.size());
  // for (auto &[states, spring] : springs) {
  //   spring_pointers.push_back(&spring);
  // }
  //
  // auto solve_spring = [&](int i) {
  //   spring_pointers[i]->CalculateSpringForce();
  // };
  //
  // BS::multi_future<void> loop_future =
  //     threads.submit_loop(0, spring_pointers.size(), solve_spring, 4096);
  // loop_future.wait();
}

#ifdef BARNES_HUT
auto MassSpringSystem::BuildOctree() -> void {
  ZoneScoped;

  octree.nodes.clear();
  octree.nodes.reserve(masses.size() * 2);

  // Compute bounding box around all masses
  Vector3 min = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
  Vector3 max = Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
  for (const auto &[state, mass] : masses) {
    min.x = std::min(min.x, mass.position.x);
    max.x = std::max(max.x, mass.position.x);
    min.y = std::min(min.y, mass.position.y);
    max.y = std::max(max.y, mass.position.y);
    min.z = std::min(min.z, mass.position.z);
    max.z = std::max(max.z, mass.position.z);
  }

  // Pad the bounding box
  float pad = 1.0;
  min = Vector3Subtract(min, Vector3Scale(Vector3One(), pad));
  max = Vector3Add(max, Vector3Scale(Vector3One(), pad));

  // Make it cubic (so subdivisions are balanced)
  float max_extent = std::max({max.x - min.x, max.y - min.y, max.z - min.z});
  max = Vector3Add(min, Vector3Scale(Vector3One(), max_extent));

  // Root node spans the entire area
  int root = octree.CreateNode(min, max);

  // Use a vector of pointers to the masses, because we can't parallelize the
  // range-based for loop over the masses unordered_map using OpenMP.
  mass_pointers.clear();
  mass_pointers.reserve(masses.size());
  for (auto &[state, mass] : masses) {
    mass_pointers.push_back(&mass);
  }
  for (std::size_t i = 0; i < mass_pointers.size(); ++i) {
    octree.Insert(root, i, mass_pointers[i]->position, mass_pointers[i]->mass);
  }
}

#else

auto MassSpringSystem::BuildUniformGrid() -> void {
  // Use a vector of pointers to masses, because we can't parallelize the
  // range-based for loop over the masses unordered_map using OpenMP.
  mass_pointers.clear();
  mass_pointers.reserve(masses.size());
  for (auto &[state, mass] : masses) {
    mass_pointers.push_back(&mass);
  }

  // Assign each mass a cell_id based on its position.
  auto cell_id = [&](const Vector3 &position) -> int64_t {
    int x = (int)std::floor(position.x / REPULSION_RANGE);
    int y = (int)std::floor(position.y / REPULSION_RANGE);
    int z = (int)std::floor(position.z / REPULSION_RANGE);
    // Pack into a single int64 (assumes a coordinate fits in 20 bits)
    return ((int64_t)(x & 0xFFFFF) << 40) | ((int64_t)(y & 0xFFFFF) << 20) |
           (int64_t)(z & 0xFFFFF);
  };

  // Sort mass indices by cell_id to improve cache locality and allow cell
  // iteration with std::lower_bound and std::upper_bound
  mass_indices.clear();
  mass_indices.resize(masses.size());
  std::iota(mass_indices.begin(), mass_indices.end(),
            0); // Fill the indices array with ascending numbers
  std::sort(mass_indices.begin(), mass_indices.end(), [&](int a, int b) {
    return cell_id(mass_pointers[a]->position) <
           cell_id(mass_pointers[b]->position);
  });

  // Build cell start/end table: maps mass index to cell_id.
  // All indices of a single cell are consecutive.
  cell_ids.clear();
  cell_ids.resize(masses.size());
  for (int i = 0; i < masses.size(); ++i) {
    cell_ids[i] = cell_id(mass_pointers[mass_indices[i]]->position);
  }
}
#endif

auto MassSpringSystem::CalculateRepulsionForces() -> void {
  ZoneScoped;

#ifdef BARNES_HUT
  BuildOctree();

  auto solve_octree = [&](int i) {
    int root = 0;
    Vector3 force = octree.CalculateForce(root, mass_pointers[i]->position);

    mass_pointers[i]->force = Vector3Add(mass_pointers[i]->force, force);
  };

// Calculate forces using Barnes-Hut
#ifdef WEB
  for (int i = 0; i < mass_pointers.size(); ++i) {
    solve_octree(i);
  }
#else
  BS::multi_future<void> loop_future =
      threads.submit_loop(0, mass_pointers.size(), solve_octree, 256);
  loop_future.wait();
#endif

#else

  // Refresh grid if necessary
  if (last_build >= REPULSION_GRID_REFRESH ||
      masses.size() != last_masses_count ||
      springs.size() != last_springs_count) {
    BuildUniformGrid();
    last_build = 0;
    last_masses_count = masses.size();
    last_springs_count = springs.size();
  }
  last_build++;

  auto solve_grid = [&](int i) {
    Mass *mass = mass_pointers[mass_indices[i]];
    int cell_x = (int)std::floor(mass->position.x / REPULSION_RANGE);
    int cell_y = (int)std::floor(mass->position.y / REPULSION_RANGE);
    int cell_z = (int)std::floor(mass->position.z / REPULSION_RANGE);

    Vector3 force = Vector3Zero();

    // Search all 3*3*3 neighbor cells for masses
    for (int dx = -1; dx <= 1; ++dx) {
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dz = -1; dz <= 1; ++dz) {
          int64_t neighbor_id = ((int64_t)((cell_x + dx) & 0xFFFFF) << 40) |
                                ((int64_t)((cell_y + dy) & 0xFFFFF) << 20) |
                                (int64_t)((cell_z + dz) & 0xFFFFF);

          // Find the first and last occurence of the neighbor_id (iterator).
          // Because cell_ids is sorted, all elements of this cell are between
          // those.
          // If there is no cell, the iterators just won't do anything.
          auto cell_start =
              std::lower_bound(cell_ids.begin(), cell_ids.end(), neighbor_id);
          auto cell_end =
              std::upper_bound(cell_ids.begin(), cell_ids.end(), neighbor_id);

          // For each mass, iterate through all the masses of neighboring cells
          // to accumulate the repulsion forces.
          // This is slow with O(n * m), where m is the number of masses in each
          // neighboring cell.
          for (auto it = cell_start; it != cell_end; ++it) {
            Mass *neighbor = mass_pointers[mass_indices[it - cell_ids.begin()]];
            if (neighbor == mass) {
              // Skip ourselves
              continue;
            }

            Vector3 direction =
                Vector3Subtract(mass->position, neighbor->position);
            float distance = Vector3Length(direction);
            if (std::abs(distance) <= 0.001f || distance >= REPULSION_RANGE) {
              continue;
            }

            force = Vector3Add(
                force, Vector3Scale(Vector3Normalize(direction), GRID_FORCE));
          }
        }
      }
    }

    mass->force = Vector3Add(mass->force, force);
  };

  // Calculate forces using uniform grid
#ifdef WEB
  // Search the neighboring cells for each mass to calculate repulsion forces
  for (int i = 0; i < mass_pointers.size(); ++i) {
    calculate_grid(i);
  }
#else
  BS::multi_future<void> loop_future =
      threads.submit_loop(0, mass_pointers.size(), solve_grid, 512);
  loop_future.wait();
#endif

#endif
}

auto MassSpringSystem::VerletUpdate(float delta_time) -> void {
  ZoneScoped;

  for (auto &[state, mass] : masses) {
    mass.VerletUpdate(delta_time);
  }
}

#ifndef BARNES_HUT
auto MassSpringSystem::InvalidateGrid() -> void {
  mass_pointers.clear();
  mass_indices.clear();
  cell_ids.clear();
  last_build = REPULSION_GRID_REFRESH;
  last_masses_count = 0;
  last_springs_count = 0;
}
#endif
