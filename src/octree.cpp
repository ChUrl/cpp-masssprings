#include "octree.hpp"
#include "config.hpp"
#include "util.hpp"

#include <raymath.h>

#ifdef TRACY
#include <tracy/Tracy.hpp>
#endif

auto octree::node::child_count() const -> int
{
    int child_count = 0;
    for (const int child : children) {
        if (child != -1) {
            ++child_count;
        }
    }
    return child_count;
}

auto octree::create_empty_leaf(const Vector3& box_min, const Vector3& box_max) -> int
{
    node n;
    n.box_min = box_min;
    n.box_max = box_max;
    nodes.emplace_back(n);

    return static_cast<int>(nodes.size() - 1);
}

auto octree::get_octant(const int node_idx, const Vector3& pos) const -> int
{
    const node& n = nodes[node_idx];
    auto [cx, cy, cz] = Vector3((n.box_min.x + n.box_max.x) / 2.0f, (n.box_min.y + n.box_max.y) / 2.0f,
                                (n.box_min.z + n.box_max.z) / 2.0f);

    // The octant is encoded as a 3-bit integer "zyx". The node area is split
    // along all 3 axes, if a position is right of an axis, this bit is set to 1.
    // If a position is right of the x-axis and y-axis and left of the z-axis, the
    // encoded octant is "011".
    int octant = 0;
    if (pos.x >= cx) {
        octant |= 1;
    }
    if (pos.y >= cy) {
        octant |= 2;
    }
    if (pos.z >= cz) {
        octant |= 4;
    }

    return octant;
}

auto octree::get_child_bounds(const int node_idx, const int octant) const -> std::pair<Vector3, Vector3>
{
    const node& n = nodes[node_idx];
    auto [cx, cy, cz] = Vector3((n.box_min.x + n.box_max.x) / 2.0f, (n.box_min.y + n.box_max.y) / 2.0f,
                                (n.box_min.z + n.box_max.z) / 2.0f);

    Vector3 min = Vector3Zero();
    Vector3 max = Vector3Zero();

    // If (octant & 1), the octant is to the right of the node region's x-axis
    // (see GetOctant). This means the left bound is the x-axis and the right
    // bound the node's region max.
    min.x = octant & 1 ? cx : n.box_min.x;
    max.x = octant & 1 ? n.box_max.x : cx;
    min.y = octant & 2 ? cy : n.box_min.y;
    max.y = octant & 2 ? n.box_max.y : cy;
    min.z = octant & 4 ? cz : n.box_min.z;
    max.z = octant & 4 ? n.box_max.z : cz;

    return std::make_pair(min, max);
}

auto octree::insert(const int node_idx, const int mass_id, const Vector3& pos, const float mass,
                    const int depth) -> void
{
    // infoln("Inserting position ({}, {}, {}) into octree at node {} (depth {})", pos.x, pos.y,
    // pos.z, node_idx, depth);
    if (depth > MAX_DEPTH) {
        throw std::runtime_error(std::format("MAX_DEPTH! node={} box_min=({},{},{}) box_max=({},{},{}) pos=({},{},{})",
                                             node_idx, nodes[node_idx].box_min.x, nodes[node_idx].box_min.y,
                                             nodes[node_idx].box_min.z, nodes[node_idx].box_max.x,
                                             nodes[node_idx].box_max.y, nodes[node_idx].box_max.z, pos.x, pos.y,
                                             pos.z));
    }

    // NOTE: Do not store a nodes[node_idx] reference as the nodes vector might reallocate during
    // this function

    // We can place the particle in the empty leaf
    if (nodes[node_idx].leaf && nodes[node_idx].mass_id == -1) {
        nodes[node_idx].mass_id = mass_id;
        nodes[node_idx].mass_center = pos;
        nodes[node_idx].mass_total = mass;
        return;
    }

    // The leaf is occupied, we need to subdivide
    if (nodes[node_idx].leaf) {
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
        nodes[node_idx].mass_total = 0.0;
        nodes[node_idx].mass_center = Vector3Zero();

        // Re-insert the existing mass into a new empty leaf (see above)
        const int oct = get_octant(node_idx, existing_pos);
        if (nodes[node_idx].children[oct] == -1) {
            const auto& [min, max] = get_child_bounds(node_idx, oct);
            const int child_idx = create_empty_leaf(min, max);
            nodes[node_idx].children[oct] = child_idx;
        }
        insert(nodes[node_idx].children[oct], existing_id, existing_pos, existing_mass, depth + 1);
    }

    // Insert the new mass
    const int oct = get_octant(node_idx, pos);
    if (nodes[node_idx].children[oct] == -1) {
        const auto& [min, max] = get_child_bounds(node_idx, oct);
        const int child_idx = create_empty_leaf(min, max);
        nodes[node_idx].children[oct] = child_idx;
    }
    insert(nodes[node_idx].children[oct], mass_id, pos, mass, depth + 1);

    // Update the center of mass
    const float new_mass = nodes[node_idx].mass_total + mass;
    nodes[node_idx].mass_center.x = (nodes[node_idx].mass_center.x * nodes[node_idx].mass_total + pos.x) / new_mass;
    nodes[node_idx].mass_center.y = (nodes[node_idx].mass_center.y * nodes[node_idx].mass_total + pos.y) / new_mass;
    nodes[node_idx].mass_center.z = (nodes[node_idx].mass_center.z * nodes[node_idx].mass_total + pos.z) / new_mass;
    nodes[node_idx].mass_total = new_mass;
}

auto octree::calculate_force(const int node_idx, const Vector3& pos) const -> Vector3
{
    if (node_idx < 0) {
        return Vector3Zero();
    }

    const node& n = nodes[node_idx];
    if (std::abs(n.mass_total) <= 0.001f) {
        return Vector3Zero();
    }

    const Vector3 diff = Vector3Subtract(pos, n.mass_center);
    float dist_sq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;

    // Softening
    dist_sq += SOFTENING;

    // Barnes-Hut
    const float size = n.box_max.x - n.box_min.x;
    if (n.leaf || size * size / dist_sq < THETA * THETA) {
        const float dist = std::sqrt(dist_sq);
        const float force_mag = BH_FORCE * n.mass_total / dist_sq;

        return Vector3Scale(diff, force_mag / dist);
    }

    // Collect child forces
    Vector3 force = Vector3Zero();
    for (const int child : n.children) {
        if (child >= 0) {
            const Vector3 child_force = calculate_force(child, pos);

            force = Vector3Add(force, child_force);
        }
    }

    return force;
}