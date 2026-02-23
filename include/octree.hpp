#ifndef __OCTREE_HPP_
#define __OCTREE_HPP_

#include <raylib.h>
#include <raymath.h>
#include <vector>

class OctreeNode {
public:
  Vector3 mass_center;
  float mass_total;
  Vector3 box_min; // area start
  Vector3 box_max; // area end
  int children[8];
  int mass_id;
  bool leaf;

public:
  OctreeNode()
      : mass_center(Vector3Zero()), mass_total(0.0),
        children(-1, -1, -1, -1, -1, -1, -1, -1), mass_id(-1), leaf(true) {}

public:
  auto ChildCount() const -> int;
};

class Octree {
public:
  std::vector<OctreeNode> nodes;

public:
  Octree() {}

  Octree(const Octree &copy) = delete;
  Octree &operator=(const Octree &copy) = delete;
  Octree(Octree &&move) = delete;
  Octree &operator=(Octree &&move) = delete;

public:
  auto CreateNode(const Vector3 &box_min, const Vector3 &box_max) -> int;

  auto GetOctant(int node_idx, const Vector3 &pos) -> int;

  auto GetChildBounds(int node_idx, int octant) -> std::pair<Vector3, Vector3>;

  auto Insert(int node_idx, int mass_id, const Vector3 &pos, float mass)
      -> void;

  auto CalculateForce(int node_idx, const Vector3 &pos) const -> Vector3;

  auto Print() const -> void;
};

#endif
