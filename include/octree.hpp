#ifndef OCTREE_HPP_
#define OCTREE_HPP_

#include "util.hpp"
#include "config.hpp"

#include <array>
#include <vector>

#include <raylib.h>
#include <raymath.h>
#include <libmorton/morton.h>

class octree
{
    class node
    {
    public:
        Vector3 mass_center = Vector3Zero();
        float mass_total = 0.0;
        uint8_t depth = 0;
        float size = 0.0f; // Because our octree cells are cubic we don't need to store the bounds
        std::array<int, 8> children = {-1, -1, -1, -1, -1, -1, -1, -1};
        int mass_id = -1;
        bool leaf = true;
    };

private:
    // 21 * 3 = 63, fits in uint64_t for combined x/y/z morton-code
    static constexpr int MAX_DEPTH = 21;

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

private:
    [[nodiscard]] INLINE static inline auto get_octant(const Vector3& box_min,
                                                       const Vector3& box_max,
                                                       const Vector3& pos) -> int;
    [[nodiscard]] INLINE static inline auto get_child_bounds(const Vector3& box_min,
                                                             const Vector3& box_max,
                                                             int octant) -> std::pair<Vector3, Vector3>;

    // Map a floating point coordinate to a discrete integer (so its morton-code can be computed)
    // The "bits" parameter determines the discrete axis resolution
    [[nodiscard]] INLINE static inline auto quantize_axis(float coordinate,
                                                          float box_min,
                                                          float box_max,
                                                          int bits) -> uint32_t;

    [[nodiscard]] INLINE static inline auto pos_to_morton(const Vector3& p,
                                                          const Vector3& root_min,
                                                          const Vector3& root_max) -> uint64_t;

    [[nodiscard]] INLINE static inline auto jitter_pos(Vector3 p,
                                                       uint32_t seed,
                                                       const Vector3& root_min,
                                                       const Vector3& root_max,
                                                       float root_extent) -> Vector3;

    // Use this to obtain an ancestor node of a leaf node (on any level).
    // Because the morton codes (interleaved coordinates) encode the octree path, we can take
    // the morten code of any leaf and only take the 3*n first interleaved bits to find the
    // leaf ancestor on level n.
    // Leaf Code: [101 110 100 001] -> Ancestors (from leaf to root):
    // - [101 110 100]
    // - [101 110]
    // - [101] (root)
    [[nodiscard]] INLINE static inline auto path_to_ancestor(uint64_t leaf_code, int leaf_depth, int depth) -> uint64_t;

    // Use this to obtain the octant a leaf node is contained in (on any level).
    // The 3 interleaved bits in the morten code encode the octant [0, 7].
    // Leaf Code: [101 110 100 001] -> Octants:
    // - [100] (Level 2)
    // - [110] (Level 1)
    // - [101] (Level 0)
    [[nodiscard]] INLINE static inline auto octant_at_level(uint64_t leaf_code, int level, int leaf_depth) -> int;

public:
    auto clear() -> void;
    auto reserve(size_t count) -> void;
    [[nodiscard]] auto empty() const -> bool;
    [[nodiscard]] auto root() const -> const node&;

    // Morton/linear octree implementation
    static auto build_octree_morton(octree& t,
                                    const std::vector<Vector3>& positions,
                                    const std::optional<BS::thread_pool<>*>& thread_pool) -> void;
    [[nodiscard]] auto calculate_force_morton(int node_idx, const Vector3& pos, int self_id) const -> Vector3;
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

INLINE inline auto octree::quantize_axis(const float coordinate,
                                         const float box_min,
                                         const float box_max,
                                         const int bits) -> uint32_t
{
    const float extent = box_max - box_min;
    if (extent <= 0.0f) {
        return 0;
    }

    float normalized = (coordinate - box_min) / extent;                            // normalize to [0,1]
    normalized = std::max(0.0f, std::min(normalized, std::nextafter(1.0f, 0.0f))); // avoid exactly 1.0

    // bits up to 21 => (1u << bits) safe in 32-bit
    const uint32_t grid_max = (1u << bits) - 1u;
    return static_cast<uint32_t>(normalized * static_cast<float>(grid_max));
}

INLINE inline auto octree::pos_to_morton(const Vector3& p, const Vector3& root_min, const Vector3& root_max) -> uint64_t
{
    const uint32_t x = quantize_axis(p.x, root_min.x, root_max.x, MAX_DEPTH);
    const uint32_t y = quantize_axis(p.y, root_min.y, root_max.y, MAX_DEPTH);
    const uint32_t z = quantize_axis(p.z, root_min.z, root_max.z, MAX_DEPTH);
    return libmorton::morton3D_64_encode(x, y, z);
}

INLINE inline auto octree::jitter_pos(Vector3 p,
                                      const uint32_t seed,
                                      const Vector3& root_min,
                                      const Vector3& root_max,
                                      const float root_extent) -> Vector3
{
    // Use a hash to calculate a deterministic jitter: The same position should always get the same jitter.
    // We want this to get stable physics, particles at the same position shouldn't get different jitters
    // across frames...
    uint32_t h = (seed ^ 61u) ^ (seed >> 16);
    h *= 9u;
    h = h ^ (h >> 4);
    h *= 0x27d4eb2du;
    h = h ^ (h >> 15);

    // finest cell size at depth L
    const float finest_cell = root_extent / static_cast<float>(1u << MAX_DEPTH);
    const float s = finest_cell * 1e-4f; // small pp

    p.x += (h & 1u) ? +s : -s;
    p.y += (h & 2u) ? +s : -s;
    p.z += (h & 4u) ? +s : -s;

    // clamp back into bounds just in case
    p.x = std::max(root_min.x, std::min(p.x, root_max.x));
    p.y = std::max(root_min.y, std::min(p.y, root_max.y));
    p.z = std::max(root_min.z, std::min(p.z, root_max.z));

    return p;
}

INLINE inline auto octree::path_to_ancestor(const uint64_t leaf_code, const int leaf_depth, const int depth) -> uint64_t
{
    // keep top 3*depth bits; drop the rest
    const int drop = 3 * (leaf_depth - depth);
    return (drop > 0) ? (leaf_code >> drop) : leaf_code;
}

INLINE inline auto octree::octant_at_level(const uint64_t leaf_code, const int level, const int leaf_depth) -> int
{
    // level 1 => child of root => topmost 3 bits
    const int shift = 3 * (leaf_depth - level);
    return static_cast<int>((leaf_code >> shift) & 0x7ull);
}

#endif