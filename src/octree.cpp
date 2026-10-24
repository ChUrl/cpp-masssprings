#include "octree.hpp"
#include "config.hpp"

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <raymath.h>

auto octree::clear() -> void
{
    nodes.clear();
    children.clear();
    order.clear();
    snapshot.clear();
    root_idx = 0;
}

auto octree::morton_order() const -> const std::vector<u32>&
{
    return order;
}

auto octree::positions_snapshot() const -> const std::vector<Vector3>&
{
    return snapshot;
}

auto octree::node_count() const -> size_t
{
    return nodes.size();
}

auto octree::child_count() const -> size_t
{
    return children.size();
}

auto octree::empty() const -> bool
{
    return nodes.empty();
}

auto octree::root() const -> const node&
{
    return nodes[static_cast<size_t>(root_idx)];
}

// Replaced the 50 line recursive octree insertion with this morton bitch to gain 5 UPS, FML
auto octree::build_octree_morton(octree& t, const std::vector<Vector3>& positions,
                                 const std::optional<BS::thread_pool<>*>& thread_pool) -> void
{
#ifdef TRACY
    ZoneScoped;
#endif

    t.clear();
    if (positions.empty()) {
        return;
    }

    // Keep the exact configuration the tree is built from; force queries use this snapshot.
    t.snapshot = positions;

    auto stage_start = std::chrono::high_resolution_clock::now();
    const auto mark_stage = [&](const int idx)
    {
        const auto now = std::chrono::high_resolution_clock::now();
        t.stage_ms[idx] = std::chrono::duration<float, std::milli>(now - stage_start).count();
        stage_start = now;
    };

    // Compute bounding box around all masses
    Vector3 root_min{FLT_MAX, FLT_MAX, FLT_MAX};
    Vector3 root_max{-FLT_MAX, -FLT_MAX, -FLT_MAX};
    for (const auto& [x, y, z] : positions) {
        root_min.x = std::min(root_min.x, x);
        root_max.x = std::max(root_max.x, x);
        root_min.y = std::min(root_min.y, y);
        root_max.y = std::max(root_max.y, y);
        root_min.z = std::min(root_min.z, z);
        root_max.z = std::max(root_max.z, z);
    }

    constexpr float pad = 1.0f;
    root_min = Vector3Subtract(root_min, Vector3Scale(Vector3One(), pad));
    root_max = Vector3Add(root_max, Vector3Scale(Vector3One(), pad));

    const float max_extent =
        std::max({root_max.x - root_min.x, root_max.y - root_min.y, root_max.z - root_min.z});
    root_max = Vector3Add(root_min, Vector3Scale(Vector3One(), max_extent));

    const float root_extent = root_max.x - root_min.x; // cubic
    mark_stage(0);

    // Container for building the particle list before sorting by morton code (reused across
    // builds)
    auto& sort_container = t.sort_scratch;
    sort_container.resize(positions.size());

    const auto calculate_morton = [&](const u32 i)
    { sort_container[i] = (pos_to_morton(positions[i], root_min, root_max) << 32) | i; };

    if (thread_pool) {
        (*thread_pool)
            ->submit_loop(0, positions.size(), calculate_morton, SMALL_TASK_BLOCK_SIZE)
            .wait();
    } else {
        for (u32 i = 0; i < positions.size(); ++i) {
            calculate_morton(i);
        }
    }
    mark_stage(1);

    // Sort the list by morton codes. Because positions close to each other have similar morton
    // codes, this provides us with "spatial locality" in the datastructure.
    //
    // The code lives in the upper 32 bits, so the radix sort only needs to touch the 3 *
    // MAX_DEPTH code bits (2 passes at depth 8). Both buffers are reused across builds.
    {
        auto& sort_tmp = t.sort_tmp;
        sort_tmp.resize(sort_container.size());

        auto* src = &sort_container;
        auto* dst = &sort_tmp;

        for (int shift = 32; shift < 32 + 3 * MAX_DEPTH; shift += 12) {
            std::array<u32, 1 << 12> histogram{};
            for (const u64 key : *src) {
                ++histogram[(key >> shift) & 0xFFFu];
            }

            u32 sum = 0;
            for (u32& bucket : histogram) {
                const u32 count = bucket;
                bucket = sum;
                sum += count;
            }

            for (const u64 key : *src) {
                (*dst)[histogram[(key >> shift) & 0xFFFu]++] = key;
            }

            std::swap(src, dst);
        }

        // An odd number of passes leaves the result in the temporary buffer.
        if (src != &sort_container) {
            sort_container.swap(sort_tmp);
        }
    }

    // Resolve duplicate cells by bumping the code to the next free one. Multiple masses can
    // quantize to the same leaf cell (dense clusters, coincident positions), and every leaf must
    // have a unique code for the child slots of the internal nodes. Doing this on the integer
    // code instead of jittering the float position is exact and always terminates:
    // - the array stays sorted because code[i] only ever grows to code[i-1] + 1
    // - no re-sorting is required
    // - the force position (stored later from `positions`) is left untouched
    for (size_t i = 1; i < sort_container.size(); ++i) {
        const u64 previous_code = sort_container[i - 1] >> 32;
        if ((sort_container[i] >> 32) <= previous_code) {
            sort_container[i] = ((previous_code + 1) << 32) | static_cast<u32>(sort_container[i]);
        }
    }
    mark_stage(2);

    auto& tree_levels = t.level_scratch;
    tree_levels.resize(MAX_DEPTH + 1);
    for (auto& level : tree_levels) {
        level.clear();
    }

    // Leaves live directly at the start of the final node array (1 particle per leaf, in Morton
    // order), so the flatten step never has to copy them.
    t.order.resize(sort_container.size());

    auto& leaves = t.leaves_scratch;
    leaves.resize(sort_container.size());

    t.nodes.clear();
    t.nodes.resize(sort_container.size());

    const float leaf_size = root_extent / static_cast<float>(1u << MAX_DEPTH);
    for (u32 i = 0; i < static_cast<u32>(sort_container.size()); ++i) {
        const u64 key = sort_container[i];
        const u32 id = static_cast<u32>(key);
        const Vector3& pos = positions[id];

        node& leaf = t.nodes[i];
        leaf.mass_id = static_cast<int>(id);
        leaf.size = leaf_size;
        leaf.mass_total = MASS;
        leaf.mass_center = pos; // exact (unjittered) position
        leaf.first_child = -1;
        leaf.child_count = 0;

        t.order[i] = id;
        leaves[i] = {key >> 32, i, (i << 4) | MAX_DEPTH};
    }
    mark_stage(3);

    auto& next_refs = t.next_refs_scratch;

    // Build internal levels from MAX_DEPTH-1 to 0. Each element's ancestor is computed exactly
    // once per level (the previous version computed it again for every comparison), and the
    // working vectors are reused across levels and frames.
    for (int current_depth = MAX_DEPTH - 1; current_depth >= 0; --current_depth) {
        auto& current_level = tree_levels[current_depth];
        current_level.clear();
        next_refs.clear();
        next_refs.reserve(leaves.size());

        if (leaves.empty()) {
            break;
        }

        const float parent_size = root_extent / static_cast<float>(1u << current_depth);

        size_t group_begin = 0;
        u64 key = path_to_ancestor(leaves[0].code, MAX_DEPTH, current_depth);
        for (size_t i = 1; i <= leaves.size(); ++i) {
            const u64 next_key = (i < leaves.size())
                ? path_to_ancestor(leaves[i].code, MAX_DEPTH, current_depth)
                : ~0ull; // sentinel: cannot collide with a 3*MAX_DEPTH bit key
            if (next_key == key) {
                continue;
            }

            const size_t group_size = i - group_begin;

            // Unary compression: just carry the child ref upward unchanged.
            if (group_size == 1) {
                next_refs.push_back(leaves[group_begin]);
            } else if (group_size > 1) {
                // The group covers a contiguous range of leaves; sum their centers directly.
                // The leaf nodes were just written and the range is sequential, so this is
                // cheaper than maintaining a separate prefix-sum array.
                const u32 first_leaf = leaves[group_begin].first_leaf;
                const u32 end_leaf = (i < leaves.size()) ? leaves[i].first_leaf
                                                         : static_cast<u32>(sort_container.size());
                const u32 leaf_count = end_leaf - first_leaf;

                Vector3 center = Vector3Zero();
                for (u32 leaf = first_leaf; leaf < end_leaf; ++leaf) {
                    center = Vector3Add(center, t.nodes[leaf].mass_center);
                }

                node parent;
                parent.mass_id = -1;
                parent.size = parent_size;
                parent.first_child = static_cast<int>(t.children.size());
                parent.child_count = static_cast<u8>(group_size);
                parent.mass_total = MASS * static_cast<float>(leaf_count);
                parent.mass_center = Vector3Scale(center, 1.0f / static_cast<float>(leaf_count));

                // Store packed local references; they are converted to global indices after all
                // levels have been flattened.
                for (size_t k = group_begin; k < i; ++k) {
                    const int child_depth = leaves[k].depth();
                    const int child_local = leaves[k].level_index();
                    t.children.push_back((child_depth << 24) | (child_local & 0x00FFFFFF));
                }

                const int parent_local = static_cast<int>(current_level.size());
                current_level.push_back(parent);

                next_refs.push_back(
                    {leaves[group_begin].code, first_leaf,
                     (static_cast<u32>(parent_local) << 4) | static_cast<u32>(current_depth)});
            }

            group_begin = i;
            key = next_key;
        }

        leaves.swap(next_refs);
    }
    mark_stage(4);

    // Flatten levels: all depth 0 nodes come first, then depth 1, last depth MAX_DEPTH.
    // Leaves already occupy [0, leaf_count). Append internal levels from deepest to the root;
    // leaves are never copied.
    std::array<int, MAX_DEPTH + 1> offsets{};
    offsets[MAX_DEPTH] = 0;
    int total = static_cast<int>(t.nodes.size());
    for (int d = MAX_DEPTH - 1; d >= 0; --d) {
        offsets[d] = total;
        total += static_cast<int>(tree_levels[d].size());
    }

    t.nodes.reserve(total);
    for (int d = MAX_DEPTH - 1; d >= 0; --d) {
        t.nodes.insert(t.nodes.end(), tree_levels[d].begin(), tree_levels[d].end());
    }

    // Convert the packed local child references into global node indices.
    for (int& packed : t.children) {
        const int child_depth = (packed >> 24) & 0xFF;
        const int child_local = packed & 0x00FFFFFF;
        packed = offsets[child_depth] + child_local;
    }

    t.root_idx = tree_levels[0].empty() ? 0 : offsets[0];
    mark_stage(5);
}

auto octree::calculate_force_morton(const int node_idx, const Vector3& pos, const int self_id) const
    -> Vector3
{
    if (node_idx < 0) {
        return Vector3Zero();
    }

    // Force accumulator
    float fx = 0.0f;
    float fy = 0.0f;
    float fz = 0.0f;

    // A DFS over the octree can hold at most 7 pending siblings per level plus the
    // current path, so 1 + 7 * MAX_DEPTH entries are enough. Keep the stack on the
    // caller's stack: this function is called once per mass per physics update, and
    // a heap allocation here dominates large graphs.
    std::array<int, 1 + 7 * MAX_DEPTH> stack{};
    size_t stack_size = 0;
    stack[stack_size++] = node_idx;

    constexpr float theta2 = THETA * THETA;

    while (stack_size > 0) {
        const int idx = stack[--stack_size];

        const node& n = nodes[idx];
        const bool is_leaf = n.leaf();

        // No self-force for single-particle leafs
        if (is_leaf && n.mass_id == self_id) {
            continue;
        }

        const float dx = pos.x - n.mass_center.x;
        const float dy = pos.y - n.mass_center.y;
        const float dz = pos.z - n.mass_center.z;

        const float dist_sq = dx * dx + dy * dy + dz * dz + SOFTENING;

        // Barnes–Hut criterion
        if (is_leaf || ((n.size * n.size) / dist_sq) < theta2) {
            const float inv_dist = 1.0f / std::sqrt(dist_sq);
            const float force_mag = (BH_FORCE * n.mass_total) / dist_sq; // ~ 1/r^2
            const float s = force_mag * inv_dist; // scale by 1/r to get vector
            fx += dx * s;
            fy += dy * s;
            fz += dz * s;
            continue;
        }

        const int first_child = n.first_child;
        const int end = first_child + n.child_count;
        for (int c = first_child; c < end; ++c) {
            stack[stack_size++] = children[c];
        }
    }

    return Vector3{fx, fy, fz};
}
