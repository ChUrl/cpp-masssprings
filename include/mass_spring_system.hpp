#ifndef MASS_SPRING_SYSTEM_HPP_
#define MASS_SPRING_SYSTEM_HPP_

#include "octree.hpp"
#include "util.hpp"
#include "config.hpp"

#include <raylib.h>
#include <raymath.h>

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

class mass_spring_system
{
public:
    class mass
    {
    public:
        Vector3 position = Vector3Zero();
        Vector3 previous_position = Vector3Zero(); // for verlet integration
        Vector3 velocity = Vector3Zero();
        Vector3 force = Vector3Zero();

    public:
        mass() = delete;

        explicit mass(const Vector3 _position)
            : position(_position), previous_position(_position) {}

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
        spring(const size_t _a, const size_t _b)
            : a(_a), b(_b) {}

    public:
        static auto calculate_spring_force(mass& _a, mass& _b) -> void;
    };

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

#endif