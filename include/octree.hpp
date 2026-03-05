#ifndef OCTREE_HPP_
#define OCTREE_HPP_

#include "util.hpp"

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
        node(const Vector3& _box_min, const Vector3& _box_max) : box_min(_box_min), box_max(_box_max) {}
    };

public:
    static constexpr int MAX_DEPTH = 20;

    std::vector<node> nodes;

    // This approach is actually slower than the array of nodes
    // beacuse we access all the attributes in the same function

    // std::vector<Vector3> mass_centers;
    // std::vector<float> mass_totals;
    // std::vector<Vector3> box_mins;
    // std::vector<Vector3> box_maxs;
    // std::vector<std::array<int, 8>> childrens;
    // std::vector<int> mass_ids;
    // std::vector<uint8_t> leafs; // bitpacked std::vector<bool> is a lot slower

public:
    octree() = default;

    octree(const octree& copy) = delete;
    auto operator=(const octree& copy) -> octree& = delete;
    octree(octree&& move) = delete;
    auto operator=(octree&& move) -> octree& = delete;

public:
    auto clear() -> void;
    auto reserve(size_t count) -> void;
    [[nodiscard]] auto empty() const -> bool;
    [[nodiscard]] INLINE static inline auto get_octant(const Vector3& box_min,
                                                       const Vector3& box_max,
                                                       const Vector3& pos) -> int;
    [[nodiscard]] INLINE static inline auto get_child_bounds(const Vector3& box_min,
                                                             const Vector3& box_max,
                                                             int octant) -> std::pair<Vector3, Vector3>;
    INLINE inline auto create_empty_leaf(const Vector3& box_min, const Vector3& box_max) -> int;
    [[nodiscard]] auto get_child_count(int node_idx) const -> int;
    auto insert(int node_idx, int mass_id, const Vector3& pos, float mass, int depth) -> void;
    static auto build_octree(octree& t, const std::vector<Vector3>& positions) -> void;

    [[nodiscard]] auto calculate_force(int node_idx, const Vector3& pos) const -> Vector3;
};

INLINE inline auto octree::get_octant(const Vector3& box_min, const Vector3& box_max, const Vector3& pos) -> int
{
    auto [cx, cy, cz] = (box_min + box_max) / 2.0f;

    // The octant is encoded as a 3-bit integer "zyx". The node area is split
    // along all 3 axes, if a position is right of an axis, this bit is set to 1.
    // If a position is right of the x-axis and y-axis and left of the z-axis, the
    // encoded octant is "011".
    return (pos.x >= cx) | ((pos.y >= cy) << 1) | ((pos.z >= cz) << 2);
}

INLINE inline auto octree::get_child_bounds(const Vector3& box_min,
                                            const Vector3& box_max,
                                            const int octant) -> std::pair<Vector3, Vector3>
{
    auto [cx, cy, cz] = (box_min + box_max) / 2.0f;

    Vector3 min = Vector3Zero();
    Vector3 max = Vector3Zero();

    // If (octant & 1), the octant is to the right of the node region's x-axis
    // (see GetOctant). This means the left bound is the x-axis and the right
    // bound the node's region max.
    min.x = octant & 1 ? cx : box_min.x;
    max.x = octant & 1 ? box_max.x : cx;
    min.y = octant & 2 ? cy : box_min.y;
    max.y = octant & 2 ? box_max.y : cy;
    min.z = octant & 4 ? cz : box_min.z;
    max.z = octant & 4 ? box_max.z : cz;

    return std::make_pair(min, max);
}

INLINE inline auto octree::create_empty_leaf(const Vector3& box_min, const Vector3& box_max) -> int
{
    nodes.emplace_back(box_min, box_max);
    return static_cast<int>(nodes.size() - 1);

    // mass_centers.emplace_back(Vector3Zero());
    // mass_totals.emplace_back(0);
    // box_mins.emplace_back(box_min);
    // box_maxs.emplace_back(box_max);
    // childrens.push_back({-1, -1, -1, -1, -1, -1, -1, -1});
    // mass_ids.emplace_back(-1);
    // leafs.emplace_back(true);
}

#endif