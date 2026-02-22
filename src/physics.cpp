#include "physics.hpp"
#include "config.hpp"

#include <algorithm>
#include <numeric>
#include <raylib.h>
#include <raymath.h>
#include <unordered_map>
#include <utility>
#include <vector>

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
  Vector3 delta_position;
  float current_length;
  Vector3 delta_velocity;
  Vector3 force_a;
  Vector3 force_b;

  delta_position = Vector3Subtract(massA.position, massB.position);
  current_length = Vector3Length(delta_position);
  delta_velocity = Vector3Subtract(massA.velocity, massB.velocity);

  float hooke = spring_constant * (current_length - rest_length);
  float dampening = dampening_constant *
                    Vector3DotProduct(delta_velocity, delta_position) /
                    current_length;

  force_a = Vector3Scale(delta_position, -(hooke + dampening) / current_length);
  force_b = Vector3Scale(force_a, -1.0);

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
  InvalidateGrid();
}

auto MassSpringSystem::ClearForces() -> void {
  for (auto &[state, mass] : masses) {
    mass.ClearForce();
  }
}

auto MassSpringSystem::CalculateSpringForces() -> void {
  for (auto &[states, spring] : springs) {
    spring.CalculateSpringForce();
  }
}

auto MassSpringSystem::BuildUniformGrid() -> void {
  // Collect pointers to all masses
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

auto MassSpringSystem::CalculateRepulsionForces() -> void {
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

  // TODO: Use Barnes-Hut + Octree
#pragma omp parallel for
  // Search the neighboring cells for each mass to calculate repulsion forces
  for (int i = 0; i < masses.size(); ++i) {
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
            if (distance == 0.0f || distance >= REPULSION_RANGE) {
              continue;
            }

            force = Vector3Add(force, Vector3Scale(Vector3Normalize(direction),
                                                   REPULSION_FORCE));
          }
        }
      }
    }

    mass->force = Vector3Add(mass->force, force);
  }
}

auto MassSpringSystem::VerletUpdate(float delta_time) -> void {
  for (auto &[state, mass] : masses) {
    mass.VerletUpdate(delta_time);
  }
}

auto MassSpringSystem::InvalidateGrid() -> void {
  mass_pointers.clear();
  mass_indices.clear();
  cell_ids.clear();
  last_build = REPULSION_GRID_REFRESH;
  last_masses_count = 0;
  last_springs_count = 0;
}
