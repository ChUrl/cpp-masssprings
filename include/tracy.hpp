#ifndef __TRACY_HPP_
#define __TRACY_HPP_

#include "config.hpp"

#ifdef TRACY

#include <cstddef>

void *operator new(std::size_t count);
void operator delete(void *ptr) noexcept;
void operator delete(void *ptr, std::size_t count) noexcept;

#endif

#endif
