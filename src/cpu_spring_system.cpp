#include "cpu_spring_system.hpp"
#include "config.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstring>

auto cpu_spring_system::clear() -> void
{
    positions.clear();
    previous_positions.clear();
    velocities.clear();
    forces.clear();
    springs.clear();
    spring_color.clear();
    color_offsets.clear();
    colored_springs.clear();
    coloring_dirty = true;
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
    // position = Vector3Scale(Vector3Normalize(position), cfg.rest_length * 2.0);

    positions.emplace_back(Vector3Zero());
    previous_positions.emplace_back(Vector3Zero());
    velocities.emplace_back(Vector3Zero());
    forces.emplace_back(Vector3Zero());
    coloring_dirty = true;
}

auto cpu_spring_system::add_spring(size_t a, size_t b) -> void
{
    // Update masses to be located along a random walk when adding the springs
    const Vector3& mass_a = positions[a];
    const Vector3& mass_b = positions[b];

    Vector3 offset{static_cast<float>(GetRandomValue(-100, 100)),
                   static_cast<float>(GetRandomValue(-100, 100)),
                   static_cast<float>(GetRandomValue(-100, 100))};

    // By spawning the masses close together, we "explode" them naturally, so they cluster faster
    // (also looks cool)
    offset = Vector3Normalize(offset) * cfg.rest_length * cfg.spawn_spread;

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
    coloring_dirty = true;
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
    const Vector3 a_pos = positions[_s.first];
    const Vector3 b_pos = positions[_s.second];
    const Vector3 a_vel = velocities[_s.first];
    const Vector3 b_vel = velocities[_s.second];

    const Vector3 delta_pos = a_pos - b_pos;
    const Vector3 delta_vel = a_vel - b_vel;

    const float sq_len = Vector3DotProduct(delta_pos, delta_pos);
    if (sq_len < 1e-12f) {
        return;
    }

    const float inv_len = 1.0f / sqrt(sq_len);
    const float len = sq_len * inv_len;

    // Tension-only spring: compression is handled positionally by the distance constraint, which
    // is what removes the stiff short-range force balance that used to jitter.
    if (len <= cfg.rest_length) {
        return;
    }

    const float hooke = cfg.spring_k * (len - cfg.rest_length);
    const float dampening = cfg.dampening_k * Vector3DotProduct(delta_vel, delta_pos) * inv_len;

    const Vector3 a_force = Vector3Scale(delta_pos, -(hooke + dampening) * inv_len);
    const Vector3 b_force = a_force * -1.0f;

    forces[_s.first] += a_force;
    forces[_s.second] += b_force;
}

auto cpu_spring_system::calculate_spring_forces(const threadpool thread_pool) -> void
{
#ifdef TRACY
    ZoneScoped;
#endif

    const auto solve_spring_force = [&](const int i) { calculate_spring_force(i); };

    if (thread_pool) {
        (*thread_pool)
            ->submit_loop(0, springs.size(), solve_spring_force, cfg.small_task_block_size)
            .wait();
    } else {
        for (size_t i = 0; i < springs.size(); ++i) {
            solve_spring_force(i);
        }
    }
}

auto cpu_spring_system::calculate_repulsion_forces(const threadpool thread_pool) -> void
{
#ifdef TRACY
    ZoneScoped;
#endif

    // Walk the queries in the tree's Morton order. The tree was built from the same mass set,
    // so `order` is a permutation of the mass ids; processing it in this order keeps both the
    // tree traversal and the force accumulation spatially coherent in the cache.
    const std::vector<u32>& order = tree.morton_order();
    const bool use_morton_order = order.size() == positions.size();
    const size_t count = positions.size();

    // Use the positions the tree was built from, not the current ones. Mixing current query
    // positions with stale mass centers produces an inconsistent force field that jitters.
    const std::vector<Vector3>& query_positions = tree.positions_snapshot();
    const bool use_snapshot = query_positions.size() == positions.size();

    const auto solve_octree = [&](const int i)
    {
        const u32 mass = use_morton_order ? order[i] : static_cast<u32>(i);
        const Vector3& query = use_snapshot ? query_positions[mass] : positions[mass];
        const Vector3 force =
            tree.calculate_force_morton(tree.node_root_index(), query, static_cast<int>(mass));

        forces[mass] += force;
    };

    // Calculate forces using Barnes-Hut
    if (thread_pool) {
        (*thread_pool)->submit_loop(0, count, solve_octree, cfg.large_task_block_size).wait();
    } else {
        for (size_t i = 0; i < count; ++i) {
            solve_octree(i);
        }
    }
}

auto cpu_spring_system::verlet_update(const size_t m, const float dt) -> void
{
    const Vector3 acc = (forces[m] / cfg.mass) * dt * dt;
    const Vector3 pos = positions[m];

    // Velocity at the *start* of this step. The spring damping pass reads this array on the next
    // step, so it must not include this step's acceleration (that feedback is unstable).
    velocities[m] = (pos - previous_positions[m]) / dt;

    Vector3 delta_pos = pos - previous_positions[m];
    delta_pos *= 1.0 - cfg.verlet_dampening; // Minimal dampening

    Vector3 step = delta_pos + acc;

    // Safety net: never let a bad force launch a mass out of the scene.
    const float step_sq = Vector3LengthSqr(step);
    if (step_sq > cfg.max_step * cfg.max_step) {
        step *= cfg.max_step / std::sqrt(step_sq);
    }

    positions[m] += step;
    previous_positions[m] = pos;
}

auto cpu_spring_system::rebuild_coloring() -> void
{
    // Greedy edge coloring: a spring gets a color that is not used by any other spring incident
    // to either of its endpoints. The graph degree is bounded by the number of legal puzzle moves
    // (<= 60), so 256 colors are far more than the <= 2*degree-1 needed.
    std::vector<std::array<u64, 4>> masks(positions.size());

    spring_color.resize(springs.size());
    max_spring_color = 0;

    for (size_t i = 0; i < springs.size(); ++i) {
        const auto [a, b] = springs[i];
        auto& ma = masks[a];
        auto& mb = masks[b];

        u32 color = 0;
        bool found = false;
        for (int word = 0; word < 4; ++word) {
            const u64 free_bits = ~(ma[word] | mb[word]);
            if (free_bits != 0) {
                color = static_cast<u32>(word * 64 + std::countr_zero(free_bits));
                found = true;
                break;
            }
        }

        // A puzzle state has at most 4 * 15 moves, so greedy edge coloring needs fewer than
        // 256 colors. This would only trip if the graph representation changed.
        assert(found);
        (void)found;
        spring_color[i] = color;
        ma[color >> 6] |= 1ull << (color & 63);
        mb[color >> 6] |= 1ull << (color & 63);
        max_spring_color = std::max(max_spring_color, color);
    }

    color_offsets.assign(static_cast<size_t>(max_spring_color) + 2, 0);
    for (const u32 color : spring_color) {
        ++color_offsets[color + 1];
    }
    for (size_t c = 1; c < color_offsets.size(); ++c) {
        color_offsets[c] += color_offsets[c - 1];
    }

    colored_springs.resize(springs.size());
    std::vector<u32> cursor = color_offsets;
    for (size_t i = 0; i < springs.size(); ++i) {
        colored_springs[cursor[spring_color[i]]++] = static_cast<u32>(i);
    }

    coloring_dirty = false;
}

auto cpu_spring_system::solve_distance_constraints(const int iterations,
                                                   const threadpool thread_pool) -> void
{
#ifdef TRACY
    ZoneScoped;
#endif

    if (coloring_dirty) {
        rebuild_coloring();
    }

    // Rigid links: repulsion is the only force, so the springs no longer participate in a stiff
    // force balance that can jitter. Instead, after integration we project every linked pair
    // back to cfg.rest_length (position-based dynamics). Colors are processed one after another;
    // within a color no two springs share a mass, so the projection is race-free in parallel.
    const auto solve_spring = [&](const u32 spring_index)
    {
        const auto [a, b] = springs[spring_index];
        Vector3 delta = positions[a] - positions[b];
        const float len_sq = Vector3DotProduct(delta, delta);
        if (len_sq < 1e-12f) {
            return;
        }

        const float len = std::sqrt(len_sq);

        // Hard minimum length only: stretched links are left to the tension-only spring force,
        // so the graph can still expand under repulsion.
        if (len >= cfg.rest_length) {
            return;
        }

        const float correction = (len - cfg.rest_length) / len * 0.5f * cfg.constraint_relaxation;

        positions[a] -= delta * correction;
        positions[b] += delta * correction;
    };

    for (int iteration = 0; iteration < iterations; ++iteration) {
        for (u32 color = 0; color <= max_spring_color; ++color) {
            const u32 begin = color_offsets[color];
            const u32 end = color_offsets[color + 1];
            if (begin == end) {
                continue;
            }

            if (thread_pool && (end - begin) >= 256) {
                (*thread_pool)
                    ->submit_loop(
                        static_cast<int>(begin), static_cast<int>(end),
                        [&](const int i) { solve_spring(colored_springs[i]); }, 64)
                    .wait();
            } else {
                for (u32 i = begin; i < end; ++i) {
                    solve_spring(colored_springs[i]);
                }
            }
        }
    }
}

auto cpu_spring_system::update(const float dt, const threadpool thread_pool) -> void
{
#ifdef TRACY
    ZoneScoped;
#endif

    const auto update = [&](const int i) { verlet_update(i, dt); };

    if (thread_pool) {
        (*thread_pool)->submit_loop(0, positions.size(), update, cfg.small_task_block_size).wait();
    } else {
        for (size_t i = 0; i < positions.size(); ++i) {
            update(i);
        }
    }
}
