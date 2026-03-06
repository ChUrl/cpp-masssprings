#include "octree.hpp"
#include "config.hpp"

#include <cfloat>
#include <raymath.h>

auto octree::clear() -> void
{
    nodes.clear();
}

auto octree::reserve(const size_t count) -> void
{
    nodes.reserve(count);
}

auto octree::empty() const -> bool
{
    return nodes.empty();
}

auto octree::root() const -> const node&
{
    return nodes[0];
}

// Replaced the 50 line recursive octree insertion with this bitch to gain 5 UPS, FML
auto octree::build_octree_morton(octree& t, const std::vector<Vector3>& positions) -> void
{
    #ifdef TRACY
    ZoneScoped;
    #endif

    t.clear();
    if (positions.empty()) {
        return;
    }

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

    const float max_extent = std::max({root_max.x - root_min.x, root_max.y - root_min.y, root_max.z - root_min.z});
    root_max = Vector3Add(root_min, Vector3Scale(Vector3One(), max_extent));

    const float root_extent = root_max.x - root_min.x; // cubic

    // Container for building the particle list before sorting by morton code
    struct sort_node
    {
        uint64_t code;
        uint32_t id;
        Vector3 pos;
    };

    std::vector<sort_node> sort_container;
    sort_container.reserve(positions.size());
    for (uint32_t i = 0; i < positions.size(); ++i) {
        sort_container.emplace_back(pos_to_morton(positions[i], root_min, root_max), i, positions[i]);
    }

    // Sort the list by morton codes. Because positions close to each other have similar morten codes,
    // this provides us with "spatial locality" in the datastructure.
    auto sort_by_code = [&]()
    {
        std::ranges::sort(sort_container,
                          [](const sort_node& a, const sort_node& b)
                          {
                              if (a.code != b.code) {
                                  return a.code < b.code;
                              }
                              return a.id < b.id;
                          });
    };

    sort_by_code();

    // Resolve duplicates by jittering the later one deterministically and re-encoding.
    for (int seed = 0; seed < 8; ++seed) {
        bool had_dupes = false;

        for (size_t i = 1; i < sort_container.size(); ++i) {
            // Because elements are spatially ordered after sorting, we can scan for duplicates in O(n)
            if (sort_container[i].code == sort_container[i - 1].code) {
                had_dupes = true;
                sort_container[i].pos = jitter_pos(sort_container[i].pos,
                                                   sort_container[i].id + seed * 0x9e3779b9u,
                                                   root_min,
                                                   root_max,
                                                   root_extent);
                sort_container[i].code = pos_to_morton(sort_container[i].pos, root_min, root_max);
            }
        }

        if (!had_dupes) {
            break;
        }
        sort_by_code();
    }

    // Sanity check
    for (size_t i = 1; i < sort_container.size(); ++i) {
        if (sort_container[i].code == sort_container[i - 1].code) {
            throw std::runtime_error("Duplicates remain after jittering");
        }
    }

    std::vector<std::vector<node>> tree_levels;
    tree_levels.assign(MAX_DEPTH + 1, {});

    // Leaves at MAX_DEPTH: 1 particle per leaf in morton order (close particles close together)
    auto& leafs = tree_levels[MAX_DEPTH];
    leafs.reserve(sort_container.size());
    const float leaf_size = root_extent / static_cast<float>(MAX_DEPTH);
    for (const auto& [code, id, pos] : sort_container) {
        node leaf;
        leaf.leaf = true;
        leaf.mass_id = static_cast<int>(id);
        leaf.depth = MAX_DEPTH;
        leaf.size = leaf_size;
        leaf.mass_total = MASS;
        leaf.mass_center = pos; // force uses mass_center instead of jittered position
        leaf.children.fill(-1);
        leafs.push_back(leaf);
    }

    // We now have to group the particles (currently we have only sorted the leaves),
    // but upwards subdivisions have to be created.
    // For grouping, store a nodes local index in its level.
    struct leaf
    {
        uint64_t leaf_code;
        int depth;
        int level_index;
    };

    std::vector<leaf> leaves;
    leaves.reserve(tree_levels[MAX_DEPTH].size());
    for (int i = 0; i < static_cast<int>(tree_levels[MAX_DEPTH].size()); ++i) {
        leaves.emplace_back(sort_container[static_cast<size_t>(i)].code, MAX_DEPTH, i);
    }

    // Build internal levels from MAX_DEPTH-1 to 0
    for (int current_depth = MAX_DEPTH - 1; current_depth >= 0; --current_depth) {
        auto& current_level = tree_levels[current_depth];
        current_level.clear();

        std::vector<leaf> next_refs;
        next_refs.reserve(leaves.size());

        const float parent_size = root_extent / static_cast<float>(1u << current_depth);

        size_t i = 0;
        while (i < leaves.size()) {
            const uint64_t key = path_to_ancestor(leaves[i].leaf_code, MAX_DEPTH, current_depth);

            size_t j = i + 1;
            while (j < leaves.size() && path_to_ancestor(leaves[j].leaf_code, MAX_DEPTH, current_depth) == key) {
                ++j;
            }

            const size_t group_size = j - i;

            // Unary compression: just carry the child ref upward unchanged.
            if (group_size == 1) {
                next_refs.push_back(leaves[i]);
                i = j;
                continue;
            }

            node parent;
            parent.leaf = false;
            parent.mass_id = -1;
            parent.depth = current_depth;
            parent.size = parent_size;
            parent.children.fill(-1);

            float mass_total = 0.0f;
            Vector3 mass_center_acc = Vector3Zero();

            for (size_t k = i; k < j; ++k) {
                const int child_depth = leaves[k].depth;
                const int child_local = leaves[k].level_index;

                // Read the child from its actual stored level.
                const node& child = tree_levels[child_depth][child_local];

                // Which octant of this parent does it belong to?
                // IMPORTANT: octant comes from the NEXT level after current_depth (i.e. current_depth+1),
                // but the child might skip levels due to compression.
                // We must use the child's "first level under the parent" which is (current_depth+1).
                const int oct = octant_at_level(leaves[k].leaf_code, current_depth + 1, MAX_DEPTH);

                // Store *global* child reference: we only have an int slot, so we need a single index space.
                parent.children[oct] = (child_depth << 24) | (child_local & 0x00FFFFFF);

                mass_total += child.mass_total;
                mass_center_acc = Vector3Add(mass_center_acc, Vector3Scale(child.mass_center, child.mass_total));
            }

            parent.mass_total = mass_total;
            parent.mass_center = (mass_total > 0.0f) ? Vector3Scale(mass_center_acc, 1.0f / mass_total) : Vector3Zero();

            const int parent_local = static_cast<int>(current_level.size());
            current_level.push_back(parent);

            next_refs.push_back({leaves[i].leaf_code, current_depth, parent_local});
            i = j;
        }

        leaves.swap(next_refs);
    }

    // Flatten levels and fix child indices from local->global.
    // All depth 0 nodes come first, then depth 1, last depth MAX_DEPTH.
    std::vector<int> offsets(tree_levels.size(), 0);
    int total = 0;
    for (int d = 0; d <= MAX_DEPTH; ++d) {
        offsets[d] = total;
        total += static_cast<int>(tree_levels[d].size());
    }

    t.nodes.clear();
    t.nodes.reserve(total);
    for (int d = 0; d <= MAX_DEPTH; ++d) {
        t.nodes.insert(t.nodes.end(), tree_levels[d].begin(), tree_levels[d].end());
    }

    // Fix child indices: convert local index in levels[d+1] to global index in t.nodes
    for (int d = 0; d <= MAX_DEPTH; ++d) {
        const int base = offsets[d];
        for (int i2 = 0; i2 < static_cast<int>(tree_levels[d].size()); ++i2) {
            node& n = t.nodes[base + i2];
            if (!n.leaf) {
                for (int c = 0; c < 8; ++c) {
                    int packed = n.children[c];
                    if (packed >= 0) {
                        const int child_depth = (packed >> 24) & 0xFF;
                        const int child_local = packed & 0x00FFFFFF;
                        n.children[c] = offsets[child_depth] + child_local;
                    }
                }
            }
        }
    }

    // const size_t _leaves = tree_levels[MAX_DEPTH].size();
    // const size_t _total = t.nodes.size();
    // const size_t _internal = _total - _leaves;
    // traceln("Morton octree nodes: {}, leaves: {}, ratio: {:.3} children per internal node.",
    //         _total,
    //         _leaves,
    //         static_cast<float>(_total - 1) / _internal);
}

auto octree::calculate_force_morton(const int node_idx, const Vector3& pos, const int self_id) const -> Vector3
{
    if (node_idx < 0) {
        return Vector3Zero();
    }

    // Force accumulator
    float fx = 0.0f;
    float fy = 0.0f;
    float fz = 0.0f;

    std::vector<int> stack;
    stack.reserve(128);
    stack.push_back(node_idx);

    constexpr float theta2 = THETA * THETA;

    while (!stack.empty()) {
        const int idx = stack.back();
        stack.pop_back();

        const node& n = nodes[idx];

        // No self-force for single-particle leafs
        if (n.leaf && n.mass_id == self_id) {
            continue;
        }

        const float dx = pos.x - n.mass_center.x;
        const float dy = pos.y - n.mass_center.y;
        const float dz = pos.z - n.mass_center.z;

        const float dist_sq = dx * dx + dy * dy + dz * dz + SOFTENING;

        // Barnes–Hut criterion
        if (n.leaf || ((n.size * n.size) / dist_sq) < theta2) {
            const float inv_dist = 1.0f / std::sqrt(dist_sq);
            const float force_mag = (BH_FORCE * n.mass_total) / dist_sq; // ~ 1/r^2
            const float s = force_mag * inv_dist;                        // scale by 1/r to get vector
            fx += dx * s;
            fy += dy * s;
            fz += dz * s;
            continue;
        }

        for (int c = 0; c < 8; ++c) {
            const int child = n.children[c];
            if (child >= 0) {
                stack.push_back(child);
            }
        }
    }

    return Vector3{fx, fy, fz};
}