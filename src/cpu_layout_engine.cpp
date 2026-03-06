#include "cpu_layout_engine.hpp"
#include "config.hpp"
#include "cpu_spring_system.hpp"
#include "util.hpp"

#include <chrono>
#include <raylib.h>
#include <raymath.h>
#include <utility>
#include <vector>

#ifdef ASYNC_OCTREE
auto cpu_layout_engine::set_octree_pool_thread_name(size_t idx) -> void
{
    BS::this_thread::set_os_thread_name(std::format("octree-{}", idx));
    // traceln("Using thread \"{}\"", BS::this_thread::get_os_thread_name().value_or("INVALID NAME"));
}
#endif

auto cpu_layout_engine::physics_thread(physics_state& state, const std::optional<BS::thread_pool<>* const> thread_pool) -> void
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
        [&](const struct add_mass&)
        {
            mass_springs.add_mass();
        },
        [&](const struct add_spring& as)
        {
            mass_springs.add_spring(as.a, as.b);
        },
        [&](const struct clear_graph&)
        {
            mass_springs.clear();
        },
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

        if (mass_springs.positions.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        // Physics update
        if (physics_accumulator.count() > TIMESTEP) {
            #ifdef ASYNC_OCTREE
            // Snapshot the positions so mass_springs is not mutating the vector while the octree is building
            std::vector<Vector3> positions = mass_springs.positions;

            // Start building the octree for the next physics update.
            // Move the snapshot into the closure so it doesn't get captured by reference (don't use [&])
            octree_future = octree_thread.submit_task([&tree_buffer, &thread_pool, positions = std::move(positions)]()
            {
                octree::build_octree_morton(tree_buffer, positions, thread_pool);
            });

            // Rebuild the tree synchronously if we changed the number of masses to not use
            // an empty tree from the last frame in the frame where the graph was generated
            if (last_mass_count != mass_springs.positions.size()) {
                traceln("Rebuilding octree synchronously because graph size changed");
                octree::build_octree_morton(mass_springs.tree, mass_springs.positions, thread_pool);
                last_mass_count = mass_springs.positions.size();
            }
            #else
            octree::build_octree_morton(mass_springs.tree, mass_springs.positions, thread_pool);
            #endif

            mass_springs.clear_forces();
            mass_springs.calculate_spring_forces(thread_pool);
            mass_springs.calculate_repulsion_forces(thread_pool);
            mass_springs.update(TIMESTEP * SIM_SPEED, thread_pool);

            // This is only helpful if we're drawing a grid at (0, 0, 0). Otherwise, it's just
            // expensive and yields no benefit since we can lock the camera to the center of mass
            // cheaply.
            // mass_springs.center_masses(thread_pool);

            ++loop_iterations;
            physics_accumulator -= std::chrono::duration<double>(TIMESTEP);
        }

        #ifdef ASYNC_OCTREE
        // Wait for the octree to be built
        if (octree_future.valid()) {
            octree_future.wait();
            octree_future = std::future<void>{};
            std::swap(mass_springs.tree, tree_buffer);
        }
        #endif

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
            {
                return state.data_consumed || !state.running.load();
            });
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

            state.data_ready = true;
            state.data_consumed = false;
        }
        // Notify the rendering thread that new data is available
        state.data_ready_cnd.notify_all();

        #ifdef TRACY
        FrameMarkEnd("PhysicsThreadProduceLock"); FrameMarkEnd("PhysicsThread");
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
                                            const std::vector<std::pair<size_t, size_t>>& springs) -> void
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