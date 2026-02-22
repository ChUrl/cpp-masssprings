#ifndef __UTIL_HPP_
#define __UTIL_HPP_

#include <iostream>
#include <raylib.h>
#include <raymath.h>

inline std::ostream &operator<<(std::ostream &os, const Vector2 &v) {
  os << "(" << v.x << ", " << v.y << ")";
  return os;
}

inline std::ostream &operator<<(std::ostream &os, const Vector3 &v) {
  os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
  return os;
}

#endif
