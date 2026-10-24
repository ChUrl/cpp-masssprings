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

auto octree::memory_bytes() const -> size_t
{
    size_t total = nodes.capacity() * sizeof(node) + children.capacity() * sizeof(int) +
        order.capacity() * sizeof(u32) + snapshot.capacity() * sizeof(Vector3) +
        sort_scratch.capacity() * sizeof(u64) + sort_tmp.capacity() * sizeof(u64) +
        level_scratch.capacity() * sizeof(std::vector<node>) +
        leaves_scratch.capacity() * sizeof(leaf_ref) +
        next_refs_scratch.capacity() * sizeof(leaf_ref);

    for (const std::vector<node>& level : level_scratch) {
        total += level.capacity() * sizeof(node);
    }
    return total;
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

auto octree::compute_bounds(const std::vector<Vector3>& positions) -> std::pair<Vector3, Vector3>
{
    if (positions.empty()) {
        return {Vector3Zero(), Vector3Zero()};
    }

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
    return {root_min, root_max};
}

// Replaced the 50 line recursive octree insertion with this morton bitch to gain 5 UPS, FML
auto octree::build_octree_morton(octree& t, const std::vector<Vector3>& positions,
                                 const std::optional<BS::thread_pool<>*>& thread_pool,
                                 const simulation_settings& settings,
                                 morton_sort_input* precomputed) -> void
{
#ifdef TRACY
    ZoneScoped;
#endif

    t.clear();
    if (positions.empty()) {
        return;
    }

    t.depth = std::clamp(settings.max_depth, 6, MAX_DEPTH_LIMIT);
    t.theta = settings.theta;
    t.bh_force = settings.bh_force;
    t.softening = settings.softening;
    t.mass = settings.mass;
    const int depth = t.depth;

    // While the CPU pipeline is active, the same pool is saturated by the force pass. Letting
    // the tree build compete for it makes the radix sort much slower than a serial sort. With
    // GPU physics the pool is free, so the parallel build pays off.
    const bool parallel_tree = thread_pool.has_value() && settings.gpu_physics;

    // Keep the exact configuration the tree is built from; force queries use this snapshot.
    t.snapshot = positions;

    auto stage_start = std::chrono::high_resolution_clock::now();
    const auto mark_stage = [&](const int idx)
    {
        const auto now = std::chrono::high_resolution_clock::now();
        t.stage_ms[idx] = std::chrono::duration<float, std::milli>(now - stage_start).count();
        stage_start = now;
    };

    Vector3 root_min{};
    Vector3 root_max{};
    float root_extent = 0.0f;
    auto& sort_container = t.sort_scratch;

    if (precomputed != nullptr) {
        // The GPU already produced the exact Morton keys and bounds. The input buffer is
        // reused across rebuilds, so copy into the tree's scratch buffer instead of taking
        // ownership; the copy is cheaper than reallocating and first-touching ~16 MB every
        // rebuild on the 2M-mass graph.
        sort_container = precomputed->sorted;
        root_min = precomputed->root_min;
        root_max = precomputed->root_max;
        root_extent = precomputed->root_extent;
        t.stage_ms[0] = precomputed->bbox_ms;
        t.stage_ms[1] = precomputed->morton_ms;
        t.stage_ms[2] = precomputed->sort_ms;
        stage_start = std::chrono::high_resolution_clock::now();
    } else {
        const auto [computed_min, computed_max] = compute_bounds(positions);
        root_min = computed_min;
        root_max = computed_max;
        root_extent = root_max.x - root_min.x; // cubic
        mark_stage(0);

        // Container for building the particle list before sorting by morton code (reused across
        // builds)
        sort_container.resize(positions.size());

        const auto calculate_morton = [&](const u32 i)
        { sort_container[i] = (pos_to_morton(positions[i], root_min, root_max, depth) << 32) | i; };

        if (thread_pool) {
            (*thread_pool)
                ->submit_loop(0, positions.size(), calculate_morton, settings.small_task_block_size)
                .wait();
        } else {
            for (u32 i = 0; i < positions.size(); ++i) {
                calculate_morton(i);
            }
        }
        mark_stage(1);

        // Sort the list by morton codes. Because positions close to each other have similar
        // morton codes, this provides us with "spatial locality" in the datastructure.
        //
        // The code lives in the upper 32 bits, so the radix sort only needs to touch the 3 *
        // depth-dependent code bits. Both buffers are reused across builds. The histogram and
        // scatter passes are split into chunks so the sort can use the thread pool instead of
        // being a serial 4 ms bottleneck.
        {
            auto& sort_tmp = t.sort_tmp;
            sort_tmp.resize(sort_container.size());

            auto* src = &sort_container;
            auto* dst = &sort_tmp;

            if (parallel_tree) {
                const size_t count = sort_container.size();
                const int chunk_count = thread_pool && count >= 4096
                    ? std::min(16, static_cast<int>((*thread_pool)->get_thread_count()))
                    : 1;
                const auto chunk_begin = [&](const int chunk)
                { return count * static_cast<size_t>(chunk) / static_cast<size_t>(chunk_count); };
                const auto chunk_end = [&](const int chunk)
                {
                    return count * static_cast<size_t>(chunk + 1) /
                        static_cast<size_t>(chunk_count);
                };

                std::vector<std::array<u32, 1 << 12>> histograms(static_cast<size_t>(chunk_count));
                std::vector<std::array<u32, 1 << 12>> chunk_offsets(
                    static_cast<size_t>(chunk_count));

                for (int shift = 32; shift < 32 + 3 * depth; shift += 12) {
                    for (auto& histogram : histograms) {
                        histogram.fill(0);
                    }

                    const auto build_histogram = [&](const int chunk)
                    {
                        auto& histogram = histograms[static_cast<size_t>(chunk)];
                        for (size_t i = chunk_begin(chunk); i < chunk_end(chunk); ++i) {
                            ++histogram[((*src)[i] >> shift) & 0xFFFu];
                        }
                    };

                    if (thread_pool) {
                        (*thread_pool)->submit_loop(0, chunk_count, build_histogram).wait();
                    } else {
                        for (int chunk = 0; chunk < chunk_count; ++chunk) {
                            build_histogram(chunk);
                        }
                    }

                    std::array<u32, 1 << 12> prefix{};
                    u32 sum = 0;
                    for (size_t bucket = 0; bucket < prefix.size(); ++bucket) {
                        prefix[bucket] = sum;
                        for (const auto& histogram : histograms) {
                            sum += histogram[bucket];
                        }
                    }

                    std::array<u32, 1 << 12> running = prefix;
                    for (int chunk = 0; chunk < chunk_count; ++chunk) {
                        auto& offsets = chunk_offsets[static_cast<size_t>(chunk)];
                        offsets = running;
                        for (size_t bucket = 0; bucket < running.size(); ++bucket) {
                            running[bucket] += histograms[static_cast<size_t>(chunk)][bucket];
                        }
                    }

                    const auto scatter = [&](const int chunk)
                    {
                        auto& offsets = chunk_offsets[static_cast<size_t>(chunk)];
                        for (size_t i = chunk_begin(chunk); i < chunk_end(chunk); ++i) {
                            const u64 key = (*src)[i];
                            (*dst)[offsets[(key >> shift) & 0xFFFu]++] = key;
                        }
                    };

                    if (thread_pool) {
                        (*thread_pool)->submit_loop(0, chunk_count, scatter).wait();
                    } else {
                        for (int chunk = 0; chunk < chunk_count; ++chunk) {
                            scatter(chunk);
                        }
                    }

                    std::swap(src, dst);
                }
            } else {
                for (int shift = 32; shift < 32 + 3 * depth; shift += 12) {
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
            }

            // An odd number of passes leaves the result in the temporary buffer.
            if (src != &sort_container) {
                sort_container.swap(sort_tmp);
            }
        }
    }

    const auto duplicate_start = std::chrono::high_resolution_clock::now();

    // Resolve duplicate cells by bumping the code to the next free one. Multiple masses can
    // quantize to the same leaf cell (dense clusters, coincident positions), and every leaf
    // must have a unique code for the child slots of the internal nodes. Doing this on the
    // integer code instead of jittering the float position is exact and always terminates:
    // - the array stays sorted because code[i] only ever grows to code[i-1] + 1
    // - no re-sorting is required
    // - the force position (stored later from `positions`) is left untouched
    for (size_t i = 1; i < sort_container.size(); ++i) {
        const u64 previous_code = sort_container[i - 1] >> 32;
        if ((sort_container[i] >> 32) <= previous_code) {
            sort_container[i] = ((previous_code + 1) << 32) | static_cast<u32>(sort_container[i]);
        }
    }
    if (precomputed != nullptr) {
        t.stage_ms[2] += std::chrono::duration<float, std::milli>(
                             std::chrono::high_resolution_clock::now() - duplicate_start)
                             .count();
        stage_start = std::chrono::high_resolution_clock::now();
    } else {
        mark_stage(2);
    }

    auto& tree_levels = t.level_scratch;
    tree_levels.resize(depth + 1);
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
    t.children.reserve(sort_container.size() * 2);

    const float leaf_size = root_extent / static_cast<float>(1u << depth);
    const auto write_leaf = [&](const int i)
    {
        const u64 key = sort_container[static_cast<size_t>(i)];
        const u32 id = static_cast<u32>(key);
        const Vector3& pos = positions[id];

        node& leaf = t.nodes[static_cast<size_t>(i)];
        leaf.mass_id = static_cast<int>(id);
        leaf.size = leaf_size;
        leaf.mass_total = t.mass;
        leaf.mass_center = pos; // exact (unjittered) position
        leaf.first_child = -1;
        leaf.child_count = 0;

        t.order[static_cast<size_t>(i)] = id;
        leaves[static_cast<size_t>(i)] = {key >> 32, static_cast<u32>(i),
                                          (static_cast<u32>(i) << 4) | static_cast<u32>(depth)};
    };

    if (parallel_tree && sort_container.size() >= 4096) {
        (*thread_pool)
            ->submit_loop(0, static_cast<int>(sort_container.size()), write_leaf,
                          settings.small_task_block_size)
            .wait();
    } else {
        for (u32 i = 0; i < static_cast<u32>(sort_container.size()); ++i) {
            write_leaf(static_cast<int>(i));
        }
    }
    mark_stage(3);

    auto& next_refs = t.next_refs_scratch;

    // Build internal levels from depth-1 to 0. Each element's ancestor is computed exactly
    // once per level (the previous version computed it again for every comparison), and the
    // working vectors are reused across levels and frames.
    for (int current_depth = depth - 1; current_depth >= 0; --current_depth) {
        auto& current_level = tree_levels[current_depth];
        current_level.clear();
        next_refs.clear();
        next_refs.reserve(leaves.size());

        if (leaves.empty()) {
            break;
        }

        const float parent_size = root_extent / static_cast<float>(1u << current_depth);

        const auto child_node = [&](const leaf_ref& ref) -> const node&
        {
            return ref.depth() == depth
                ? t.nodes[static_cast<size_t>(ref.level_index())]
                : tree_levels[ref.depth()][static_cast<size_t>(ref.level_index())];
        };

        size_t group_begin = 0;
        u64 key = path_to_ancestor(leaves[0].code, depth, current_depth);
        for (size_t i = 1; i <= leaves.size(); ++i) {
            const u64 next_key = (i < leaves.size())
                ? path_to_ancestor(leaves[i].code, depth, current_depth)
                : ~0ull; // sentinel: cannot collide with a 3*depth bit key
            if (next_key == key) {
                continue;
            }

            const size_t group_size = i - group_begin;

            // Unary compression: just carry the child ref upward unchanged.
            if (group_size == 1) {
                next_refs.push_back(leaves[group_begin]);
            } else if (group_size > 1) {
                // The group's children already carry the centers and masses of their complete
                // subtrees, so the parent is their mass-weighted average. Re-summing every leaf
                // under the subtree would make this stage O(nodes * depth).
                const u32 first_leaf = leaves[group_begin].first_leaf;

                Vector3 center = Vector3Zero();
                float mass_total = 0.0f;
                for (size_t k = group_begin; k < i; ++k) {
                    const node& child = child_node(leaves[k]);
                    center.x += child.mass_center.x * child.mass_total;
                    center.y += child.mass_center.y * child.mass_total;
                    center.z += child.mass_center.z * child.mass_total;
                    mass_total += child.mass_total;
                }

                node parent;
                parent.mass_id = -1;
                parent.size = parent_size;
                parent.first_child = static_cast<int>(t.children.size());
                parent.child_count = static_cast<int>(group_size);
                parent.mass_total = mass_total;
                parent.mass_center = Vector3Scale(center, 1.0f / mass_total);

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

    // Flatten internal levels, deepest first.
    // Leaves already occupy [0, leaf_count). Append internal levels from deepest to the root;
    // leaves are never copied.
    std::array<int, MAX_DEPTH_LIMIT + 1> offsets{};
    offsets[depth] = 0;
    int total = static_cast<int>(t.nodes.size());
    for (int d = depth - 1; d >= 0; --d) {
        offsets[d] = total;
        total += static_cast<int>(tree_levels[d].size());
    }

    t.nodes.reserve(total);
    for (int d = depth - 1; d >= 0; --d) {
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
    // current path, so 1 + 7 * MAX_DEPTH_LIMIT entries are enough. Keep the stack on the
    // caller's stack: this function is called once per mass per physics update, and
    // a heap allocation here dominates large graphs.
    std::array<int, 1 + 7 * MAX_DEPTH_LIMIT> stack{};
    size_t stack_size = 0;
    stack[stack_size++] = node_idx;

    const float theta2 = theta * theta;

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

        const float dist_sq = dx * dx + dy * dy + dz * dz + softening;

        // Barnes–Hut criterion
        if (is_leaf || ((n.size * n.size) / dist_sq) < theta2) {
            const float inv_dist = 1.0f / std::sqrt(dist_sq);
            const float force_mag = (bh_force * n.mass_total) / dist_sq; // ~ 1/r^2
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
