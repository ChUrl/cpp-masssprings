#ifndef MASS_SPRING_SYSTEM_HPP_
#define MASS_SPRING_SYSTEM_HPP_

#include "octree.hpp"
#include "config.hpp"

#include <optional>
#include <raylib.h>

using spring = std::pair<size_t, size_t>;

class cpu_spring_system
{
public:
    octree tree;

    // This is the main ownership of all the states/masses/springs.
    std::vector<Vector3> positions;
    std::vector<Vector3> previous_positions; // for verlet integration
    std::vector<Vector3> velocities;
    std::vector<Vector3> forces;

    std::vector<spring> springs;

public:
    cpu_spring_system() {}

    NO_COPY_NO_MOVE(cpu_spring_system);

public:
    auto clear() -> void;
    auto add_mass() -> void;
    auto add_spring(size_t a, size_t b) -> void;

    auto clear_forces() -> void;
    auto calculate_spring_force(size_t s) -> void;
    auto calculate_spring_forces(threadpool thread_pool = std::nullopt) -> void;
    auto calculate_repulsion_forces(threadpool thread_pool = std::nullopt) -> void;
    auto integrate_velocity(size_t m, float dt) -> void;
    auto integrate_position(size_t m, float dt) -> void;
    auto verlet_update(size_t m, float dt) -> void;
    auto update(float dt, threadpool thread_pool = std::nullopt) -> void;

    auto center_masses(threadpool thread_pool = std::nullopt) -> void;
};

#endif