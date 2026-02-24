#ifndef __PHYSICS_HPP_
#define __PHYSICS_HPP_

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>
#include <raylib.h>
#include <raymath.h>
#include <thread>
#include <tracy/Tracy.hpp>
#include <variant>
#include <vector>

#include "octree.hpp"

#ifndef WEB
#define BS_THREAD_POOL_NATIVE_EXTENSIONS
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
  std::size_t a;
  std::size_t b;

public:
  Spring(std::size_t _a, std::size_t _b) : a(_a), b(_b) {}

public:
  auto CalculateSpringForce(Mass &_a, Mass &_b) const -> void;
};

class MassSpringSystem {
private:
  Octree octree;

#ifndef WEB
  BS::thread_pool<BS::tp::none> threads;
#endif

public:
  // This is the main ownership of all the states/masses/springs.
  std::vector<Mass> masses;
  std::vector<Spring> springs;

public:
  MassSpringSystem()
      : threads(std::thread::hardware_concurrency() - 1, SetThreadName) {
    std::cout << "Using Barnes-Hut + octree repulsion force calculation."
              << std::endl;

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
  static auto SetThreadName(std::size_t idx) -> void;

  auto BuildOctree() -> void;

public:
  auto AddMass() -> void;

  auto AddSpring(int a, int b) -> void;

  auto Clear() -> void;

  auto ClearForces() -> void;

  auto CalculateSpringForces() -> void;

  auto CalculateRepulsionForces() -> void;

  auto VerletUpdate(float delta_time) -> void;
};

class ThreadedPhysics {
  struct AddMass {};
  struct AddSpring {
    std::size_t a;
    std::size_t b;
  };
  struct ClearGraph {};

  using Command = std::variant<AddMass, AddSpring, ClearGraph>;

  struct PhysicsState {
    TracyLockable(std::mutex, command_mtx);
    std::queue<Command> pending_commands;

    TracyLockable(std::mutex, data_mtx);
    std::condition_variable_any data_ready_cnd;
    std::condition_variable_any data_consumed_cnd;
    unsigned int ups = 0;
    bool data_ready = false;
    bool data_consumed = true;
    std::vector<Vector3> masses; // Read by renderer
    std::vector<std::pair<std::size_t, std::size_t>>
        springs; // Read by renderer

    std::atomic<bool> running{true};
  };

private:
  std::thread physics;

public:
  PhysicsState state;

public:
  ThreadedPhysics() : physics(PhysicsThread, std::ref(state)) {}

  ThreadedPhysics(const ThreadedPhysics &copy) = delete;
  ThreadedPhysics &operator=(const ThreadedPhysics &copy) = delete;
  ThreadedPhysics(ThreadedPhysics &&move) = delete;
  ThreadedPhysics &operator=(ThreadedPhysics &&move) = delete;

  ~ThreadedPhysics() {
    state.running = false;
    state.data_ready_cnd.notify_all();
    state.data_consumed_cnd.notify_all();
    physics.join();
  }

private:
  static auto PhysicsThread(PhysicsState &state) -> void;

public:
  auto AddMassCmd() -> void;

  auto AddSpringCmd(std::size_t a, std::size_t b) -> void;

  auto ClearCmd() -> void;

  auto AddMassSpringsCmd(
      std::size_t num_masses,
      const std::vector<std::pair<std::size_t, std::size_t>> &springs) -> void;
};

// https://en.cppreference.com/w/cpp/utility/variant/visit
template <class... Ts> struct overloads : Ts... {
  using Ts::operator()...;
};

#endif
