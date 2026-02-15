#include "mass_springs.hpp"

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

  Vector3 temp;

  temp = Vector3Scale(velocity, delta_time);
  position = Vector3Add(position, temp);
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

auto MassSpringSystem::AddMass(float mass, Vector3 position, bool fixed)
    -> void {
  masses.emplace_back(mass, position, fixed);
}

auto MassSpringSystem::GetMass(const size_t index) -> Mass & {
  return masses[index];
}

auto MassSpringSystem::AddSpring(int massA, int massB, float spring_constant,
                                 float dampening_constant, float rest_length)
    -> void {
  springs.emplace_back(masses[massA], masses[massB], spring_constant,
                       dampening_constant, rest_length);
}

auto MassSpringSystem::GetSpring(const size_t index) -> Spring & {
  return springs[index];
}

auto MassSpringSystem::ClearForces() -> void {
  for (auto &mass : masses) {
    mass.ClearForce();
  }
}

auto MassSpringSystem::CalculateSpringForces() -> void {
  for (auto &spring : springs) {
    spring.CalculateSpringForce();
  }
}

auto MassSpringSystem::IntegrateVelocities(const float delta_time) -> void {
  for (auto &mass : masses) {
    mass.CalculateVelocity(delta_time);
  }
}

auto MassSpringSystem::IntegratePositions(const float delta_time) -> void {
  for (auto &mass : masses) {
    mass.CalculatePosition(delta_time);
  }
}
