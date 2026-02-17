#include "mass_springs.hpp"
#include "config.hpp"

#include <cstddef>
#include <raymath.h>

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
  displacement = Vector3Scale(displacement, 0.99);

  position = Vector3Add(Vector3Add(position, displacement), accel_term);
  previous_position = temp_position;
}

auto Spring::CalculateSpringForce() -> void {
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
                               std::string state) -> void {
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
  springs.emplace_back(GetMass(massA), GetMass(massB), spring_constant,
                       dampening_constant, rest_length);
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
  for (auto &spring : springs) {
    spring.CalculateSpringForce();
  }
}

auto MassSpringSystem::CalculateRepulsionForces() -> void {
  for (auto &[state, mass] : masses) {
    for (auto &[s, m] : masses) {
      Vector3 dx = Vector3Subtract(mass.position, m.position);

      // This can be accelerated with a spatial data structure
      if (Vector3Length(dx) >= 3 * DEFAULT_REST_LENGTH) {
        continue;
      }

      mass.force =
          Vector3Add(mass.force, Vector3Scale(Vector3Normalize(dx),
                                              DEFAULT_REPULSION_FORCE));
    }
  }
}

auto MassSpringSystem::EulerUpdate(const float delta_time) -> void {
  for (auto &[state, mass] : masses) {
    mass.CalculateVelocity(delta_time);
    mass.CalculatePosition(delta_time);
  }
}

auto MassSpringSystem::VerletUpdate(const float delta_time) -> void {
  for (auto &[state, mass] : masses) {
    mass.VerletUpdate(delta_time);
  }
}
