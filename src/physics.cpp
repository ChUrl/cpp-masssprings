#include "physics.hpp"
#include "config.hpp"

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <raylib.h>
#include <raymath.h>
#include <utility>
#include <vector>

#ifdef TRACY
    #include <tracy/Tracy.hpp>
#endif

auto mass::clear_force() -> void
{
    force = Vector3Zero();
}

auto mass::calculate_velocity(const float delta_time) -> void
{
    const Vector3 acceleration = Vector3Scale(force, 1.0 / MASS);
    const Vector3 temp = Vector3Scale(acceleration, delta_time);
    velocity = Vector3Add(velocity, temp);
}

auto mass::calculate_position(const float delta_time) -> void
{
    previous_position = position;

    const Vector3 temp = Vector3Scale(velocity, delta_time);
    position = Vector3Add(position, temp);
}

auto mass::verlet_update(const float delta_time) -> void
{
    const Vector3 acceleration = Vector3Scale(force, 1.0 / MASS);
    const Vector3 temp_position = position;

    Vector3 displacement = Vector3Subtract(position, previous_position);
    const Vector3 accel_term = Vector3Scale(acceleration, delta_time * delta_time);

    // Minimal dampening
    displacement = Vector3Scale(displacement, 1.0 - VERLET_DAMPENING);

    position = Vector3Add(Vector3Add(position, displacement), accel_term);
    previous_position = temp_position;
}

auto spring::calculate_spring_force(mass& _a, mass& _b) -> void
{
    // TODO: Use a bungee force here instead of springs, since we already have global repulsion?
    const Vector3 delta_position = Vector3Subtract(_a.position, _b.position);
    const float current_length = Vector3Length(delta_position);
    const float inv_current_length = 1.0f / current_length;
    const Vector3 delta_velocity = Vector3Subtract(_a.velocity, _b.velocity);

    const float hooke = SPRING_CONSTANT * (current_length - REST_LENGTH);
    const float dampening =
        DAMPENING_CONSTANT * Vector3DotProduct(delta_velocity, delta_position) * inv_current_length;

    const Vector3 force_a = Vector3Scale(delta_position, -(hooke + dampening) * inv_current_length);
    const Vector3 force_b = Vector3Scale(force_a, -1.0);

    _a.force = Vector3Add(_a.force, force_a);
    _b.force = Vector3Add(_b.force, force_b);
}

auto mass_spring_system::add_mass() -> void
{
    // Adding all positions to (0, 0, 0) breaks the octree

    // Done when adding springs
    // Vector3 position{
    //   static_cast<float>(GetRandomValue(-100, 100)), static_cast<float>(GetRandomValue(-100,
    //   100)), static_cast<float>(GetRandomValue(-100, 100))
    // };
    // position = Vector3Scale(Vector3Normalize(position), REST_LENGTH * 2.0);

    masses.emplace_back(Vector3Zero());
}

auto mass_spring_system::add_spring(size_t a, size_t b) -> void
{
    // Update masses to be located along a random walk when adding the springs
    const mass& mass_a = masses.at(a);
    mass& mass_b = masses.at(b);

    Vector3 offset{static_cast<float>(GetRandomValue(-100, 100)),
                   static_cast<float>(GetRandomValue(-100, 100)),
                   static_cast<float>(GetRandomValue(-100, 100))};
    offset = Vector3Normalize(offset) * REST_LENGTH;

    // If the offset moves the mass closer to the current center of mass, flip it
    if (!tree.nodes.empty()) {
        const Vector3 mass_center_direction =
            Vector3Subtract(mass_a.position, tree.nodes.at(0).mass_center);
        const float mass_center_distance = Vector3Length(mass_center_direction);

        if (mass_center_distance > 0 && Vector3DotProduct(offset, mass_center_direction) < 0.0f) {
            offset = Vector3Negate(offset);
        }
    }

    mass_b.position = mass_a.position + offset;
    mass_b.previous_position = mass_b.position;

    // infoln("Adding spring: ({}, {}, {})->({}, {}, {})", mass_a.position.x, mass_a.position.y,
    // mass_a.position.z,
    //        mass_b.position.x, mass_b.position.y, mass_b.position.z);

    springs.emplace_back(a, b);
}

auto mass_spring_system::clear() -> void
{
    masses.clear();
    springs.clear();
    tree.nodes.clear();
}

auto mass_spring_system::clear_forces() -> void
{
#ifdef TRACY
    ZoneScoped;
#endif

    for (auto& mass : masses) {
        mass.clear_force();
    }
}

auto mass_spring_system::calculate_spring_forces() -> void
{
#ifdef TRACY
    ZoneScoped;
#endif

    for (const auto s : springs) {
        mass& a = masses.at(s.a);
        mass& b = masses.at(s.b);
        spring::calculate_spring_force(a, b);
    }
}

#ifdef THREADPOOL
auto mass_spring_system::set_thread_name(size_t idx) -> void
{
    BS::this_thread::set_os_thread_name(std::format("bh-worker-{}", idx));
}
#endif

auto mass_spring_system::build_octree() -> void
{
#ifdef TRACY
    ZoneScoped;
#endif

    tree.nodes.clear();
    tree.nodes.reserve(masses.size() * 2);

    // Compute bounding box around all masses
    Vector3 min{FLT_MAX, FLT_MAX, FLT_MAX};
    Vector3 max{-FLT_MAX, -FLT_MAX, -FLT_MAX};
    for (const auto& mass : masses) {
        min.x = std::min(min.x, mass.position.x);
        max.x = std::max(max.x, mass.position.x);
        min.y = std::min(min.y, mass.position.y);
        max.y = std::max(max.y, mass.position.y);
        min.z = std::min(min.z, mass.position.z);
        max.z = std::max(max.z, mass.position.z);
    }

    // Pad the bounding box
    constexpr float pad = 1.0;
    min = Vector3Subtract(min, Vector3Scale(Vector3One(), pad));
    max = Vector3Add(max, Vector3Scale(Vector3One(), pad));

    // Make it cubic (so subdivisions are balanced)
    const float max_extent = std::max({max.x - min.x, max.y - min.y, max.z - min.z});
    max = Vector3Add(min, Vector3Scale(Vector3One(), max_extent));

    // Root node spans the entire area
    const int root = tree.create_empty_leaf(min, max);

    for (size_t i = 0; i < masses.size(); ++i) {
        tree.insert(root, static_cast<int>(i), masses[i].position, MASS, 0);
    }
}

auto mass_spring_system::calculate_repulsion_forces() -> void
{
#ifdef TRACY
    ZoneScoped;
#endif

    build_octree();

    auto solve_octree = [&](const int i)
    {
        const Vector3 force = tree.calculate_force(0, masses[i].position);
        masses[i].force = Vector3Add(masses[i].force, force);
    };

// Calculate forces using Barnes-Hut
#ifdef THREADPOOL
    const BS::multi_future<void> loop_future =
        threads.submit_loop(0, masses.size(), solve_octree, 256);
    loop_future.wait();
#else
    for (size_t i = 0; i < masses.size(); ++i) {
        solve_octree(i);
    }
#endif
}

auto mass_spring_system::verlet_update(const float delta_time) -> void
{
#ifdef TRACY
    ZoneScoped;
#endif

    for (auto& mass : masses) {
        mass.verlet_update(delta_time);
    }
}

auto mass_spring_system::center_masses() -> void
{
    Vector3 mean = Vector3Zero();
    for (const auto& mass : masses) {
        mean += mass.position;
    }
    mean /= static_cast<float>(masses.size());

    for (auto& mass : masses) {
        mass.position -= mean;
    }
}

auto threaded_physics::physics_thread(physics_state& state) -> void
{
#ifdef THREADPOOL
    BS::this_thread::set_os_thread_name("physics");
#endif

    mass_spring_system mass_springs;

    const auto visitor = overloads{
        [&](const struct add_mass& am) { mass_springs.add_mass(); },
        [&](const struct add_spring& as) { mass_springs.add_spring(as.a, as.b); },
        [&](const struct clear_graph& cg) { mass_springs.clear(); },
    };

    std::chrono::time_point last = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> physics_accumulator(0);
    std::chrono::duration<double> ups_accumulator(0);
    int loop_iterations = 0;

    while (state.running.load()) {
#ifdef TRACY
        FrameMarkStart("PhysicsThread");
#endif

        // Time tracking
        std::chrono::time_point now = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<double> deltatime = now - last;
        physics_accumulator += deltatime;
        ups_accumulator += deltatime;
        last = now;

        // Handle queued commands
        {
#ifdef TRACY
            std::lock_guard<LockableBase(std::mutex)> lock(state.command_mtx);
#else
            std::lock_guard<std::mutex> lock(state.command_mtx);
#endif
            while (!state.pending_commands.empty()) {
                command& cmd = state.pending_commands.front();
                cmd.visit(visitor);
                state.pending_commands.pop();
            }
        }

        if (mass_springs.masses.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        // Physics update
        if (physics_accumulator.count() > TIMESTEP) {
            mass_springs.clear_forces();
            mass_springs.calculate_spring_forces();
            mass_springs.calculate_repulsion_forces();
            mass_springs.verlet_update(TIMESTEP * SIM_SPEED);

            // This is only helpful if we're drawing a grid at (0, 0, 0). Otherwise, it's just
            // expensive and yields no benefit since we can lock the camera to the center of mass
            // cheaply. mass_springs.center_masses();

            ++loop_iterations;
            physics_accumulator -= std::chrono::duration<double>(TIMESTEP);
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
            state.data_consumed_cnd.wait(lock, [&]
                                         { return state.data_consumed || !state.running.load(); });
            if (!state.running.load()) {
                // Running turned false while we were waiting for the condition
                break;
            }

            if (ups_accumulator.count() > 1.0) {
                // Update each second
                state.ups = loop_iterations;
                loop_iterations = 0;
                ups_accumulator = std::chrono::duration<double>(0);
            }
            if (mass_springs.tree.nodes.empty()) {
                state.mass_center = Vector3Zero();
            } else {
                state.mass_center = mass_springs.tree.nodes.at(0).mass_center;
            }

            state.masses.clear();
            state.masses.reserve(mass_springs.masses.size());
            for (const auto& mass : mass_springs.masses) {
                state.masses.emplace_back(mass.position);
            }

            state.mass_count = mass_springs.masses.size();
            state.spring_count = mass_springs.springs.size();

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

auto threaded_physics::add_mass_cmd() -> void
{
    {
#ifdef TRACY
        std::lock_guard<LockableBase(std::mutex)> lock(state.command_mtx);
#else
        std::lock_guard<std::mutex> lock(state.command_mtx);
#endif
        state.pending_commands.emplace(add_mass{});
    }
}

auto threaded_physics::add_spring_cmd(const size_t a, const size_t b) -> void
{
    {
#ifdef TRACY
        std::lock_guard<LockableBase(std::mutex)> lock(state.command_mtx);
#else
        std::lock_guard<std::mutex> lock(state.command_mtx);
#endif
        state.pending_commands.emplace(add_spring{a, b});
    }
}

auto threaded_physics::clear_cmd() -> void
{
    {
#ifdef TRACY
        std::lock_guard<LockableBase(std::mutex)> lock(state.command_mtx);
#else
        std::lock_guard<std::mutex> lock(state.command_mtx);
#endif
        state.pending_commands.emplace(clear_graph{});
    }
}

auto threaded_physics::add_mass_springs_cmd(const size_t num_masses,
                                            const std::vector<std::pair<size_t, size_t>>& springs)
    -> void
{
    {
#ifdef TRACY
        std::lock_guard<LockableBase(std::mutex)> lock(state.command_mtx);
#else
        std::lock_guard<std::mutex> lock(state.command_mtx);
#endif
        for (size_t i = 0; i < num_masses; ++i) {
            state.pending_commands.emplace(add_mass{});
        }
        for (const auto& [from, to] : springs) {
            state.pending_commands.emplace(add_spring{from, to});
        }
    }
}
