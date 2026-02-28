#include "threaded_physics.hpp"
#include "config.hpp"
#include "mass_spring_system.hpp"

#include <chrono>
#include <raylib.h>
#include <raymath.h>
#include <utility>
#include <vector>

#ifdef TRACY
#include <tracy/Tracy.hpp>
#endif

auto threaded_physics::physics_thread(physics_state& state) -> void
{
    #ifdef THREADPOOL
    BS::this_thread::set_os_thread_name("physics");
    #endif

    mass_spring_system mass_springs;

    const auto visitor = overloads{
        [&](const struct add_mass& am)
        {
            mass_springs.add_mass();
        },
        [&](const struct add_spring& as)
        {
            mass_springs.add_spring(as.a, as.b);
        },
        [&](const struct clear_graph& cg)
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