#ifndef GPU_PHYSICS_HPP_
#define GPU_PHYSICS_HPP_

#include "gpu_octree.hpp"
#include "settings.hpp"

#include <raylib.h>

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class cpu_spring_system;
class gpu_context;
class octree;

// OpenGL 4.3 compute backend. This class is only used on the physics thread, with the hidden
// context created by gpu_context current.
class gpu_physics
{
private:
    unsigned int clear_program = 0;
    unsigned int springs_program = 0;
    unsigned int repulsion_program = 0;
    unsigned int integrate_program = 0;
    unsigned int constraints_program = 0;

    unsigned int morton_program = 0;
    unsigned int radix_histogram_program = 0;
    unsigned int radix_offsets_program = 0;
    unsigned int radix_scatter_program = 0;
    unsigned int radix_pack_program = 0;

    unsigned int positions_buffer = 0;
    unsigned int previous_buffer = 0;
    unsigned int velocities_buffer = 0;
    unsigned int forces_buffer = 0;
    unsigned int queries_buffer = 0;
    unsigned int nodes_buffer = 0;
    unsigned int children_buffer = 0;
    unsigned int order_buffer = 0;
    unsigned int adj_offsets_buffer = 0;
    unsigned int adj_neighbors_buffer = 0;
    unsigned int springs_buffer = 0;
    unsigned int colored_springs_buffer = 0;

    std::array<unsigned int, 2> sort_positions_buffers{};
    unsigned int sort_elements_a_buffer = 0;
    unsigned int sort_elements_b_buffer = 0;
    unsigned int sort_histogram_buffer = 0;
    unsigned int sort_offsets_buffer = 0;
    unsigned int sort_packed_buffer = 0;

    size_t positions_capacity = 0;
    size_t previous_capacity = 0;
    size_t velocities_capacity = 0;
    size_t forces_capacity = 0;
    size_t queries_capacity = 0;
    size_t nodes_capacity = 0;
    size_t children_capacity = 0;
    size_t order_capacity = 0;
    size_t adj_offsets_capacity = 0;
    size_t adj_neighbors_capacity = 0;
    size_t springs_capacity = 0;
    size_t colored_springs_capacity = 0;
    std::array<size_t, 2> sort_positions_capacities{};
    size_t sort_elements_a_capacity = 0;
    size_t sort_elements_b_capacity = 0;
    size_t sort_histogram_capacity = 0;
    size_t sort_offsets_capacity = 0;
    size_t sort_packed_capacity = 0;

    int clear_count = -1;
    int springs_count = -1;
    int springs_rest_length = -1;
    int springs_k = -1;
    int springs_damping = -1;
    int repulsion_root = -1;
    int repulsion_count = -1;
    int repulsion_theta2 = -1;
    int repulsion_force = -1;
    int repulsion_softening = -1;
    int integrate_count = -1;
    int integrate_dt = -1;
    int integrate_mass = -1;
    int integrate_damping = -1;
    int integrate_max_step = -1;
    int constraints_offset = -1;
    int constraints_count = -1;
    int constraints_rest_length = -1;
    int constraints_relaxation = -1;

    int morton_count = -1;
    int morton_depth = -1;
    int morton_root_min = -1;
    int morton_root_max = -1;
    int radix_histogram_count = -1;
    int radix_histogram_shift = -1;
    int radix_offsets_block_count = -1;
    int radix_scatter_count = -1;
    int radix_scatter_shift = -1;
    int radix_pack_count = -1;

    std::vector<std::uint32_t> adj_offsets;
    std::vector<std::uint32_t> adj_neighbors;
    std::vector<std::uint32_t> spring_pairs;
    size_t uploaded_topology_version = static_cast<size_t>(-1);
    bool dynamic_state_dirty = true;

    std::array<gpu_octree, 2> octrees_{};
    int active_tree_slot_ = 0;
    int build_tree_slot_ = 1;
    bool gpu_tree_ready_ = false;
    size_t gpu_tree_count_ = 0;
    float gpu_tree_build_ms_ = 0.0f;

    gpu_context* build_context_ = nullptr;
    std::thread tree_worker_;
    std::mutex tree_mutex_;
    std::condition_variable tree_cv_;
    bool tree_stop_ = false;
    bool tree_request_pending_ = false;
    bool tree_build_running_ = false;
    bool tree_result_ready_ = false;
    bool tree_result_ok_ = false;
    int tree_result_slot_ = -1;
    size_t tree_result_count_ = 0;
    float tree_result_build_ms_ = 0.0f;
    std::vector<Vector3> tree_request_positions_;
    simulation_settings tree_request_cfg_{};
    int tree_request_slot_ = -1;

    auto tree_worker_loop() -> void;
    auto build_tree_impl(const std::vector<Vector3>& positions, const simulation_settings& cfg,
                         int slot) -> bool;
    auto wait_tree_build() -> void;
    auto stop_tree_worker() -> void;

    std::string error;

    auto compile_program(const std::string& path, unsigned int& program) -> bool;
    auto upload_buffer(unsigned int& buffer, size_t& capacity, const void* data, size_t bytes)
        -> void;
    auto upload_topology(cpu_spring_system& mass_springs) -> bool;
    auto dispatch(unsigned int program, unsigned int count) -> void;
    auto dispatch_groups(unsigned int program, unsigned int groups) -> void;
    auto ensure_buffer(unsigned int& buffer, size_t& capacity, size_t bytes) -> void;

public:
    gpu_physics() = default;
    ~gpu_physics();

    gpu_physics(const gpu_physics&) = delete;
    auto operator=(const gpu_physics&) -> gpu_physics& = delete;
    gpu_physics(gpu_physics&&) = delete;
    auto operator=(gpu_physics&&) -> gpu_physics& = delete;

    auto init(const std::string& shader_dir = "shader/compute",
              gpu_context* build_context = nullptr) -> bool;
    auto shutdown() -> void;

    // Estimated bytes of CPU-side topology buffers and GPU shader-storage buffers.
    [[nodiscard]] auto memory_bytes() const -> size_t;
    [[nodiscard]] auto octree_memory_bytes() const -> size_t;

    // Runs clear, spring forces, Barnes-Hut repulsion, integration, and optionally the colored
    // distance constraints. CPU positions/velocities are read back at the end.
    auto step(cpu_spring_system& mass_springs, const octree& tree, const simulation_settings& cfg,
              float dt, bool solve_constraints) -> bool;

    // Stage 1 of the GPU octree build: compute Morton codes from positions and stable-sort them
    // with a 4-bit LSD radix sort. The returned keys use the same (code << 32) | mass_id layout as
    // the CPU builder. Times are measured with glFinish so they are useful in the perf trace.
    auto sort_morton(const std::vector<Vector3>& positions, int depth, const Vector3& root_min,
                     const Vector3& root_max, std::vector<std::uint64_t>& sorted, float& morton_ms,
                     float& sort_ms, bool download = true, int positions_slot = 0) -> bool;

    // Builds the complete octree on the GPU. The sorted keys stay in GPU memory; only the root
    // index, node/child counts and root center are read back.
    auto build_tree(const std::vector<Vector3>& positions, const simulation_settings& cfg) -> bool;
    auto request_tree_build(const std::vector<Vector3>& positions, const simulation_settings& cfg)
        -> bool;
    auto poll_tree_build() -> bool;
    [[nodiscard]] auto gpu_tree_ready() const -> bool
    {
        return gpu_tree_ready_;
    }
    [[nodiscard]] auto gpu_tree_matches(size_t count) const -> bool
    {
        return gpu_tree_ready_ && gpu_tree_count_ == count;
    }
    [[nodiscard]] auto gpu_tree_nodes() const -> size_t
    {
        return octrees_[static_cast<size_t>(active_tree_slot_)].node_count();
    }
    [[nodiscard]] auto gpu_tree_root() const -> int
    {
        return octrees_[static_cast<size_t>(active_tree_slot_)].root();
    }
    [[nodiscard]] auto gpu_tree_children() const -> size_t
    {
        return octrees_[static_cast<size_t>(active_tree_slot_)].child_count();
    }
    [[nodiscard]] auto gpu_tree_center() const -> const Vector3&
    {
        return octrees_[static_cast<size_t>(active_tree_slot_)].center();
    }
    [[nodiscard]] auto gpu_tree_stages() const -> const std::array<float, 6>&
    {
        return octrees_[static_cast<size_t>(active_tree_slot_)].stage_ms();
    }
    [[nodiscard]] auto gpu_tree_build_ms() const -> float
    {
        return gpu_tree_build_ms_;
    }
    [[nodiscard]] auto gpu_tree_async_available() const -> bool
    {
        return build_context_ != nullptr;
    }

    // Runs the force/integration pipeline against the GPU-resident tree.
    auto step_gpu_tree(cpu_spring_system& mass_springs, const simulation_settings& cfg, float dt,
                       bool solve_constraints) -> bool;

    // Marks the CPU-side positions/velocities as newer than the GPU copy. Used when switching
    // from the CPU pipeline or after graph commands.
    auto mark_state_dirty() -> void;
    // Reads all dynamic state back. Used before switching from GPU to the CPU pipeline.
    auto sync_to_cpu(cpu_spring_system& mass_springs) -> bool;

    [[nodiscard]] auto ok() const -> bool
    {
        return clear_program != 0 && springs_program != 0 && repulsion_program != 0 &&
            integrate_program != 0 && constraints_program != 0;
    }

    [[nodiscard]] auto error_message() const -> const std::string&
    {
        return error;
    }
};

#endif
