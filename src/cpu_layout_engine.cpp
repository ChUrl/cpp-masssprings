#include "cpu_layout_engine.hpp"
#include "config.hpp"
#include "cpu_spring_system.hpp"
#include "util.hpp"

#include <chrono>
#include <ctime>
#include <raylib.h>
#include <raymath.h>
#include <utility>
#include <vector>

#ifdef ASYNC_OCTREE
auto cpu_layout_engine::set_octree_pool_thread_name(size_t idx) -> void
{
    BS::this_thread::set_os_thread_name(std::format("octree-{}", idx));
    // traceln("Using thread \"{}\"", BS::this_thread::get_os_thread_name().value_or("INVALID
    // NAME"));
}
#endif

auto cpu_layout_engine::physics_thread(physics_state& state, const threadpool thread_pool) -> void
{
    cpu_spring_system mass_springs;

#ifdef ASYNC_OCTREE
    BS::this_thread::set_os_thread_name("physics");

    BS::thread_pool<> octree_thread(1, set_octree_pool_thread_name);
    std::future<void> octree_future;
    octree tree_buffer;
    size_t last_mass_count = 0;
#endif

    const auto visitor = overloads{
        [&](const struct add_mass&) { mass_springs.add_mass(); },
        [&](const struct add_spring& as) { mass_springs.add_spring(as.a, as.b); },
        [&](const struct clear_graph&) { mass_springs.clear(); },
    };

    std::chrono::time_point last = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> physics_accumulator(0);
    std::chrono::duration<double> ups_accumulator(0);
    int loop_iterations = 0;

    const auto elapsed_ms = [](const std::chrono::high_resolution_clock::time_point start)
    {
        return std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() -
                                                         start)
            .count();
    };

    size_t step_index = 0;
    float build_ms = 0.0f;
    float tree_ms = 0.0f;
    float tree_cpu_ms = 0.0f;
    float wait_ms = 0.0f;
    float spring_ms = 0.0f;
    float repulsion_ms = 0.0f;
    float integrate_ms = 0.0f;

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

        if (mass_springs.positions.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        // Physics update
        bool stepped = false;
        const auto step_start = std::chrono::high_resolution_clock::now();
        if (physics_accumulator.count() > TIMESTEP) {
            stepped = true;
            auto stage_start = std::chrono::high_resolution_clock::now();

#ifdef ASYNC_OCTREE
            const size_t position_count = mass_springs.positions.size();
            const bool size_changed = last_mass_count != position_count;
            ++step_index;

            if (size_changed) {
                // The tree has to match the mass set for this very step, so build synchronously.
                traceln("Rebuilding octree synchronously because graph size changed");
                const auto sync_start = std::chrono::high_resolution_clock::now();
                octree::build_octree_morton(mass_springs.tree, mass_springs.positions,
                                            std::nullopt);
                last_mass_count = position_count;
                tree_ms = static_cast<float>(elapsed_ms(sync_start));
                tree_cpu_ms = tree_ms;
            } else if (step_index % OCTREE_REBUILD_INTERVAL == 0) {
                // Snapshot the positions so mass_springs is not mutating the vector while the
                // octree is building.
                std::vector<Vector3> positions = mass_springs.positions;

                // The build is the step limiter, so let its Morton-code pass use the shared pool
                // too. The repulsion pass has enough slack to absorb the brief contention.
                octree_future = octree_thread.submit_task(
                    [&tree_buffer, &tree_ms, &tree_cpu_ms, &thread_pool,
                     positions = std::move(positions)]()
                    {
                        const auto tree_start = std::chrono::high_resolution_clock::now();
                        timespec cpu_start{};
                        timespec cpu_end{};
                        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpu_start);

                        octree::build_octree_morton(tree_buffer, positions, thread_pool);

                        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpu_end);
                        tree_ms = static_cast<float>(
                            std::chrono::duration<double, std::milli>(
                                std::chrono::high_resolution_clock::now() - tree_start)
                                .count());
                        tree_cpu_ms =
                            static_cast<float>(cpu_end.tv_sec - cpu_start.tv_sec) * 1000.0f +
                            static_cast<float>(cpu_end.tv_nsec - cpu_start.tv_nsec) / 1.0e6f;
                    });
            }
#else
            const auto sync_start = std::chrono::high_resolution_clock::now();
            octree::build_octree_morton(mass_springs.tree, mass_springs.positions, thread_pool);
            tree_ms = static_cast<float>(elapsed_ms(sync_start));
            tree_cpu_ms = tree_ms;
#endif

            build_ms = 0.9f * build_ms + 0.1f * elapsed_ms(stage_start);
            stage_start = std::chrono::high_resolution_clock::now();

            mass_springs.clear_forces();
            mass_springs.calculate_spring_forces(thread_pool);
            mass_springs.calculate_repulsion_forces(thread_pool);
            repulsion_ms = 0.9f * repulsion_ms + 0.1f * elapsed_ms(stage_start);
            stage_start = std::chrono::high_resolution_clock::now();

            mass_springs.update(TIMESTEP * SIM_SPEED, thread_pool);
            integrate_ms = 0.9f * integrate_ms + 0.1f * elapsed_ms(stage_start);
            stage_start = std::chrono::high_resolution_clock::now();

            // Links have a hard minimum length: project compressed links back to REST_LENGTH
            // after the repulsion integration (every CONSTRAINT_INTERVAL steps).
            if (step_index % CONSTRAINT_INTERVAL == 0) {
                mass_springs.solve_distance_constraints(CONSTRAINT_ITERATIONS, thread_pool);
            }
            spring_ms = 0.9f * spring_ms + 0.1f * elapsed_ms(stage_start);

            ++loop_iterations;
            physics_accumulator -= std::chrono::duration<double>(TIMESTEP);
        }

#ifdef ASYNC_OCTREE
        // Wait for the octree to be built
        if (octree_future.valid()) {
            const auto wait_start = std::chrono::high_resolution_clock::now();
            octree_future.wait();
            octree_future = std::future<void>{};
            std::swap(mass_springs.tree, tree_buffer);
            wait_ms = 0.9f * wait_ms + 0.1f * elapsed_ms(wait_start);
        }
#endif

        if (stepped) {
            const std::chrono::duration<double, std::milli> elapsed =
                std::chrono::high_resolution_clock::now() - step_start;
            state.step_ms = 0.9f * state.step_ms + 0.1f * static_cast<float>(elapsed.count());
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
            if (mass_springs.tree.empty()) {
                state.mass_center = Vector3Zero();
            } else {
                state.mass_center = mass_springs.tree.root().mass_center;
            }

            state.masses.clear();
            state.masses.reserve(mass_springs.positions.size());
            for (const Vector3& pos : mass_springs.positions) {
                state.masses.emplace_back(pos);
            }

            state.mass_count = mass_springs.positions.size();
            state.spring_count = mass_springs.springs.size();
            state.build_ms = build_ms;
            state.tree_ms = tree_ms;
            state.tree_cpu_ms = tree_cpu_ms;
            state.wait_ms = wait_ms;
            state.tree_nodes = mass_springs.tree.node_count();
            state.tree_children = mass_springs.tree.child_count();
            state.tree_stages = mass_springs.tree.stage_ms;
            state.spring_ms = spring_ms;
            state.repulsion_ms = repulsion_ms;
            state.integrate_ms = integrate_ms;

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

auto cpu_layout_engine::clear_cmd() -> void
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

auto cpu_layout_engine::add_mass_cmd() -> void
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

auto cpu_layout_engine::add_spring_cmd(const size_t a, const size_t b) -> void
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

auto cpu_layout_engine::add_mass_springs_cmd(const size_t num_masses,
                                             const std::vector<spring>& springs) -> void
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
