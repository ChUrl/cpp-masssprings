#ifndef PHYSICS_HPP_
#define PHYSICS_HPP_

#include "config.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <raylib.h>
#include <raymath.h>
#include <thread>
#include <variant>
#include <vector>

#ifdef TRACY
#include <tracy/Tracy.hpp>
#endif

class threaded_physics
{
    struct add_mass {};

    struct add_spring
    {
        size_t a;
        size_t b;
    };

    struct clear_graph {};

    using command = std::variant<add_mass, add_spring, clear_graph>;

    struct physics_state
    {
        #ifdef TRACY
        TracyLockable(std::mutex, command_mtx);
        #else
        std::mutex command_mtx;
        #endif
        std::queue<command> pending_commands;

        #ifdef TRACY
        TracyLockable(std::mutex, data_mtx);
        #else
        std::mutex data_mtx;
        #endif
        std::condition_variable_any data_ready_cnd;
        std::condition_variable_any data_consumed_cnd;
        Vector3 mass_center = Vector3Zero();
        int ups = 0;
        size_t mass_count = 0;       // For debug
        size_t spring_count = 0;     // For debug
        std::vector<Vector3> masses; // Read by renderer
        bool data_ready = false;
        bool data_consumed = true;

        std::atomic<bool> running{true};
    };

private:
    std::thread physics;

public:
    physics_state state;

public:
    threaded_physics()
        : physics(physics_thread, std::ref(state)) {}

    threaded_physics(const threaded_physics& copy) = delete;
    auto operator=(const threaded_physics& copy) -> threaded_physics& = delete;
    threaded_physics(threaded_physics&& move) = delete;
    auto operator=(threaded_physics&& move) -> threaded_physics& = delete;

    ~threaded_physics()
    {
        state.running = false;
        state.data_ready_cnd.notify_all();
        state.data_consumed_cnd.notify_all();
        physics.join();
    }

private:
    #ifdef ASYNC_OCTREE
    static auto set_octree_pool_thread_name(size_t idx) -> void;
    #endif

    static auto physics_thread(physics_state& state) -> void;

public:
    auto clear_cmd() -> void;
    auto add_mass_cmd() -> void;
    auto add_spring_cmd(size_t a, size_t b) -> void;
    auto add_mass_springs_cmd(size_t num_masses, const std::vector<std::pair<size_t, size_t>>& springs) -> void;
};

#endif