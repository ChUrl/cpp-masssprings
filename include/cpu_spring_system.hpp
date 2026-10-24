#ifndef MASS_SPRING_SYSTEM_HPP_
#define MASS_SPRING_SYSTEM_HPP_

#include "config.hpp"
#include "octree.hpp"

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

private:
    // Edge coloring for the parallel distance-constraint solve: no two springs of the same color
    // share a mass, so each color can be projected in parallel without races.
    std::vector<u32> spring_color;
    std::vector<u32> color_offsets;   // CSR offsets into colored_springs, size max_color + 2
    std::vector<u32> colored_springs; // spring indices grouped by color
    u32 max_spring_color = 0;
    bool coloring_dirty = true;

    auto rebuild_coloring() -> void;

public:
    cpu_spring_system()
    {}

    NO_COPY_NO_MOVE(cpu_spring_system);

public:
    auto clear() -> void;
    auto add_mass() -> void;
    auto add_spring(size_t a, size_t b) -> void;

    auto clear_forces() -> void;
    auto calculate_spring_force(size_t s) -> void;
    auto calculate_spring_forces(threadpool thread_pool = std::nullopt) -> void;
    auto calculate_repulsion_forces(threadpool thread_pool = std::nullopt) -> void;
    // Position-based distance constraints: project linked masses back to REST_LENGTH.
    auto solve_distance_constraints(int iterations, threadpool thread_pool = std::nullopt) -> void;
    auto verlet_update(size_t m, float dt) -> void;
    auto update(float dt, threadpool thread_pool = std::nullopt) -> void;
};

#endif
