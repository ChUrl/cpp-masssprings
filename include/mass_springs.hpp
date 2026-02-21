#ifndef __MASS_SPRINGS_HPP_
#define __MASS_SPRINGS_HPP_

#include "config.hpp"
#include "klotski.hpp"
#include <raylib.h>
#include <raymath.h>
#include <unordered_map>
#include <vector>

class Mass {
public:
  const float mass;
  Vector3 position;
  Vector3 previous_position; // for verlet integration
  Vector3 velocity;
  Vector3 force;
  const bool fixed;

public:
  Mass(float mass, Vector3 position, bool fixed)
      : mass(mass), position(position), previous_position(position),
        velocity(Vector3Zero()), force(Vector3Zero()), fixed(fixed) {}

  Mass(const Mass &copy)
      : mass(copy.mass), position(copy.position),
        previous_position(copy.previous_position), velocity(copy.velocity),
        force(copy.force), fixed(copy.fixed) {};

  Mass &operator=(const Mass &copy) = delete;

  Mass(Mass &&move)
      : mass(move.mass), position(move.position),
        previous_position(move.previous_position), velocity(move.velocity),
        force(move.force), fixed(move.fixed) {};

  Mass &operator=(Mass &&move) = delete;

  ~Mass() {}

public:
  auto ClearForce() -> void;

  auto CalculateVelocity(const float delta_time) -> void;

  auto CalculatePosition(const float delta_time) -> void;

  auto VerletUpdate(const float delta_time) -> void;
};

class Spring {
public:
  Mass &massA;
  Mass &massB;
  const float spring_constant;
  const float dampening_constant;
  const float rest_length;

public:
  Spring(Mass &massA, Mass &massB, float spring_constant,
         float dampening_constant, float rest_length)
      : massA(massA), massB(massB), spring_constant(spring_constant),
        dampening_constant(dampening_constant), rest_length(rest_length) {}

  Spring(const Spring &copy)
      : massA(copy.massA), massB(copy.massB),
        spring_constant(copy.spring_constant),
        dampening_constant(copy.dampening_constant),
        rest_length(copy.rest_length) {};

  Spring &operator=(const Spring &copy) = delete;

  Spring(Spring &&move)
      : massA(move.massA), massB(move.massB),
        spring_constant(move.spring_constant),
        dampening_constant(move.dampening_constant),
        rest_length(move.rest_length) {}

  Spring &operator=(Spring &&move) = delete;

  ~Spring() {}

public:
  auto CalculateSpringForce() const -> void;
};

class MassSpringSystem {
private:
  // TODO: Use references
  std::vector<Mass *> mass_vec;
  std::vector<int> indices;
  std::vector<int64_t> cell_ids;
  int last_build;
  int last_masses_count;
  int last_springs_count;

public:
  // This is the main ownership of all the states/masses/springs.
  // Everything is stored multiple times but idc.
  std::unordered_map<State, Mass> masses;
  std::unordered_map<std::pair<State, State>, Spring> springs;

public:
  MassSpringSystem() : last_build(REPULSION_GRID_REFRESH) {};

  MassSpringSystem(const MassSpringSystem &copy) = delete;
  MassSpringSystem &operator=(const MassSpringSystem &copy) = delete;
  MassSpringSystem(MassSpringSystem &move) = delete;
  MassSpringSystem &operator=(MassSpringSystem &&move) = delete;

  ~MassSpringSystem() {};

private:
  auto BuildGrid() -> void;

public:
  auto AddMass(float mass, bool fixed, const State &state) -> void;

  auto GetMass(const State &state) -> Mass &;

  auto AddSpring(const State &massA, const State &massB, float spring_constant,
                 float dampening_constant, float rest_length) -> void;

  auto Clear() -> void;

  auto ClearForces() -> void;

  auto CalculateSpringForces() -> void;

  auto CalculateRepulsionForces() -> void;

  auto EulerUpdate(float delta_time) -> void;

  auto VerletUpdate(float delta_time) -> void;

  auto InvalidateGrid() -> void;
};

#endif
