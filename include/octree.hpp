#ifndef OCTREE_HPP_
#define OCTREE_HPP_

#include <array>
#include <vector>

#include <raylib.h>
#include <raymath.h>

class octree
{
    class node
    {
    public:
        Vector3 mass_center = Vector3Zero();
        float mass_total = 0.0;
        Vector3 box_min = Vector3Zero(); // area start
        Vector3 box_max = Vector3Zero(); // area end
        std::array<int, 8> children = {-1, -1, -1, -1, -1, -1, -1, -1};
        int mass_id = -1;
        bool leaf = true;

    public:
        [[nodiscard]] auto child_count() const -> int;
    };

public:
    static constexpr int MAX_DEPTH = 20;

    std::vector<node> nodes;

public:
    octree() = default;

    // octree(const octree& copy) = delete;
    // auto operator=(const octree& copy) -> octree& = delete;
    // octree(octree&& move) = delete;
    // auto operator=(octree&& move) -> octree& = delete;

public:
    [[nodiscard]] auto get_octant(int node_idx, const Vector3& pos) const -> int;
    [[nodiscard]] auto get_child_bounds(int node_idx, int octant) const
        -> std::pair<Vector3, Vector3>;
    auto create_empty_leaf(const Vector3& box_min, const Vector3& box_max) -> int;
    auto insert(int node_idx, int mass_id, const Vector3& pos, float mass, int depth) -> void;
    static auto build_octree(octree& t, const std::vector<Vector3>& positions) -> void;

    [[nodiscard]] auto calculate_force(int node_idx, const Vector3& pos) const -> Vector3;
};

#endif