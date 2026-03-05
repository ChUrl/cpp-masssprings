#include "octree.hpp"
#include "config.hpp"

#include <cfloat>
#include <raymath.h>

auto octree::clear() -> void
{
    nodes.clear();

    // mass_centers.clear();
    // mass_totals.clear();
    // box_mins.clear();
    // box_maxs.clear();
    // childrens.clear();
    // mass_ids.clear();
    // leafs.clear();
}

auto octree::reserve(const size_t count) -> void
{
    nodes.reserve(count);

    // mass_centers.reserve(count);
    // mass_totals.reserve(count);
    // box_mins.reserve(count);
    // box_maxs.reserve(count);
    // childrens.reserve(count);
    // mass_ids.reserve(count);
    // leafs.reserve(count);
}

auto octree::empty() const -> bool
{
    return nodes.empty();

    // return mass_centers.empty();
}

auto octree::get_child_count(const int node_idx) const -> int
{
    int child_count = 0;
    for (const int child : nodes[node_idx].children) {
        if (child != -1) {
            ++child_count;
        }
    }
    return child_count;
}

auto octree::insert(const int node_idx,
                    const int mass_id,
                    const Vector3& pos,
                    const float mass,
                    const int depth) -> void
{
    if (depth > MAX_DEPTH) {
        throw std::runtime_error("Octree insertion reachead max depth");
    }

    // NOTE: Do not store a nodes[node_idx] reference as the nodes vector might reallocate during
    // this function

    // We can place the particle in the empty leaf
    const bool leaf = nodes[node_idx].leaf;
    if (leaf && nodes[node_idx].mass_id == -1) {
        nodes[node_idx].mass_id = mass_id;
        nodes[node_idx].mass_center = pos;
        nodes[node_idx].mass_total = mass;
        return;
    }

    const Vector3& box_min = nodes[node_idx].box_min;
    const Vector3& box_max = nodes[node_idx].box_max;

    // The leaf is occupied, we need to subdivide
    if (leaf) {
        const int existing_id = nodes[node_idx].mass_id;
        const Vector3 existing_pos = nodes[node_idx].mass_center;
        const float existing_mass = nodes[node_idx].mass_total;

        // If positions are identical we jitter the particles
        const Vector3 diff = Vector3Subtract(pos, existing_pos);
        if (diff == Vector3Zero()) {
            // warnln("Trying to insert an identical partical into octree (jittering position)");

            Vector3 jittered = pos;
            jittered.x += 0.001;
            jittered.y += 0.001;
            insert(node_idx, mass_id, jittered, mass, depth);
            return;

            // Could also merge them, but that leads to the octree having less leafs than we have
            // masses nodes[node_idx].mass_total += mass; return;
        }

        // Convert the leaf to an internal node
        nodes[node_idx].mass_id = -1;
        nodes[node_idx].leaf = false;
        nodes[node_idx].mass_center = Vector3Zero();
        nodes[node_idx].mass_total = 0.0;

        // Re-insert the existing mass into a new empty leaf (see above)
        const int oct = get_octant(box_min, box_max, existing_pos);
        if (nodes[node_idx].children[oct] == -1) {
            const auto& [min, max] = get_child_bounds(box_min, box_max, oct);
            const int child_idx = create_empty_leaf(min, max);
            nodes[node_idx].children[oct] = child_idx;
        }
        insert(nodes[node_idx].children[oct], existing_id, existing_pos, existing_mass, depth + 1);
    }

    // Insert the new mass
    const int oct = get_octant(box_min, box_max, pos);
    if (nodes[node_idx].children[oct] == -1) {
        const auto& [min, max] = get_child_bounds(box_min, box_max, oct);
        const int child_idx = create_empty_leaf(min, max);
        nodes[node_idx].children[oct] = child_idx;
    }
    insert(nodes[node_idx].children[oct], mass_id, pos, mass, depth + 1);

    // Update the center of mass
    const float new_mass = nodes[node_idx].mass_total + mass;
    nodes[node_idx].mass_center = (nodes[node_idx].mass_center * nodes[node_idx].mass_total + pos) / new_mass;
    nodes[node_idx].mass_total = new_mass;
}

auto octree::build_octree(octree& t, const std::vector<Vector3>& positions) -> void
{
    #ifdef TRACY
    ZoneScoped;
    #endif

    t.clear();
    t.reserve(positions.size() * 2);

    // Compute bounding box around all masses
    Vector3 min{FLT_MAX, FLT_MAX, FLT_MAX};
    Vector3 max{-FLT_MAX, -FLT_MAX, -FLT_MAX};
    for (const auto& [x, y, z] : positions) {
        min.x = std::min(min.x, x);
        max.x = std::max(max.x, x);
        min.y = std::min(min.y, y);
        max.y = std::max(max.y, y);
        min.z = std::min(min.z, z);
        max.z = std::max(max.z, z);
    }

    // Pad the bounding box
    constexpr float pad = 1.0;
    min = Vector3Subtract(min, Vector3Scale(Vector3One(), pad));
    max = Vector3Add(max, Vector3Scale(Vector3One(), pad));

    // Make it cubic (so subdivisions are balanced)
    const float max_extent = std::max({max.x - min.x, max.y - min.y, max.z - min.z});
    max = Vector3Add(min, Vector3Scale(Vector3One(), max_extent));

    // Root node spans the entire area
    const int root = t.create_empty_leaf(min, max);

    for (size_t i = 0; i < positions.size(); ++i) {
        t.insert(root, static_cast<int>(i), positions[i], MASS, 0);
    }
}

auto octree::calculate_force(const int node_idx, const Vector3& pos) const -> Vector3
{
    if (node_idx < 0) {
        return Vector3Zero();
    }

    const node& n = nodes[node_idx];

    const float mass_total = n.mass_total;
    const Vector3& mass_center = n.mass_center;
    const Vector3& box_min = n.box_min;
    const Vector3& box_max = n.box_max;
    const std::array<int, 8>& children = n.children;
    const bool leaf = n.leaf;

    if (std::abs(mass_total) <= 0.001f) {
        return Vector3Zero();
    }

    const Vector3 diff = Vector3Subtract(pos, mass_center);
    float dist_sq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;

    // Softening
    dist_sq += SOFTENING;

    // Barnes-Hut
    const float size = box_max.x - box_min.x;
    if (leaf || size * size / dist_sq < THETA * THETA) {
        const float dist = std::sqrt(dist_sq);
        const float force_mag = BH_FORCE * mass_total / dist_sq;

        return Vector3Scale(diff, force_mag / dist);
    }

    // Collect child forces
    Vector3 force = Vector3Zero();
    for (const int child : children) {
        if (child >= 0) {
            const Vector3 child_force = calculate_force(child, pos);

            force = Vector3Add(force, child_force);
        }
    }

    return force;
}