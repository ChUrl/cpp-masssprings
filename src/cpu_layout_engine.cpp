#include "cpu_layout_engine.hpp"
#include "config.hpp"
#include "cpu_spring_system.hpp"
#include "gpu_context.hpp"
#include "gpu_physics.hpp"
#include "octree.hpp"
#include "settings.hpp"
#include "util.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <raylib.h>
#include <raymath.h>
#include <utility>
#include <vector>

auto cpu_layout_engine::set_octree_pool_thread_name(size_t idx) -> void
{
    BS::this_thread::set_os_thread_name(std::format("octree-{}", idx));
}

auto cpu_layout_engine::physics_thread(physics_state& state, const threadpool thread_pool,
                                       gpu_context* gpu, gpu_context* tree_gpu) -> void
{
    cpu_spring_system mass_springs;

    BS::this_thread::set_os_thread_name("physics");

    bool gpu_ready = false;
    gpu_physics gpu_engine;
    if (gpu != nullptr && gpu->ok()) {
        gpu_ready = gpu->make_current();
        if (gpu_ready) {
            infoln("GPU compute context active on physics thread: {}",
                   gpu::runtime_status().version);
            if (!gpu_engine.init("shader/compute",
                                 tree_gpu != nullptr && tree_gpu->ok() ? tree_gpu : nullptr)) {
                warnln("Could not initialize GPU repulsion: {}", gpu_engine.error_message());
                gpu_ready = false;
            } else {
                infoln("GPU physics compute shaders ready.");
            }
        } else {
            warnln("Could not activate GPU compute context: {}", gpu::runtime_status().error);
        }
    } else {
        infoln("No GPU compute context available; using the CPU pipeline.");
    }

    if (!gpu_ready && get_settings().gpu_physics) {
        simulation_settings settings = get_settings();
        settings.gpu_physics = false;
        set_settings(settings);
    }

    BS::thread_pool<> octree_thread(1, set_octree_pool_thread_name);
    std::future<void> octree_future;
    octree tree_buffer;
    size_t last_mass_count = 0;

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
    float gpu_step_ms = 0.0f;
    float integrate_ms = 0.0f;

    bool last_gpu_selected = false;
    bool gpu_was_active = false;
    bool using_gpu_tree = false;

    // The z coordinates of the 3D layout, preserved while the planar mode is active.
    bool last_planar = get_settings().planar_graph;
    std::vector<float> planar_saved_z;
    std::uint64_t planar_saved_topology = 0;

    const auto publish_tracked_memory = [&]
    {
        state.cpu_physics_bytes.store(mass_springs.memory_bytes(), std::memory_order_relaxed);
        state.octree_bytes.store(mass_springs.tree.memory_bytes() + tree_buffer.memory_bytes(),
                                 std::memory_order_relaxed);
        state.gpu_physics_bytes.store(gpu_engine.memory_bytes(), std::memory_order_relaxed);
        state.gpu_octree_bytes.store(gpu_engine.octree_memory_bytes(), std::memory_order_relaxed);
    };

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

        // Snapshot the runtime settings once per iteration so all stages see a consistent set.
        const simulation_settings cfg = get_settings();
        mass_springs.cfg = cfg;

        const bool gpu_selected = gpu_ready && cfg.gpu_physics;
        const bool gpu_octree_selected = gpu_selected && cfg.gpu_tree_sort;
        gpu::set_active(gpu_selected);

        if (gpu_selected != last_gpu_selected) {
            infoln("Physics backend: {}.", gpu_selected ? "GPU" : "CPU");
            last_gpu_selected = gpu_selected;
        }

        if (gpu_selected && !gpu_was_active) {
            gpu_engine.mark_state_dirty();
        } else if (!gpu_selected && gpu_was_active) {
            gpu_engine.sync_to_cpu(mass_springs);
            if (mass_springs.tree.empty() && !mass_springs.positions.empty()) {
                octree::build_octree_morton(mass_springs.tree, mass_springs.positions, std::nullopt,
                                            cfg);
            }
        }
        gpu_was_active = gpu_selected;

        // Switching between the 3D and planar layout. The planar mode flattens the current z
        // coordinates; the previous z values are restored when switching back to 3D.
        if (cfg.planar_graph != last_planar) {
            // Drop an in-flight tree build: it used the old z coordinates and must not overwrite
            // the tree rebuilt from the flattened positions below.
            if (octree_future.valid()) {
                octree_future.wait();
                octree_future = std::future<void>{};
            }
            tree_buffer.clear();

            if (cfg.planar_graph) {
                planar_saved_topology = mass_springs.topology_version;
                planar_saved_z.resize(mass_springs.positions.size());
                for (size_t i = 0; i < mass_springs.positions.size(); ++i) {
                    planar_saved_z[i] = mass_springs.positions[i].z;
                    mass_springs.positions[i].z = 0.0f;
                }
                for (Vector3& position : mass_springs.previous_positions) {
                    position.z = 0.0f;
                }
                for (Vector3& velocity : mass_springs.velocities) {
                    velocity.z = 0.0f;
                }
            } else {
                if (planar_saved_z.size() == mass_springs.positions.size() &&
                    planar_saved_topology == mass_springs.topology_version) {
                    for (size_t i = 0; i < mass_springs.positions.size(); ++i) {
                        mass_springs.positions[i].z = planar_saved_z[i];
                        mass_springs.previous_positions[i].z = planar_saved_z[i];
                    }
                } else if (!mass_springs.positions.empty()) {
                    // The graph changed while it was planar, so the saved depth no longer maps to
                    // the current nodes. Give the restored 3D layout an initial depth spread.
                    float min_x = mass_springs.positions[0].x;
                    float max_x = min_x;
                    float min_y = mass_springs.positions[0].y;
                    float max_y = min_y;
                    for (const Vector3& position : mass_springs.positions) {
                        min_x = std::min(min_x, position.x);
                        max_x = std::max(max_x, position.x);
                        min_y = std::min(min_y, position.y);
                        max_y = std::max(max_y, position.y);
                    }
                    const float z_scale =
                        std::max(1.0f, std::max(max_x - min_x, max_y - min_y) * 0.15f);
                    for (size_t i = 0; i < mass_springs.positions.size(); ++i) {
                        const float z =
                            static_cast<float>(GetRandomValue(-100, 100)) / 100.0f * z_scale;
                        mass_springs.positions[i].z = z;
                        mass_springs.previous_positions[i].z = z;
                        mass_springs.velocities[i].z = 0.0f;
                    }
                }
                planar_saved_z.clear();
            }
            mass_springs.tree.clear();
            last_mass_count = 0;
            if (gpu_was_active) {
                gpu_engine.mark_state_dirty();
            }
            last_planar = cfg.planar_graph;
        }

        const float timestep = 1.0f / std::max(1.0f, cfg.target_ups);
        physics_accumulator = std::min(physics_accumulator, std::chrono::duration<double>(0.25));

        // Handle queued commands
        {
#ifdef TRACY
            std::lock_guard<LockableBase(std::mutex)> lock(state.command_mtx);
#else
            std::lock_guard<std::mutex> lock(state.command_mtx);
#endif
            const bool had_commands = !state.pending_commands.empty();
            if (gpu_was_active && had_commands) {
                // Graph edits can change positions (new masses/springs), so bring the CPU
                // velocity state up to date before applying them.
                gpu_engine.sync_to_cpu(mass_springs);
            }
            while (!state.pending_commands.empty()) {
                command& cmd = state.pending_commands.front();
                cmd.visit(visitor);
                state.pending_commands.pop();
            }
            if (gpu_was_active && had_commands) {
                gpu_engine.mark_state_dirty();
            }
        }

        if (mass_springs.positions.empty()) {
            publish_tracked_memory();
            {
#ifdef TRACY
                std::lock_guard<LockableBase(std::mutex)> lock(state.data_mtx);
#else
                std::lock_guard<std::mutex> lock(state.data_mtx);
#endif
                state.readback_bytes.store(state.masses.capacity() * sizeof(Vector3),
                                           std::memory_order_relaxed);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        // Physics update
        bool stepped = false;
        const auto step_start = std::chrono::high_resolution_clock::now();
        if (physics_accumulator.count() > timestep) {
            stepped = true;
            auto stage_start = std::chrono::high_resolution_clock::now();

            const size_t position_count = mass_springs.positions.size();
            const bool size_changed = last_mass_count != position_count;
            ++step_index;

            bool use_gpu_tree = gpu_octree_selected;
            if (use_gpu_tree) {
                if (gpu_engine.poll_tree_build()) {
                    tree_ms = gpu_engine.gpu_tree_build_ms();
                    tree_cpu_ms = 0.0f;
                    wait_ms = 0.0f;
                }

                const bool tree_due = size_changed ||
                    !gpu_engine.gpu_tree_matches(position_count) ||
                    step_index % static_cast<size_t>(std::max(1, cfg.rebuild_interval)) == 0;
                if (tree_due) {
                    if (size_changed || !gpu_engine.gpu_tree_ready() ||
                        !gpu_engine.gpu_tree_matches(position_count)) {
                        // First tree, or a graph-size change: the old tree cannot be used for
                        // this step, so pay for a synchronous build.
                        const auto sync_start = std::chrono::high_resolution_clock::now();
                        if (!gpu_engine.build_tree(mass_springs.positions, cfg)) {
                            warnln("GPU octree build failed; falling back to CPU tree: {}",
                                   gpu_engine.error_message());
                            use_gpu_tree = false;
                            simulation_settings settings = get_settings();
                            settings.gpu_tree_sort = false;
                            set_settings(settings);
                            octree::build_octree_morton(mass_springs.tree, mass_springs.positions,
                                                        std::nullopt, cfg);
                        }
                        last_mass_count = position_count;
                        tree_ms = static_cast<float>(elapsed_ms(sync_start));
                        tree_cpu_ms = 0.0f;
                        wait_ms = 0.0f;
                    } else if (gpu_engine.gpu_tree_async_available()) {
                        // Build into the inactive buffer while forces keep using the active tree.
                        gpu_engine.request_tree_build(mass_springs.positions, cfg);
                        last_mass_count = position_count;
                    } else {
                        const auto sync_start = std::chrono::high_resolution_clock::now();
                        gpu_engine.build_tree(mass_springs.positions, cfg);
                        last_mass_count = position_count;
                        tree_ms = static_cast<float>(elapsed_ms(sync_start));
                        tree_cpu_ms = 0.0f;
                        wait_ms = 0.0f;
                    }
                }
            } else if (mass_springs.tree.empty()) {
                // First CPU step after a GPU-tree session (or after toggling the GPU tree off)
                // needs a matching CPU tree before forces can run.
                const auto sync_start = std::chrono::high_resolution_clock::now();
                octree::build_octree_morton(mass_springs.tree, mass_springs.positions, std::nullopt,
                                            cfg);
                last_mass_count = position_count;
                tree_ms = static_cast<float>(elapsed_ms(sync_start));
                tree_cpu_ms = tree_ms;
            } else if (!cfg.async_octree) {
                // Synchronous mode: build from the current positions before the force pass.
                const auto sync_start = std::chrono::high_resolution_clock::now();
                octree::build_octree_morton(mass_springs.tree, mass_springs.positions, thread_pool,
                                            cfg);
                last_mass_count = position_count;
                tree_ms = static_cast<float>(elapsed_ms(sync_start));
                tree_cpu_ms = tree_ms;
            } else if (size_changed) {
                // The tree has to match the mass set for this very step, so build synchronously.
                traceln("Rebuilding octree synchronously because graph size changed");
                const auto sync_start = std::chrono::high_resolution_clock::now();
                octree::build_octree_morton(mass_springs.tree, mass_springs.positions, std::nullopt,
                                            cfg);
                last_mass_count = position_count;
                tree_ms = static_cast<float>(elapsed_ms(sync_start));
                tree_cpu_ms = tree_ms;
            } else if (step_index % static_cast<size_t>(std::max(1, cfg.rebuild_interval)) == 0) {
                // Snapshot the positions so mass_springs is not mutating the vector while the
                // octree is building.
                std::vector<Vector3> positions = mass_springs.positions;

                octree_future = octree_thread.submit_task(
                    [&tree_buffer, &tree_ms, &tree_cpu_ms, &thread_pool, cfg,
                     positions = std::move(positions)]()
                    {
                        const auto tree_start = std::chrono::high_resolution_clock::now();
                    // MinGW has no CLOCK_THREAD_CPUTIME_ID, so Windows falls back to
                    // the wall time below, like the synchronous build paths do.
#if !defined(_WIN32)
                        timespec cpu_start{};
                        timespec cpu_end{};
                        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpu_start);
#endif

                        octree::build_octree_morton(tree_buffer, positions, thread_pool, cfg);

#if !defined(_WIN32)
                        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpu_end);
#endif
                        tree_ms = static_cast<float>(
                            std::chrono::duration<double, std::milli>(
                                std::chrono::high_resolution_clock::now() - tree_start)
                                .count());
#if defined(_WIN32)
                        tree_cpu_ms = tree_ms;
#else
                        tree_cpu_ms =
                            static_cast<float>(cpu_end.tv_sec - cpu_start.tv_sec) * 1000.0f +
                            static_cast<float>(cpu_end.tv_nsec - cpu_start.tv_nsec) / 1.0e6f;
#endif
                    });
            }

            build_ms = 0.9f * build_ms + 0.1f * elapsed_ms(stage_start);
            stage_start = std::chrono::high_resolution_clock::now();

            bool step_processed = false;
            const bool tree_ready = use_gpu_tree
                ? gpu_engine.gpu_tree_matches(mass_springs.positions.size())
                : (!mass_springs.tree.empty() &&
                   mass_springs.tree.positions_snapshot().size() == mass_springs.positions.size());
            using_gpu_tree = gpu_selected && use_gpu_tree && tree_ready;

            if (gpu_selected) {
                if (tree_ready) {
                    const bool solve_constraints =
                        step_index % static_cast<size_t>(std::max(1, cfg.constraint_interval)) == 0;
                    const auto gpu_start = std::chrono::high_resolution_clock::now();
                    const bool gpu_step_ok = use_gpu_tree
                        ? gpu_engine.step_gpu_tree(mass_springs, cfg, timestep * cfg.sim_speed,
                                                   solve_constraints)
                        : gpu_engine.step(mass_springs, mass_springs.tree, cfg,
                                          timestep * cfg.sim_speed, solve_constraints);
                    if (gpu_step_ok) {
                        gpu_step_ms =
                            0.9f * gpu_step_ms + 0.1f * static_cast<float>(elapsed_ms(gpu_start));
                        repulsion_ms = gpu_step_ms;
                        spring_ms = 0.0f;
                        integrate_ms = 0.0f;
                        step_processed = true;
                    } else {
                        warnln("GPU physics failed; falling back to CPU: {}",
                               gpu_engine.error_message());
                        gpu_ready = false;
                        gpu::set_active(false);
                        simulation_settings settings = get_settings();
                        settings.gpu_physics = false;
                        set_settings(settings);
                    }
                }
            } else {
                mass_springs.clear_forces();
                mass_springs.calculate_spring_forces(thread_pool);
                mass_springs.calculate_repulsion_forces(thread_pool);
                repulsion_ms = 0.9f * repulsion_ms + 0.1f * elapsed_ms(stage_start);
                stage_start = std::chrono::high_resolution_clock::now();

                mass_springs.update(timestep * cfg.sim_speed, thread_pool);
                integrate_ms = 0.9f * integrate_ms + 0.1f * elapsed_ms(stage_start);
                stage_start = std::chrono::high_resolution_clock::now();

                // Links have a hard minimum length: project compressed links back to REST_LENGTH.
                if (step_index % static_cast<size_t>(std::max(1, cfg.constraint_interval)) == 0) {
                    mass_springs.solve_distance_constraints(std::max(1, cfg.constraint_iterations),
                                                            thread_pool);
                }
                spring_ms = 0.9f * spring_ms + 0.1f * elapsed_ms(stage_start);
                gpu_step_ms = 0.0f;
                step_processed = true;
            }

            stepped = step_processed;
            if (step_processed) {
                ++loop_iterations;
                physics_accumulator -= std::chrono::duration<double>(timestep);
            }
        }

        // Wait for the async octree build (if one was submitted this step).
        if (octree_future.valid()) {
            const auto wait_start = std::chrono::high_resolution_clock::now();
            octree_future.wait();
            octree_future = std::future<void>{};
            std::swap(mass_springs.tree, tree_buffer);
            wait_ms = 0.9f * wait_ms + 0.1f * elapsed_ms(wait_start);
        }

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
                break;
            }

            if (ups_accumulator.count() > 1.0) {
                state.ups = loop_iterations;
                loop_iterations = 0;
                ups_accumulator = std::chrono::duration<double>(0);
            }
            if (using_gpu_tree && gpu_engine.gpu_tree_ready()) {
                state.mass_center = gpu_engine.gpu_tree_center();
            } else if (mass_springs.tree.empty()) {
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
            if (using_gpu_tree && gpu_engine.gpu_tree_ready()) {
                state.tree_nodes = gpu_engine.gpu_tree_nodes();
                state.tree_children = gpu_engine.gpu_tree_children();
                state.tree_stages = gpu_engine.gpu_tree_stages();
            } else {
                state.tree_nodes = mass_springs.tree.node_count();
                state.tree_children = mass_springs.tree.child_count();
                state.tree_stages = mass_springs.tree.stage_ms;
            }
            state.spring_ms = spring_ms;
            state.repulsion_ms = repulsion_ms;
            state.gpu_step_ms = gpu_step_ms;
            state.integrate_ms = integrate_ms;

            publish_tracked_memory();
            state.readback_bytes.store(state.masses.capacity() * sizeof(Vector3),
                                       std::memory_order_relaxed);

            state.data_ready = true;
            state.data_consumed = false;
        }
        state.data_ready_cnd.notify_all();

#ifdef TRACY
        FrameMarkEnd("PhysicsThreadProduceLock");
        FrameMarkEnd("PhysicsThread");
#endif
    }

    gpu_engine.shutdown();
    if (gpu != nullptr) {
        gpu->release_current();
    }
    gpu::set_active(false);
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
