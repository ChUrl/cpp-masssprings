#include "mass_springs.hpp"
#include "config.hpp"

#include <format>
#include <raymath.h>
#include <unordered_map>
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

auto MassSpringSystem::AddMass(float mass, Vector3 position, bool fixed,
                               const std::string &state) -> void {
  if (!masses.contains(state)) {
    masses.insert(std::make_pair(state, Mass(mass, position, fixed)));
  }
}

auto MassSpringSystem::GetMass(const std::string &state) -> Mass & {
  return masses.at(state);
}

auto MassSpringSystem::AddSpring(const std::string &massA,
                                 const std::string &massB,
                                 float spring_constant,
                                 float dampening_constant, float rest_length)
    -> void {
  std::string states;
  if (std::hash<std::string>{}(massA) < std::hash<std::string>{}(massB)) {
    states = std::format("{}{}", massA, massB);
  } else {
    states = std::format("{}{}", massB, massA);
  }

  if (!springs.contains(states)) {
    springs.insert(std::make_pair(
        states, Spring(GetMass(massA), GetMass(massB), spring_constant,
                       dampening_constant, rest_length)));
  }
}

auto MassSpringSystem::Clear() -> void {
  masses.clear();
  springs.clear();
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

auto MassSpringSystem::CalculateRepulsionForces() -> void {
  const float INV_CELL = 1.0 / REPULSION_RANGE;

  struct CellKey {
    int x, y, z;
    bool operator==(const CellKey &other) const {
      return x == other.x && y == other.y && z == other.z;
    }
  };
  struct CellHash {
    size_t operator()(const CellKey &key) const {
      return ((size_t)key.x * 73856093) ^ ((size_t)key.y * 19349663) ^
             ((size_t)key.z * 83492791);
    }
  };

  // Accelerate with uniform grid
  std::unordered_map<CellKey, std::vector<Mass *>, CellHash> grid;
  grid.reserve(masses.size());

  for (auto &[state, mass] : masses) {
    CellKey key{
        (int)std::floor(mass.position.x * INV_CELL),
        (int)std::floor(mass.position.y * INV_CELL),
        (int)std::floor(mass.position.z * INV_CELL),
    };
    grid[key].push_back(&mass);
  }

  for (auto &[state, mass] : masses) {
    int cx = (int)std::floor(mass.position.x * INV_CELL);
    int cy = (int)std::floor(mass.position.y * INV_CELL);
    int cz = (int)std::floor(mass.position.z * INV_CELL);

    // Check all 27 neighboring cells (including own)
    for (int dx = -1; dx <= 1; ++dx) {
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dz = -1; dz <= 1; ++dz) {
          CellKey neighbor{cx + dx, cy + dy, cz + dz};
          auto it = grid.find(neighbor);
          if (it == grid.end()) {
            continue;
          }

          for (Mass *m : it->second) {
            if (m == &mass) {
              continue; // skip self
            }

            Vector3 diff = Vector3Subtract(mass.position, m->position);
            float len = Vector3Length(diff);

            if (len == 0.0f || len >= REPULSION_RANGE) {
              continue;
            }

            mass.force =
                Vector3Add(mass.force, Vector3Scale(Vector3Normalize(diff),
                                                    REPULSION_FORCE));
          }
        }
      }
    }
  }

  // Old method
  // for (auto &[state, mass] : masses) {
  //   for (auto &[s, m] : masses) {
  //     Vector3 dx = Vector3Subtract(mass.position, m.position);
  //
  //     // This can be accelerated with a spatial data structure
  //     if (Vector3Length(dx) >= 3 * REST_LENGTH) {
  //       continue;
  //     }
  //
  //     mass.force = Vector3Add(
  //         mass.force, Vector3Scale(Vector3Normalize(dx), REPULSION_FORCE));
  //   }
  // }
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
