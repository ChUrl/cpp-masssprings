#include "mass_springs.hpp"
#include "config.hpp"

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

auto MassSpringSystem::BuildGrid() -> void {
  const float INV_CELL = 1.0f / REPULSION_RANGE;
  const int n = masses.size();

  // Collect pointers
  mass_vec.clear();
  mass_vec.reserve(n);
  for (auto &[state, mass] : masses) {
    mass_vec.push_back(&mass);
  }

  // Assign each particle a cell index
  auto cellID = [&](const Vector3 &p) -> int64_t {
    int x = (int)std::floor(p.x * INV_CELL);
    int y = (int)std::floor(p.y * INV_CELL);
    int z = (int)std::floor(p.z * INV_CELL);
    // Pack into a single int64 (assumes coords fit in 20 bits each)
    return ((int64_t)(x & 0xFFFFF) << 40) | ((int64_t)(y & 0xFFFFF) << 20) |
           (int64_t)(z & 0xFFFFF);
  };

  // Sort particles by cell
  indices.clear();
  indices.resize(n);
  std::iota(indices.begin(), indices.end(), 0);
  std::sort(indices.begin(), indices.end(), [&](int a, int b) {
    return cellID(mass_vec[a]->position) < cellID(mass_vec[b]->position);
  });

  // Build cell start/end table
  cell_ids.clear();
  cell_ids.resize(n);
  for (int i = 0; i < n; ++i) {
    cell_ids[i] = cellID(mass_vec[indices[i]]->position);
  }
}

auto MassSpringSystem::CalculateRepulsionForces() -> void {
  const float INV_CELL = 1.0f / REPULSION_RANGE;
  const int n = masses.size();

  if (last_build >= REPULSION_GRID_REFRESH ||
      masses.size() != last_masses_count ||
      springs.size() != last_springs_count) {
    BuildGrid();
    last_build = 0;
    last_masses_count = masses.size();
    last_springs_count = springs.size();
  }
  last_build++;

#pragma omp parallel for
  for (int i = 0; i < n; ++i) {
    Mass *mass = mass_vec[indices[i]];
    int cx = (int)std::floor(mass->position.x * INV_CELL);
    int cy = (int)std::floor(mass->position.y * INV_CELL);
    int cz = (int)std::floor(mass->position.z * INV_CELL);

    Vector3 force = {0, 0, 0};

    // Search all 3*3*3 neighbor cells for particles
    for (int dx = -1; dx <= 1; ++dx) {
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dz = -1; dz <= 1; ++dz) {
          int64_t nid = ((int64_t)((cx + dx) & 0xFFFFF) << 40) |
                        ((int64_t)((cy + dy) & 0xFFFFF) << 20) |
                        (int64_t)((cz + dz) & 0xFFFFF);

          // Binary search for this neighbor cell in sorted array
          auto lo = std::lower_bound(cell_ids.begin(), cell_ids.end(), nid);
          auto hi = std::upper_bound(cell_ids.begin(), cell_ids.end(), nid);

          for (auto it = lo; it != hi; ++it) {
            Mass *m = mass_vec[indices[it - cell_ids.begin()]];
            if (m == mass) {
              continue;
            }

            Vector3 diff = Vector3Subtract(mass->position, m->position);
            float len = Vector3Length(diff);
            if (len == 0.0f || len >= REPULSION_RANGE) {
              continue;
            }

            force = Vector3Add(
                force, Vector3Scale(Vector3Normalize(diff), REPULSION_FORCE));
          }
        }
      }
    }

    mass->force = Vector3Add(mass->force, force);
  }
}

auto MassSpringSystem::EulerUpdate(float delta_time) -> void {
  for (auto &[state, mass] : masses) {
    mass.CalculateVelocity(delta_time);
    mass.CalculatePosition(delta_time);
  }
}

auto MassSpringSystem::VerletUpdate(float delta_time) -> void {
  for (auto &[state, mass] : masses) {
    mass.VerletUpdate(delta_time);
  }
}

auto MassSpringSystem::InvalidateGrid() -> void {
  mass_vec.clear();
  indices.clear();
  cell_ids.clear();
  last_build = REPULSION_GRID_REFRESH;
  last_masses_count = 0;
  last_springs_count = 0;
}
