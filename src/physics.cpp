#include "physics.hpp"
#include "config.hpp"

#include <BS_thread_pool.hpp>
#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cstddef>
#include <raylib.h>
#include <raymath.h>
#include <utility>
#include <vector>

#ifdef TRACY
#include "tracy.hpp"
#include <tracy/Tracy.hpp>
#endif

auto Mass::ClearForce() -> void { force = Vector3Zero(); }

auto Mass::CalculateVelocity(const float delta_time) -> void {
  Vector3 acceleration;
  Vector3 temp;

  acceleration = Vector3Scale(force, 1.0 / MASS);
  temp = Vector3Scale(acceleration, delta_time);
  velocity = Vector3Add(velocity, temp);
}

auto Mass::CalculatePosition(const float delta_time) -> void {
  previous_position = position;

  Vector3 temp;

  temp = Vector3Scale(velocity, delta_time);
  position = Vector3Add(position, temp);
}

auto Mass::VerletUpdate(const float delta_time) -> void {
  Vector3 acceleration = Vector3Scale(force, 1.0 / MASS);
  Vector3 temp_position = position;

  Vector3 displacement = Vector3Subtract(position, previous_position);
  Vector3 accel_term = Vector3Scale(acceleration, delta_time * delta_time);

  // Minimal dampening
  displacement = Vector3Scale(displacement, 1.0 - VERLET_DAMPENING);

  position = Vector3Add(Vector3Add(position, displacement), accel_term);
  previous_position = temp_position;
}

auto Spring::CalculateSpringForce(Mass &_mass_a, Mass &_mass_b) const -> void {
  Vector3 delta_position = Vector3Subtract(_mass_a.position, _mass_b.position);
  float current_length = Vector3Length(delta_position);
  float inv_current_length = 1.0 / current_length;
  Vector3 delta_velocity = Vector3Subtract(_mass_a.velocity, _mass_b.velocity);

  float hooke = SPRING_CONSTANT * (current_length - REST_LENGTH);
  float dampening = DAMPENING_CONSTANT *
                    Vector3DotProduct(delta_velocity, delta_position) *
                    inv_current_length;

  Vector3 force_a =
      Vector3Scale(delta_position, -(hooke + dampening) * inv_current_length);
  Vector3 force_b = Vector3Scale(force_a, -1.0);

  _mass_a.force = Vector3Add(_mass_a.force, force_a);
  _mass_b.force = Vector3Add(_mass_b.force, force_b);
}

auto MassSpringSystem::AddMass() -> void { masses.emplace_back(Vector3Zero()); }

auto MassSpringSystem::AddSpring(int a, int b) -> void {
  Mass &mass_a = masses.at(a);
  Mass &mass_b = masses.at(b);

  Vector3 position = mass_a.position;
  Vector3 offset = Vector3(static_cast<float>(GetRandomValue(-100, 100)),
                           static_cast<float>(GetRandomValue(-100, 100)),
                           static_cast<float>(GetRandomValue(-100, 100)));
  offset = Vector3Scale(Vector3Normalize(offset), REST_LENGTH);

  if (mass_b.position == Vector3Zero()) {
    mass_b.position = Vector3Add(position, offset);
  }

  springs.emplace_back(a, b);
}

auto MassSpringSystem::Clear() -> void {
  masses.clear();
  springs.clear();
  octree.nodes.clear();
}

auto MassSpringSystem::ClearForces() -> void {
#ifdef TRACY
  ZoneScoped;
#endif

  for (auto &mass : masses) {
    mass.ClearForce();
  }
}

auto MassSpringSystem::CalculateSpringForces() -> void {
#ifdef TRACY
  ZoneScoped;
#endif

  for (const auto spring : springs) {
    Mass &a = masses.at(spring.a);
    Mass &b = masses.at(spring.b);
    spring.CalculateSpringForce(a, b);
  }
}

auto MassSpringSystem::SetThreadName(std::size_t idx) -> void {
  BS::this_thread::set_os_thread_name(std::format("bh-worker-{}", idx));
}

auto MassSpringSystem::BuildOctree() -> void {
#ifdef TRACY
  ZoneScoped;
#endif

  octree.nodes.clear();
  octree.nodes.reserve(masses.size() * 2);

  // Compute bounding box around all masses
  Vector3 min = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
  Vector3 max = Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
  for (const auto &mass : masses) {
    min.x = std::min(min.x, mass.position.x);
    max.x = std::max(max.x, mass.position.x);
    min.y = std::min(min.y, mass.position.y);
    max.y = std::max(max.y, mass.position.y);
    min.z = std::min(min.z, mass.position.z);
    max.z = std::max(max.z, mass.position.z);
  }

  // Pad the bounding box
  float pad = 1.0;
  min = Vector3Subtract(min, Vector3Scale(Vector3One(), pad));
  max = Vector3Add(max, Vector3Scale(Vector3One(), pad));

  // Make it cubic (so subdivisions are balanced)
  float max_extent = std::max({max.x - min.x, max.y - min.y, max.z - min.z});
  max = Vector3Add(min, Vector3Scale(Vector3One(), max_extent));

  // Root node spans the entire area
  int root = octree.CreateNode(min, max);

  for (std::size_t i = 0; i < masses.size(); ++i) {
    octree.Insert(root, i, masses[i].position, MASS);
  }
}

auto MassSpringSystem::CalculateRepulsionForces() -> void {
#ifdef TRACY
  ZoneScoped;
#endif

  BuildOctree();

  auto solve_octree = [&](int i) {
    Vector3 force = octree.CalculateForce(0, masses[i].position);
    masses[i].force = Vector3Add(masses[i].force, force);
  };

// Calculate forces using Barnes-Hut
#ifdef WEB
  for (int i = 0; i < mass_pointers.size(); ++i) {
    solve_octree(i);
  }
#else
  BS::multi_future<void> loop_future =
      threads.submit_loop(0, masses.size(), solve_octree, 256);
  loop_future.wait();
#endif
}

auto MassSpringSystem::VerletUpdate(float delta_time) -> void {
#ifdef TRACY
  ZoneScoped;
#endif

  for (auto &mass : masses) {
    mass.VerletUpdate(delta_time);
  }
}

auto ThreadedPhysics::PhysicsThread(ThreadedPhysics::PhysicsState &state)
    -> void {
  BS::this_thread::set_os_thread_name("physics");

  MassSpringSystem mass_springs;

  const auto visitor = overloads{
      [&](const struct AddMass &am) { mass_springs.AddMass(); },
      [&](const struct AddSpring &as) { mass_springs.AddSpring(as.a, as.b); },
      [&](const struct ClearGraph &cg) { mass_springs.Clear(); },
  };

  std::chrono::time_point last = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> accumulator(0);
  std::chrono::duration<double> update_accumulator(0);
  unsigned int updates = 0;

  while (state.running.load()) {
#ifdef TRACY
    FrameMarkStart("PhysicsThread");
#endif

    // Time tracking
    std::chrono::time_point now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> deltatime = now - last;
    accumulator += deltatime;
    update_accumulator += deltatime;
    last = now;

    // Handle queued commands
    {
#ifdef TRACY
      std::lock_guard<LockableBase(std::mutex)> lock(state.command_mtx);
#else
      std::lock_guard<std::mutex> lock(state.command_mtx);
#endif
      while (!state.pending_commands.empty()) {
        Command &cmd = state.pending_commands.front();
        cmd.visit(visitor);
        state.pending_commands.pop();
      }
    }

    if (mass_springs.masses.empty()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      continue;
    }

    // Physics update
    if (accumulator.count() > TIMESTEP) {
      mass_springs.ClearForces();
      mass_springs.CalculateSpringForces();
      mass_springs.CalculateRepulsionForces();
      mass_springs.VerletUpdate(TIMESTEP * SIM_SPEED);

      ++updates;
      accumulator -= std::chrono::duration<double>(TIMESTEP);
    }

    // Publish the positions for the renderer (copy)
#ifdef TRACY
    FrameMarkStart("PhysicsThreadProduceLock");
#endif
    {
#ifdef TRACY
      std::unique_lock<LockableBase(std::mutex)> lock(state.data_mtx);
#else
      std::unique_lock<std::mutex> lock(state.data_mtx);
#endif
      state.data_consumed_cnd.wait(
          lock, [&] { return state.data_consumed || !state.running.load(); });
      if (!state.running.load()) {
        // Running turned false while we were waiting for the condition
        break;
      }

      if (update_accumulator.count() > 1.0) {
        // Update each second
        state.ups = updates;
        updates = 0;
        update_accumulator = std::chrono::duration<double>(0);
      }

      state.masses.clear();
      state.masses.reserve(mass_springs.masses.size());
      for (const auto &mass : mass_springs.masses) {
        state.masses.emplace_back(mass.position);
      }

      state.springs.clear();
      state.springs.reserve(mass_springs.springs.size());
      for (const auto &spring : mass_springs.springs) {
        state.springs.emplace_back(spring.a, spring.b);
      }

      state.data_ready = true;
      state.data_consumed = false;
    }
    // Notify the rendering thread that new data is available
    state.data_ready_cnd.notify_all();
#ifdef TRACY
    FrameMarkEnd("PhysicsThreadProduceLock");

    FrameMarkEnd("PhysicsThread");
#endif
  }
}

auto ThreadedPhysics::AddMassCmd() -> void {
  {
#ifdef TRACY
    std::lock_guard<LockableBase(std::mutex)> lock(state.command_mtx);
#else
    std::lock_guard<std::mutex> lock(state.command_mtx);
#endif
    state.pending_commands.push(AddMass{});
  }
}

auto ThreadedPhysics::AddSpringCmd(std::size_t a, std::size_t b) -> void {
  {
#ifdef TRACY
    std::lock_guard<LockableBase(std::mutex)> lock(state.command_mtx);
#else
    std::lock_guard<std::mutex> lock(state.command_mtx);
#endif
    state.pending_commands.push(AddSpring{a, b});
  }
}

auto ThreadedPhysics::ClearCmd() -> void {
  {
#ifdef TRACY
    std::lock_guard<LockableBase(std::mutex)> lock(state.command_mtx);
#else
    std::lock_guard<std::mutex> lock(state.command_mtx);
#endif
    state.pending_commands.push(ClearGraph{});
  }
}

auto ThreadedPhysics::AddMassSpringsCmd(
    std::size_t num_masses,
    const std::vector<std::pair<std::size_t, std::size_t>> &springs) -> void {
  {
#ifdef TRACY
    std::lock_guard<LockableBase(std::mutex)> lock(state.command_mtx);
#else
    std::lock_guard<std::mutex> lock(state.command_mtx);
#endif
    for (std::size_t i = 0; i < num_masses; ++i) {
      state.pending_commands.push(AddMass{});
    }
    for (const auto &[from, to] : springs) {
      state.pending_commands.push(AddSpring{from, to});
    }
  }
}
