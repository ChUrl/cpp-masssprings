#include "cpu_spring_system.hpp"
#include "config.hpp"

#include <cfloat>
#include <cstring>

auto cpu_spring_system::clear() -> void
{
    positions.clear();
    previous_positions.clear();
    velocities.clear();
    forces.clear();
    springs.clear();
    tree.clear();
}

auto cpu_spring_system::add_mass() -> void
{
    // Adding all positions to (0, 0, 0) breaks the octree

    // Done when adding springs
    // Vector3 position{
    //   static_cast<float>(GetRandomValue(-100, 100)), static_cast<float>(GetRandomValue(-100,
    //   100)), static_cast<float>(GetRandomValue(-100, 100))
    // };
    // position = Vector3Scale(Vector3Normalize(position), REST_LENGTH * 2.0);

    positions.emplace_back(Vector3Zero());
    previous_positions.emplace_back(Vector3Zero());
    velocities.emplace_back(Vector3Zero());
    forces.emplace_back(Vector3Zero());
}

auto cpu_spring_system::add_spring(size_t a, size_t b) -> void
{
    // Update masses to be located along a random walk when adding the springs
    const Vector3& mass_a = positions[a];
    const Vector3& mass_b = positions[b];

    Vector3 offset{static_cast<float>(GetRandomValue(-100, 100)),
        static_cast<float>(GetRandomValue(-100, 100)),
        static_cast<float>(GetRandomValue(-100, 100))};

    // By spawning the masses close together, we "explode" them naturally, so they cluster faster (also looks cool)
    offset = Vector3Normalize(offset) * REST_LENGTH * 0.1;

    // If the offset moves the mass closer to the current center of mass, flip it
    if (!tree.empty()) {
        const Vector3 mass_center_direction =
            Vector3Subtract(positions[a], tree.root().mass_center);
        const float mass_center_distance = Vector3Length(mass_center_direction);

        if (mass_center_distance > 0 && Vector3DotProduct(offset, mass_center_direction) < 0.0f) {
            offset = Vector3Negate(offset);
        }
    }

    positions[b] = mass_a + offset;
    previous_positions[b] = mass_b;

    // infoln("Adding spring: ({}, {}, {})->({}, {}, {})", mass_a.position.x, mass_a.position.y,
    // mass_a.position.z,
    //        mass_b.position.x, mass_b.position.y, mass_b.position.z);

    springs.emplace_back(a, b);
}

auto cpu_spring_system::clear_forces() -> void
{
    #ifdef TRACY
    ZoneScoped;
    #endif

    memset(forces.data(), 0, forces.size() * sizeof(Vector3));
}

auto cpu_spring_system::calculate_spring_force(const size_t s) -> void
{
    const spring _s = springs[s];
    const Vector3 a_pos = positions[_s.a];
    const Vector3 b_pos = positions[_s.b];
    const Vector3 a_vel = velocities[_s.a];
    const Vector3 b_vel = velocities[_s.b];

    const Vector3 delta_pos = a_pos - b_pos;
    const Vector3 delta_vel = a_vel - b_vel;

    const float sq_len = Vector3DotProduct(delta_pos, delta_pos);
    const float inv_len = 1.0f / sqrt(sq_len);
    const float len = sq_len * inv_len;

    const float hooke = SPRING_K * (len - REST_LENGTH);
    const float dampening = DAMPENING_K * Vector3DotProduct(delta_vel, delta_pos) * inv_len;

    const Vector3 a_force = Vector3Scale(delta_pos, -(hooke + dampening) * inv_len);
    const Vector3 b_force = a_force * -1.0f;

    forces[_s.a] += a_force;
    forces[_s.b] += b_force;
}

auto cpu_spring_system::calculate_spring_forces(
    const std::optional<BS::thread_pool<>* const> thread_pool) -> void
{
    #ifdef TRACY
    ZoneScoped;
    #endif

    const auto solve_spring_force = [&](const int i) { calculate_spring_force(i); };

    if (thread_pool) {
        (*thread_pool)
            ->submit_loop(0, springs.size(), solve_spring_force, SMALL_TASK_BLOCK_SIZE)
            .wait();
    } else {
        for (size_t i = 0; i < springs.size(); ++i) {
            solve_spring_force(i);
        }
    }
}

auto cpu_spring_system::calculate_repulsion_forces(
    const std::optional<BS::thread_pool<>* const> thread_pool) -> void
{
    #ifdef TRACY
    ZoneScoped;
    #endif

    const auto solve_octree = [&](const int i)
    {
        const Vector3 force = tree.calculate_force_morton(0, positions[i], i);
        forces[i] += force;
    };

    // Calculate forces using Barnes-Hut
    if (thread_pool) {
        (*thread_pool)
            ->submit_loop(0, positions.size(), solve_octree, LARGE_TASK_BLOCK_SIZE)
            .wait();
    } else {
        for (size_t i = 0; i < positions.size(); ++i) {
            solve_octree(i);
        }
    }
}

auto cpu_spring_system::integrate_velocity(const size_t m, const float dt) -> void
{
    const Vector3 acc = forces[m] / MASS;
    velocities[m] += acc * dt;
}

auto cpu_spring_system::integrate_position(const size_t m, const float dt) -> void
{
    previous_positions[m] = positions[m];
    positions[m] += velocities[m] * dt;
}

auto cpu_spring_system::verlet_update(const size_t m, const float dt) -> void
{
    const Vector3 acc = (forces[m] / MASS) * dt * dt;
    const Vector3 pos = positions[m];

    Vector3 delta_pos = pos - previous_positions[m];
    delta_pos *= 1.0 - VERLET_DAMPENING; // Minimal dampening

    positions[m] += delta_pos + acc;
    previous_positions[m] = pos;
}

auto cpu_spring_system::update(const float dt,
                                const std::optional<BS::thread_pool<>* const> thread_pool) -> void
{
    #ifdef TRACY
    ZoneScoped;
    #endif

    const auto update = [&](const int i) { verlet_update(i, dt); };

    if (thread_pool) {
        (*thread_pool)->submit_loop(0, positions.size(), update, SMALL_TASK_BLOCK_SIZE).wait();
    } else {
        for (size_t i = 0; i < positions.size(); ++i) {
            update(i);
        }
    }
}

auto cpu_spring_system::center_masses(const std::optional<BS::thread_pool<>* const> thread_pool)
    -> void
{
    Vector3 mean = Vector3Zero();
    for (const Vector3& pos : positions) {
        mean += pos;
    }
    mean /= static_cast<float>(positions.size());

    const auto center_mass = [&](const int i) { positions[i] -= mean; };

    if (thread_pool) {
        (*thread_pool)->submit_loop(0, positions.size(), center_mass, SMALL_TASK_BLOCK_SIZE).wait();
    } else {
        for (size_t i = 0; i < positions.size(); ++i) {
            center_mass(i);
        }
    }
}