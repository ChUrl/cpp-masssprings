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
  Vector3 position;
  Vector3 previous_position; // for verlet integration
  Vector3 velocity;
  Vector3 force;

public:
  Mass(Vector3 _position)
      : position(_position), previous_position(_position),
        velocity(Vector3Zero()), force(Vector3Zero()) {}

public:
  auto ClearForce() -> void;

  auto CalculateVelocity(const float delta_time) -> void;

  auto CalculatePosition(const float delta_time) -> void;

  auto VerletUpdate(const float delta_time) -> void;
};

class Spring {
public:
  int mass_a;
  int mass_b;

public:
  Spring(int _mass_a, int _mass_b) : mass_a(_mass_a), mass_b(_mass_b) {}

public:
  auto CalculateSpringForce(Mass &_mass_a, Mass &_mass_b) const -> void;
};

class MassSpringSystem {
private:
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
  std::vector<Mass> masses;
  std::unordered_map<State, int> state_masses;
  std::vector<Spring> springs;
  std::unordered_map<std::pair<State, State>, int> state_springs;

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
