#ifndef __PHYSICS_HPP_
#define __PHYSICS_HPP_

#include <raylib.h>
#include <raymath.h>
#include <unordered_map>
#include <vector>

#include "config.hpp"
#include "puzzle.hpp"

#ifdef BARNES_HUT
#include "octree.hpp"
#endif

#ifndef WEB
#include <BS_thread_pool.hpp>
#endif

class Mass {
public:
  const float mass;
  Vector3 position;
  Vector3 previous_position; // for verlet integration
  Vector3 velocity;
  Vector3 force;
  const bool fixed;

public:
  Mass(float _mass, Vector3 _position, bool _fixed)
      : mass(_mass), position(_position), previous_position(_position),
        velocity(Vector3Zero()), force(Vector3Zero()), fixed(_fixed) {}

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
  Spring(Mass &_massA, Mass &_massB, float _spring_constant,
         float _dampening_constant, float _rest_length)
      : massA(_massA), massB(_massB), spring_constant(_spring_constant),
        dampening_constant(_dampening_constant), rest_length(_rest_length) {}

public:
  auto CalculateSpringForce() const -> void;
};

class MassSpringSystem {
private:
  std::vector<Mass *> mass_pointers;

#ifdef BARNES_HUT
  // Barnes-Hut
  Octree octree;
#else
  // Uniform grid
  std::vector<int> mass_indices;
  std::vector<int64_t> cell_ids;
  int last_build;
  int last_masses_count;
  int last_springs_count;
#endif

#ifndef WEB
  BS::thread_pool<BS::tp::none> threads;
#endif

public:
  // This is the main ownership of all the states/masses/springs.
  // TODO: Everything is stored multiple times but idc (currently).
  std::unordered_map<State, Mass> masses;
  std::unordered_map<std::pair<State, State>, Spring> springs;

public:
  MassSpringSystem() {
#ifndef BARNES_HUT
    last_build = REPULSION_GRID_REFRESH;
    std::cout << "Using uniform grid repulsion force calculation." << std::endl;
#else
    std::cout << "Using Barnes-Hut + octree repulsion force calculation."
              << std::endl;
#endif

#ifndef WEB
    std::cout << "Thread-Pool: " << threads.get_thread_count() << " threads."
              << std::endl;
#endif
  };

  MassSpringSystem(const MassSpringSystem &copy) = delete;
  MassSpringSystem &operator=(const MassSpringSystem &copy) = delete;
  MassSpringSystem(MassSpringSystem &move) = delete;
  MassSpringSystem &operator=(MassSpringSystem &&move) = delete;

private:
#ifdef BARNES_HUT
  auto BuildOctree() -> void;
#else
  auto BuildUniformGrid() -> void;
#endif

public:
  auto AddMass(float mass, bool fixed, const State &state) -> void;

  auto GetMass(const State &state) -> Mass &;

  auto GetMass(const State &state) const -> const Mass &;

  auto AddSpring(const State &massA, const State &massB, float spring_constant,
                 float dampening_constant, float rest_length) -> void;

  auto Clear() -> void;

  auto ClearForces() -> void;

  auto CalculateSpringForces() -> void;

  auto CalculateRepulsionForces() -> void;

  auto VerletUpdate(float delta_time) -> void;

#ifndef BARNES_HUT
  auto InvalidateGrid() -> void;
#endif
};

#endif
