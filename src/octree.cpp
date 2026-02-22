#include "octree.hpp"
#include "config.hpp"

#include <raymath.h>

auto Octree::CreateNode(const Vector3 &box_min, const Vector3 &box_max) -> int {
  OctreeNode node;
  node.box_min = box_min;
  node.box_max = box_max;
  nodes.push_back(node);

  return nodes.size() - 1;
}

auto Octree::GetOctant(int node_idx, const Vector3 &pos) -> int {
  OctreeNode &node = nodes[node_idx];
  Vector3 center = Vector3((node.box_min.x + node.box_max.x) / 2.0,
                           (node.box_min.y + node.box_max.y) / 2.0,
                           (node.box_min.z + node.box_max.z) / 2.0);

  // The octant is encoded as a 3-bit integer "zyx". The node area is split
  // along all 3 axes, if a position is right of an axis, this bit is set to 1.
  // If a position is right of the x-axis and y-axis and left of the z-axis, the
  // encoded octant is "011".
  int octant = 0;
  if (pos.x >= center.x) {
    octant |= 1;
  }
  if (pos.y >= center.y) {
    octant |= 2;
  }
  if (pos.z >= center.z) {
    octant |= 4;
  }

  return octant;
}

auto Octree::GetChildBounds(int node_idx, int octant)
    -> std::pair<Vector3, Vector3> {
  OctreeNode &node = nodes[node_idx];
  Vector3 center = Vector3((node.box_min.x + node.box_max.x) / 2.0,
                           (node.box_min.y + node.box_max.y) / 2.0,
                           (node.box_min.z + node.box_max.z) / 2.0);

  Vector3 min;
  Vector3 max;

  // If (octant & 1), the octant is to the right of the node region's x-axis
  // (see GetOctant). This means the left bound is the x-axis and the right
  // bound the node's region max.
  min.x = (octant & 1) ? center.x : node.box_min.x;
  max.x = (octant & 1) ? node.box_max.x : center.x;
  min.y = (octant & 2) ? center.y : node.box_min.y;
  max.y = (octant & 2) ? node.box_max.y : center.y;
  min.z = (octant & 4) ? center.z : node.box_min.z;
  max.z = (octant & 4) ? node.box_max.z : center.z;

  return std::make_pair(min, max);
}

auto Octree::Insert(int node_idx, int mass_id, const Vector3 &pos, float mass)
    -> void {
  OctreeNode &node = nodes[node_idx];

  if (node.leaf && node.mass_id == -1) {
    // We can place the particle in the empty leaf
    node.mass_id = mass_id;
    node.mass_center = pos;
    node.mass_total = mass;
    return;
  }

  if (node.leaf) {
    // The leaf is occupied, we need to subdivide
    int existing_id = node.mass_id;
    Vector3 existing_pos = node.mass_center;
    float existing_mass = node.mass_total;
    node.mass_id = -1;
    node.leaf = false;

    // Re-add the existing mass into a new empty leaf (see above)
    int oct = GetOctant(node_idx, existing_pos);
    if (node.children[oct] == -1) {
      auto [min, max] = GetChildBounds(node_idx, oct);
      node.children[oct] = CreateNode(min, max);
    }
    Insert(node.children[oct], existing_id, existing_pos, existing_mass);
  }

  // Insert the new mass
  int oct = GetOctant(node_idx, pos);
  if (nodes[node_idx].children[oct] == -1) {
    auto [min, max] = GetChildBounds(node_idx, oct);
    nodes[node_idx].children[oct] = CreateNode(min, max);
  }
  Insert(nodes[node_idx].children[oct], mass_id, pos, mass);

  // Update the center of mass
  node = nodes[node_idx];
  float new_mass = node.mass_total + mass;
  node.mass_center.x =
      (node.mass_center.x * node.mass_total + pos.x) / new_mass;
  node.mass_center.y =
      (node.mass_center.y * node.mass_total + pos.y) / new_mass;
  node.mass_center.z =
      (node.mass_center.z * node.mass_total + pos.z) / new_mass;
  node.mass_total = new_mass;
}

auto Octree::CalculateForce(int node_idx, const Vector3 &pos) -> Vector3 {
  if (node_idx < 0) {
    return Vector3Zero();
  }

  OctreeNode &node = nodes[node_idx];
  if (node.mass_total == 0.0f) {
    return Vector3Zero();
  }

  Vector3 diff = Vector3Subtract(pos, node.mass_center);
  float dist_sq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;

  // Softening
  dist_sq += SOFTENING;

  float size = node.box_max.x - node.box_min.x;

  // Barnes-Hut
  if (node.leaf || (size * size / dist_sq) < (THETA * THETA)) {
    float dist = std::sqrt(dist_sq);
    float force_mag = BH_FORCE * node.mass_total / dist_sq;

    return Vector3Scale(diff, force_mag / dist);
  }

  // Collect child forces
  Vector3 force = Vector3Zero();
  for (int i = 0; i < 8; ++i) {
    if (node.children[i] >= 0) {
      Vector3 child_force = CalculateForce(node.children[i], pos);

      force = Vector3Add(force, child_force);
    }
  }

  return force;
}
