#ifndef PHYSICS_HPP_
#define PHYSICS_HPP_

#include "config.hpp"
#include "octree.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <raylib.h>
#include <raymath.h>
#include <thread>
#include <variant>
#include <vector>

#include "util.hpp"

#ifdef THREADPOOL
    #if defined(_WIN32)
        #define NOGDI  // All GDI defines and routines
        #define NOUSER // All USER defines and routines
    #endif
    #define BS_THREAD_POOL_NATIVE_EXTENSIONS
    #include <BS_thread_pool.hpp>
    #if defined(_WIN32) // raylib uses these names as function parameters
        #undef near
        #undef far
    #endif
#endif

#ifdef TRACY
    #include <tracy/Tracy.hpp>
#endif

class mass
{
public:
    Vector3 position = Vector3Zero();
    Vector3 previous_position = Vector3Zero(); // for verlet integration
    Vector3 velocity = Vector3Zero();
    Vector3 force = Vector3Zero();

public:
    mass() = delete;

    explicit mass(const Vector3 _position) : position(_position), previous_position(_position)
    {}

public:
    auto clear_force() -> void;
    auto calculate_velocity(float delta_time) -> void;
    auto calculate_position(float delta_time) -> void;
    auto verlet_update(float delta_time) -> void;
};

class spring
{
public:
    size_t a;
    size_t b;

public:
    spring(const size_t _a, const size_t _b) : a(_a), b(_b)
    {}

public:
    static auto calculate_spring_force(mass& _a, mass& _b) -> void;
};

class mass_spring_system
{
private:
#ifdef THREADPOOL
    BS::thread_pool<> threads;
#endif

public:
    octree tree;

    // This is the main ownership of all the states/masses/springs.
    std::vector<mass> masses;
    std::vector<spring> springs;

public:
    mass_spring_system()
#ifdef THREADPOOL
        : threads(std::thread::hardware_concurrency() - 1, set_thread_name)
#endif
    {
        infoln("Using Barnes-Hut + Octree repulsion force calculation.");

#ifdef THREADPOOL
        infoln("Thread-pool: {} threads.", threads.get_thread_count());
#else
        infoln("Thread-pool: Disabled.");
#endif
    }

    mass_spring_system(const mass_spring_system& copy) = delete;
    auto operator=(const mass_spring_system& copy) -> mass_spring_system& = delete;
    mass_spring_system(mass_spring_system& move) = delete;
    auto operator=(mass_spring_system&& move) -> mass_spring_system& = delete;

private:
#ifdef THREADPOOL
    static auto set_thread_name(size_t idx) -> void;
#endif

    auto build_octree() -> void;

public:
    auto clear() -> void;
    auto add_mass() -> void;
    auto add_spring(size_t a, size_t b) -> void;

    auto clear_forces() -> void;
    auto calculate_spring_forces() -> void;
    auto calculate_repulsion_forces() -> void;
    auto verlet_update(float delta_time) -> void;

    auto center_masses() -> void;
};

class threaded_physics
{
    struct add_mass
    {};

    struct add_spring
    {
        size_t a;
        size_t b;
    };

    struct clear_graph
    {};

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
    threaded_physics() : physics(physics_thread, std::ref(state))
    {}

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
    static auto physics_thread(physics_state& state) -> void;

public:
    auto add_mass_cmd() -> void;

    auto add_spring_cmd(size_t a, size_t b) -> void;

    auto clear_cmd() -> void;

    auto add_mass_springs_cmd(size_t num_masses,
                              const std::vector<std::pair<size_t, size_t>>& springs) -> void;
};

#endif
