#ifndef GPU_OCTREE_HPP_
#define GPU_OCTREE_HPP_

#include "settings.hpp"

#include <raylib.h>
#include <raymath.h>

#include <array>
#include <cstdint>
#include <string>

// GPU-resident octree built from the packed Morton order produced by gpu_physics::sort_morton.
// Leaves and internal nodes are built with compute shaders; no tree data is read back to the CPU
// during normal operation. Internal node order is arbitrary (the repulsion traversal only follows
// child indices), which lets the hierarchy use atomic hash tables instead of prefix scans.
class gpu_octree
{
private:
    unsigned int resolve_program = 0;
    unsigned int leaves_program = 0;
    unsigned int clear_program = 0;
    unsigned int insert_program = 0;
    unsigned int finalize_program = 0;
    unsigned int advance_program = 0;

    unsigned int nodes_buffer = 0;
    unsigned int children_buffer = 0;
    unsigned int order_buffer = 0;
    unsigned int refs_a_buffer = 0;
    unsigned int refs_b_buffer = 0;
    unsigned int hash_entries_buffer = 0;
    unsigned int records_buffer = 0;
    unsigned int counters_buffer = 0;

    size_t nodes_capacity = 0;
    size_t children_capacity = 0;
    size_t order_capacity = 0;
    size_t refs_a_capacity = 0;
    size_t refs_b_capacity = 0;
    size_t hash_capacity = 0;
    size_t records_capacity = 0;
    size_t counters_capacity = 0;

    int resolve_count = -1;
    int leaves_count = -1;
    int leaves_depth = -1;
    int leaves_extent = -1;
    int leaves_mass = -1;

    int clear_hash_size = -1;

    int insert_current_depth = -1;
    int insert_hash_mask = -1;

    int finalize_hash_size = -1;
    int finalize_current_depth = -1;
    int finalize_root_extent = -1;

    std::string error;

    size_t node_count_ = 0;
    size_t child_count_ = 0;
    int root_ = 0;
    Vector3 center_ = Vector3Zero();
    int depth_ = 8;
    std::array<float, 6> stage_ms_{};

    auto compile_program(const std::string& path, unsigned int& program) -> bool;
    auto ensure_buffer(unsigned int& buffer, size_t& capacity, size_t bytes) -> void;
    auto dispatch(unsigned int program, unsigned int count) -> void;

public:
    gpu_octree() = default;
    ~gpu_octree();

    gpu_octree(const gpu_octree&) = delete;
    auto operator=(const gpu_octree&) = delete;
    gpu_octree(gpu_octree&&) = delete;
    auto operator=(gpu_octree&&) = delete;

    auto init(const std::string& shader_dir) -> bool;
    auto shutdown() -> void;

    // Builds leaves + hierarchy. `sorted_buffer` is the packed uvec2 [mass_id, code] buffer from
    // gpu_physics::sort_morton; `positions_buffer` is the matching snapshot.
    auto build(unsigned int sorted_buffer, unsigned int positions_buffer, size_t mass_count,
               int depth, float root_extent, const simulation_settings& cfg) -> bool;

    [[nodiscard]] auto nodes() const -> unsigned int
    {
        return nodes_buffer;
    }
    [[nodiscard]] auto children() const -> unsigned int
    {
        return children_buffer;
    }
    [[nodiscard]] auto order() const -> unsigned int
    {
        return order_buffer;
    }
    [[nodiscard]] auto root() const -> int
    {
        return root_;
    }
    [[nodiscard]] auto node_count() const -> size_t
    {
        return node_count_;
    }
    [[nodiscard]] auto child_count() const -> size_t
    {
        return child_count_;
    }
    [[nodiscard]] auto center() const -> const Vector3&
    {
        return center_;
    }
    auto set_center(const Vector3& center) -> void
    {
        center_ = center;
    }
    [[nodiscard]] auto depth() const -> int
    {
        return depth_;
    }
    [[nodiscard]] auto stage_ms() const -> const std::array<float, 6>&
    {
        return stage_ms_;
    }
    [[nodiscard]] auto memory_bytes() const -> size_t
    {
        return nodes_capacity + children_capacity + order_capacity + refs_a_capacity +
            refs_b_capacity + hash_capacity + records_capacity + counters_capacity;
    }
    auto set_prefix_stages(float bbox_ms, float morton_ms, float sort_ms) -> void
    {
        stage_ms_[0] = bbox_ms;
        stage_ms_[1] = morton_ms;
        stage_ms_[2] = sort_ms;
    }
    [[nodiscard]] auto ok() const -> bool
    {
        return resolve_program != 0 && leaves_program != 0 && clear_program != 0 &&
            insert_program != 0 && finalize_program != 0 && advance_program != 0;
    }
    [[nodiscard]] auto error_message() const -> const std::string&
    {
        return error;
    }
};

#endif
