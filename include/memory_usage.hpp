#ifndef MEMORY_USAGE_HPP_
#define MEMORY_USAGE_HPP_

#include <cstddef>

/**
 * Portable estimate of the application's own tracked allocations. This intentionally excludes
 * raylib/GLFW/OpenGL driver memory, allocator overhead, thread stacks and the OS process footprint.
 */
struct memory_usage
{
    size_t search = 0;
    size_t model = 0;
    size_t graph = 0;
    size_t results = 0;

    size_t physics_cpu = 0;
    size_t physics_octree = 0;
    size_t physics_readback = 0;
    size_t gpu_physics = 0;
    size_t gpu_octree = 0;

    size_t renderer_cpu = 0;
    size_t renderer_gpu = 0;

    [[nodiscard]] auto cpu_total() const -> size_t
    {
        return search + model + graph + results + physics_cpu + physics_octree + physics_readback +
            renderer_cpu;
    }

    [[nodiscard]] auto gpu_total() const -> size_t
    {
        return gpu_physics + gpu_octree + renderer_gpu;
    }

    [[nodiscard]] auto total() const -> size_t
    {
        return cpu_total() + gpu_total();
    }
};

#endif
