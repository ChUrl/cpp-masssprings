#ifndef MASS_SPRING_SYSTEM_HPP_
#define MASS_SPRING_SYSTEM_HPP_

#include "octree.hpp"
#include "config.hpp"

#include <optional>
#include <raylib.h>

class mass_spring_system
{
public:
    class spring
    {
    public:
        size_t a;
        size_t b;

    public:
        spring(const size_t _a, const size_t _b)
            : a(_a), b(_b) {}
    };

public:
    static constexpr int SMALL_TASK_BLOCK_SIZE = 256;
    static constexpr int LARGE_TASK_BLOCK_SIZE = 256;

    octree tree;

    // This is the main ownership of all the states/masses/springs.
    std::vector<Vector3> positions;
    std::vector<Vector3> previous_positions; // for verlet integration
    std::vector<Vector3> velocities;
    std::vector<Vector3> forces;

    std::vector<spring> springs;

public:
    mass_spring_system() {}

    mass_spring_system(const mass_spring_system& copy) = delete;
    auto operator=(const mass_spring_system& copy) -> mass_spring_system& = delete;
    mass_spring_system(mass_spring_system& move) = delete;
    auto operator=(mass_spring_system&& move) -> mass_spring_system& = delete;

public:
    auto clear() -> void;
    auto add_mass() -> void;
    auto add_spring(size_t a, size_t b) -> void;

    auto clear_forces() -> void;
    auto calculate_spring_force(size_t s) -> void;
    auto calculate_spring_forces(std::optional<BS::thread_pool<>* const> thread_pool = std::nullopt) -> void;
    auto calculate_repulsion_forces(std::optional<BS::thread_pool<>* const> thread_pool = std::nullopt) -> void;
    auto integrate_velocity(size_t m, float dt) -> void;
    auto integrate_position(size_t m, float dt) -> void;
    auto verlet_update(size_t m, float dt) -> void;
    auto update(float dt, std::optional<BS::thread_pool<>* const> thread_pool = std::nullopt) -> void;

    auto center_masses(std::optional<BS::thread_pool<>* const> thread_pool = std::nullopt) -> void;
};

#endif