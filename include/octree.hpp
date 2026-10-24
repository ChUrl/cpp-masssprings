#ifndef OCTREE_HPP_
#define OCTREE_HPP_

#include "config.hpp"
#include "settings.hpp"
#include "util.hpp"

#include <array>
#include <vector>

#include <libmorton/morton.h>
#include <raylib.h>
#include <raymath.h>

class octree
{
    class node
    {
    public:
        Vector3 mass_center = Vector3Zero(); // 12
        float mass_total = 0.0;              // 4
        float size = 0.0f;    // 4 - our octree cells are cubic, so no bounds are needed
        int mass_id = -1;     // 4 - >= 0 for a leaf, -1 for an internal node
        int first_child = -1; // 4 - index into octree::children (internal nodes only)
        u8 child_count = 0;   // 1 (+3 padding) - at most 8

        [[nodiscard]] auto leaf() const -> bool
        {
            return mass_id >= 0;
        }
    };
    static_assert(sizeof(node) == 32, "octree::node should stay compact for cache efficiency");

private:
    // Upper bound for the configurable tree depth (12 * 3 = 36 Morton bits). The actual depth
    // is a runtime setting in [6, MAX_DEPTH_LIMIT].
    static constexpr int MAX_DEPTH_LIMIT = 12;

    struct leaf_ref
    {
        u64 code;
        u32 first_leaf; // index of the first leaf covered by this subtree
        // Packed (level_index << 4) | depth. level_index needs < 2^28 entries, depth < 16.
        u32 depth_and_index;

        [[nodiscard]] auto depth() const -> int
        {
            return static_cast<int>(depth_and_index & 0xFu);
        }
        [[nodiscard]] auto level_index() const -> int
        {
            return static_cast<int>(depth_and_index >> 4);
        }
    };

    std::vector<node> nodes;

    // Layout is leaves first, then internal levels up to the root, so leaf nodes never have to
    // be copied into their final position.
    int root_idx = 0;

    // Build-time configuration. Force queries use these exact values so the tree and the force
    // model stay consistent even if the UI changes them later.
    int depth = 8;
    float theta = 1.6f;
    float bh_force = 3.0f;
    float softening = 0.05f;
    float mass = 1.0f;

    // Child node indices, stored contiguously per parent. Kept out of `node` so that leaves
    // (the vast majority of nodes) don't waste 32 bytes on unused child slots.
    std::vector<int> children;

    // Mass ids in Morton order of the last build. Iterating force queries in this order keeps
    // the tree traversal cache-friendly instead of jumping randomly through memory per mass.
    std::vector<u32> order;

    // Positions the tree was built from. Force queries must use these rather than the current
    // positions; mixing current positions with stale mass centers makes the force field
    // inconsistent and visibly jitters dense clusters.
    std::vector<Vector3> snapshot;

    // Scratch buffers reused across builds. Without these, every physics step allocated and
    // grew ~15 vectors, which showed up as allocator churn in the step time.
    // Packed (morton_code << 32) | mass_id. The code needs at most 3 * MAX_DEPTH bits, so an
    // 8-byte record is enough and halves the radix-sort memory traffic.
    std::vector<u64> sort_scratch;
    std::vector<u64> sort_tmp;

public:
    // Debug: wall time of each build stage (bbox, morton, sort, leaves, levels, flatten).
    std::array<float, 6> stage_ms{};

private:
    std::vector<std::vector<node>> level_scratch;
    std::vector<leaf_ref> leaves_scratch;
    std::vector<leaf_ref> next_refs_scratch;

    // This approach is actually slower than the array of nodes
    // beacuse we access all the attributes in the same function
    // std::vector<Vector3> mass_centers;
    // std::vector<float> mass_totals;
    // std::vector<Vector3> box_mins;
    // std::vector<Vector3> box_maxs;
    // std::vector<std::array<int, 8>> childrens;
    // std::vector<int> mass_ids;
    // std::vector<u8> leafs; // bitpacked std::vector<bool> is a lot slower

public:
    octree() = default;

    // Required for async octree
    // NO_COPY_NO_MOVE(octree);

private:
    // Map a floating point coordinate to a discrete integer (so its morton-code can be computed)
    // The "bits" parameter determines the discrete axis resolution
    [[nodiscard]] INLINE static inline auto quantize_axis(float coordinate, float box_min,
                                                          float box_max, int bits) -> u32;

    [[nodiscard]] INLINE static inline auto pos_to_morton(const Vector3& p, const Vector3& root_min,
                                                          const Vector3& root_max, int bits) -> u64;

    // Use this to obtain an ancestor node of a leaf node (on any level).
    // Because the morton codes (interleaved coordinates) encode the octree path, we can take
    // the morten code of any leaf and only take the 3*n first interleaved bits to find the
    // leaf ancestor on level n.
    // Leaf Code: [101 110 100 001] -> Ancestors (from leaf to root):
    // - [101 110 100]
    // - [101 110]
    // - [101] (root)
    [[nodiscard]] INLINE static inline auto path_to_ancestor(u64 leaf_code, int leaf_depth,
                                                             int depth) -> u64;

public:
    auto clear() -> void;
    [[nodiscard]] auto empty() const -> bool;
    [[nodiscard]] auto root() const -> const node&;
    [[nodiscard]] auto morton_order() const -> const std::vector<u32>&;
    [[nodiscard]] auto node_root_index() const -> int
    {
        return root_idx;
    }
    [[nodiscard]] auto positions_snapshot() const -> const std::vector<Vector3>&;
    [[nodiscard]] auto node_count() const -> size_t;
    [[nodiscard]] auto child_count() const -> size_t;

    // Morton/linear octree implementation
    static auto build_octree_morton(octree& t, const std::vector<Vector3>& positions,
                                    const std::optional<BS::thread_pool<>*>& thread_pool,
                                    const simulation_settings& settings) -> void;
    [[nodiscard]] auto calculate_force_morton(int node_idx, const Vector3& pos, int self_id) const
        -> Vector3;
};

INLINE inline auto octree::quantize_axis(const float coordinate, const float box_min,
                                         const float box_max, const int bits) -> u32
{
    const float extent = box_max - box_min;
    if (extent <= 0.0f) {
        return 0;
    }

    float normalized = (coordinate - box_min) / extent; // normalize to [0,1]
    normalized =
        std::max(0.0f, std::min(normalized, std::nextafter(1.0f, 0.0f))); // avoid exactly 1.0

    // bits up to 21 => (1u << bits) safe in 32-bit
    const u32 grid_max = (1u << bits) - 1u;
    return static_cast<u32>(normalized * static_cast<float>(grid_max));
}

INLINE inline auto octree::pos_to_morton(const Vector3& p, const Vector3& root_min,
                                         const Vector3& root_max, const int bits) -> u64
{
    const u32 x = quantize_axis(p.x, root_min.x, root_max.x, bits);
    const u32 y = quantize_axis(p.y, root_min.y, root_max.y, bits);
    const u32 z = quantize_axis(p.z, root_min.z, root_max.z, bits);
    return libmorton::morton3D_64_encode(x, y, z);
}

INLINE inline auto octree::path_to_ancestor(const u64 leaf_code, const int leaf_depth,
                                            const int depth) -> u64
{
    // keep top 3*depth bits; drop the rest
    const int drop = 3 * (leaf_depth - depth);
    return (drop > 0) ? (leaf_code >> drop) : leaf_code;
}

#endif
