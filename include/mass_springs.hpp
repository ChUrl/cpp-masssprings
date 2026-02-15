#ifndef __MASS_SPRINGS_HPP_
#define __MASS_SPRINGS_HPP_

#include <cstddef>
#include <raylib.h>
#include <vector>

class Mass {
public:
  float mass;
  Vector3 position;
  Vector3 velocity;
  Vector3 force;
  bool fixed;

public:
  Mass(float mass, Vector3 position, bool fixed)
      : mass(mass), position(position), fixed(fixed) {}

  Mass(const Mass &copy)
      : mass(copy.mass), position(copy.position), fixed(copy.fixed) {};

  Mass &operator=(const Mass &copy) = delete;

  Mass(Mass &&move)
      : mass(move.mass), position(move.position), fixed(move.fixed) {};

  Mass &operator=(Mass &&move) = delete;

  ~Mass() {}

public:
  auto ClearForce() -> void;

  auto CalculateVelocity(const float delta_time) -> void;

  auto CalculatePosition(const float delta_time) -> void;
};

using MassList = std::vector<Mass>;

class Spring {
public:
  Mass &massA;
  Mass &massB;
  int indexB;
  float spring_constant;
  float dampening_constant;
  float rest_length;

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
  auto CalculateSpringForce() -> void;
};

using SpringList = std::vector<Spring>;

class MassSpringSystem {
public:
  MassList masses;
  SpringList springs;

public:
  MassSpringSystem() {};

  MassSpringSystem(const MassSpringSystem &copy) = delete;
  MassSpringSystem &operator=(const MassSpringSystem &copy) = delete;
  MassSpringSystem(MassSpringSystem &move) = delete;
  MassSpringSystem &operator=(MassSpringSystem &&move) = delete;

  ~MassSpringSystem() {};

public:
  auto AddMass(float mass, Vector3 position, bool fixed) -> void;

  auto GetMass(const size_t index) -> Mass &;

  auto AddSpring(int massA, int massB, float spring_constant,
                 float dampening_constant, float rest_length) -> void;

  auto GetSpring(const size_t index) -> Spring &;

  auto ClearForces() -> void;

  auto CalculateSpringForces() -> void;

  auto IntegrateVelocities(const float delta_time) -> void;

  auto IntegratePositions(const float delta_time) -> void;
};

#endif
