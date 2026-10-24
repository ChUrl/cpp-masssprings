#include "gpu_physics.hpp"
#include "cpu_spring_system.hpp"
#include "gpu_context.hpp"
#include "octree.hpp"

#include <GL/glew.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace
{
constexpr unsigned int sort_block_size = 1024;

struct sort_element
{
    std::uint32_t code_lo;
    std::uint32_t code_hi;
    std::uint32_t mass_id;
    std::uint32_t padding;
};
static_assert(sizeof(sort_element) == 16, "sort_element must match uvec4 in the shaders");

auto read_text_file(const std::string& path, std::string& text) -> bool
{
    std::ifstream file(path);
    if (!file) {
        return false;
    }

    std::ostringstream stream;
    stream << file.rdbuf();
    text = stream.str();
    return true;
}
} // namespace

gpu_physics::~gpu_physics()
{
    shutdown();
}

auto gpu_physics::compile_program(const std::string& path, unsigned int& program) -> bool
{
    std::string source;
    if (!read_text_file(path, source)) {
        error = "Could not read compute shader: " + path;
        return false;
    }

    const char* source_ptr = source.c_str();
    const GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &source_ptr, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<size_t>(std::max(1, length)), '\0');
        glGetShaderInfoLog(shader, length, nullptr, log.data());
        error = "Compute shader compilation failed (" + path + "):\n" + log;
        glDeleteShader(shader);
        return false;
    }

    program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);
    glDeleteShader(shader);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE) {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<size_t>(std::max(1, length)), '\0');
        glGetProgramInfoLog(program, length, nullptr, log.data());
        error = "Compute shader link failed (" + path + "):\n" + log;
        glDeleteProgram(program);
        program = 0;
        return false;
    }

    return true;
}

auto gpu_physics::init(const std::string& shader_dir, gpu_context* build_context) -> bool
{
    shutdown();

    if (!compile_program(shader_dir + "/clear_forces.comp", clear_program) ||
        !compile_program(shader_dir + "/springs.comp", springs_program) ||
        !compile_program(shader_dir + "/repulsion.comp", repulsion_program) ||
        !compile_program(shader_dir + "/integrate.comp", integrate_program) ||
        !compile_program(shader_dir + "/constraints.comp", constraints_program) ||
        !compile_program(shader_dir + "/morton.comp", morton_program) ||
        !compile_program(shader_dir + "/radix_histogram.comp", radix_histogram_program) ||
        !compile_program(shader_dir + "/radix_offsets.comp", radix_offsets_program) ||
        !compile_program(shader_dir + "/radix_scatter.comp", radix_scatter_program) ||
        !compile_program(shader_dir + "/radix_pack.comp", radix_pack_program)) {
        shutdown();
        return false;
    }

    clear_count = glGetUniformLocation(clear_program, "count");

    springs_count = glGetUniformLocation(springs_program, "count");
    springs_rest_length = glGetUniformLocation(springs_program, "restLength");
    springs_k = glGetUniformLocation(springs_program, "springK");
    springs_damping = glGetUniformLocation(springs_program, "dampeningK");

    repulsion_root = glGetUniformLocation(repulsion_program, "root");
    repulsion_count = glGetUniformLocation(repulsion_program, "count");
    repulsion_theta2 = glGetUniformLocation(repulsion_program, "theta2");
    repulsion_force = glGetUniformLocation(repulsion_program, "bhForce");
    repulsion_softening = glGetUniformLocation(repulsion_program, "softening");

    integrate_count = glGetUniformLocation(integrate_program, "count");
    integrate_dt = glGetUniformLocation(integrate_program, "dt");
    integrate_mass = glGetUniformLocation(integrate_program, "mass");
    integrate_damping = glGetUniformLocation(integrate_program, "verletDamping");
    integrate_max_step = glGetUniformLocation(integrate_program, "maxStep");

    constraints_offset = glGetUniformLocation(constraints_program, "offset");
    constraints_count = glGetUniformLocation(constraints_program, "count");
    constraints_rest_length = glGetUniformLocation(constraints_program, "restLength");
    constraints_relaxation = glGetUniformLocation(constraints_program, "relaxation");

    morton_count = glGetUniformLocation(morton_program, "count");
    morton_depth = glGetUniformLocation(morton_program, "depth");
    morton_root_min = glGetUniformLocation(morton_program, "rootMin");
    morton_root_max = glGetUniformLocation(morton_program, "rootMax");
    radix_histogram_count = glGetUniformLocation(radix_histogram_program, "count");
    radix_histogram_shift = glGetUniformLocation(radix_histogram_program, "shift");
    radix_offsets_block_count = glGetUniformLocation(radix_offsets_program, "blockCount");
    radix_scatter_count = glGetUniformLocation(radix_scatter_program, "count");
    radix_scatter_shift = glGetUniformLocation(radix_scatter_program, "shift");
    radix_pack_count = glGetUniformLocation(radix_pack_program, "count");

    if (clear_count < 0 || springs_count < 0 || springs_rest_length < 0 || springs_k < 0 ||
        springs_damping < 0 || repulsion_root < 0 || repulsion_count < 0 || repulsion_theta2 < 0 ||
        repulsion_force < 0 || repulsion_softening < 0 || integrate_count < 0 || integrate_dt < 0 ||
        integrate_mass < 0 || integrate_damping < 0 || integrate_max_step < 0 ||
        constraints_offset < 0 || constraints_count < 0 || constraints_rest_length < 0 ||
        constraints_relaxation < 0 || morton_count < 0 || morton_depth < 0 || morton_root_min < 0 ||
        morton_root_max < 0 || radix_histogram_count < 0 || radix_histogram_shift < 0 ||
        radix_offsets_block_count < 0 || radix_scatter_count < 0 || radix_scatter_shift < 0 ||
        radix_pack_count < 0) {
        error = "One or more GPU compute uniforms are missing";
        shutdown();
        return false;
    }

    glGenBuffers(1, &positions_buffer);
    glGenBuffers(1, &previous_buffer);
    glGenBuffers(1, &velocities_buffer);
    glGenBuffers(1, &forces_buffer);
    glGenBuffers(1, &queries_buffer);
    glGenBuffers(1, &nodes_buffer);
    glGenBuffers(1, &children_buffer);
    glGenBuffers(1, &order_buffer);
    glGenBuffers(1, &adj_offsets_buffer);
    glGenBuffers(1, &adj_neighbors_buffer);
    glGenBuffers(1, &springs_buffer);
    glGenBuffers(1, &colored_springs_buffer);
    glGenBuffers(1, &sort_positions_buffers[0]);
    glGenBuffers(1, &sort_positions_buffers[1]);
    glGenBuffers(1, &sort_elements_a_buffer);
    glGenBuffers(1, &sort_elements_b_buffer);
    glGenBuffers(1, &sort_histogram_buffer);
    glGenBuffers(1, &sort_offsets_buffer);
    glGenBuffers(1, &sort_packed_buffer);

    for (auto& tree : octrees_) {
        if (!tree.init(shader_dir)) {
            error = "Could not initialize GPU octree: " + tree.error_message();
            shutdown();
            return false;
        }
    }

    build_context_ = build_context;
    if (build_context_ != nullptr) {
        tree_worker_ = std::thread(&gpu_physics::tree_worker_loop, this);
    }
    return true;
}

auto gpu_physics::shutdown() -> void
{
    stop_tree_worker();

    const auto delete_buffer = [](unsigned int& buffer)
    {
        if (buffer != 0) {
            glDeleteBuffers(1, &buffer);
            buffer = 0;
        }
    };

    delete_buffer(positions_buffer);
    delete_buffer(previous_buffer);
    delete_buffer(velocities_buffer);
    delete_buffer(forces_buffer);
    delete_buffer(queries_buffer);
    delete_buffer(nodes_buffer);
    delete_buffer(children_buffer);
    delete_buffer(order_buffer);
    delete_buffer(adj_offsets_buffer);
    delete_buffer(adj_neighbors_buffer);
    delete_buffer(springs_buffer);
    delete_buffer(colored_springs_buffer);
    delete_buffer(sort_positions_buffers[0]);
    delete_buffer(sort_positions_buffers[1]);
    delete_buffer(sort_elements_a_buffer);
    delete_buffer(sort_elements_b_buffer);
    delete_buffer(sort_histogram_buffer);
    delete_buffer(sort_offsets_buffer);
    delete_buffer(sort_packed_buffer);

    const auto delete_program = [](unsigned int& program)
    {
        if (program != 0) {
            glDeleteProgram(program);
            program = 0;
        }
    };
    delete_program(clear_program);
    delete_program(springs_program);
    delete_program(repulsion_program);
    delete_program(integrate_program);
    delete_program(constraints_program);
    delete_program(morton_program);
    delete_program(radix_histogram_program);
    delete_program(radix_offsets_program);
    delete_program(radix_scatter_program);
    delete_program(radix_pack_program);
    for (auto& tree : octrees_) {
        tree.shutdown();
    }

    positions_capacity = 0;
    previous_capacity = 0;
    velocities_capacity = 0;
    forces_capacity = 0;
    queries_capacity = 0;
    nodes_capacity = 0;
    children_capacity = 0;
    order_capacity = 0;
    adj_offsets_capacity = 0;
    adj_neighbors_capacity = 0;
    springs_capacity = 0;
    colored_springs_capacity = 0;
    sort_positions_capacities[0] = 0;
    sort_positions_capacities[1] = 0;
    sort_elements_a_capacity = 0;
    sort_elements_b_capacity = 0;
    sort_histogram_capacity = 0;
    sort_offsets_capacity = 0;
    sort_packed_capacity = 0;
    uploaded_topology_version = static_cast<size_t>(-1);
    gpu_tree_ready_ = false;
    gpu_tree_count_ = 0;
    gpu_tree_build_ms_ = 0.0f;
    active_tree_slot_ = 0;
    build_tree_slot_ = 1;
    tree_result_ready_ = false;
    tree_result_ok_ = false;
    tree_request_pending_ = false;
    tree_build_running_ = false;
}

auto gpu_physics::memory_bytes() const -> size_t
{
    return positions_capacity + previous_capacity + velocities_capacity + forces_capacity +
        queries_capacity + nodes_capacity + children_capacity + order_capacity +
        adj_offsets_capacity + adj_neighbors_capacity + springs_capacity +
        colored_springs_capacity + sort_positions_capacities[0] + sort_positions_capacities[1] +
        sort_elements_a_capacity + sort_elements_b_capacity + sort_histogram_capacity +
        sort_offsets_capacity + sort_packed_capacity +
        adj_offsets.capacity() * sizeof(std::uint32_t) +
        adj_neighbors.capacity() * sizeof(std::uint32_t) +
        spring_pairs.capacity() * sizeof(std::uint32_t) +
        tree_request_positions_.capacity() * sizeof(Vector3);
}

auto gpu_physics::octree_memory_bytes() const -> size_t
{
    return octrees_[0].memory_bytes() + octrees_[1].memory_bytes();
}

auto gpu_physics::upload_buffer(unsigned int& buffer, size_t& capacity, const void* data,
                                const size_t bytes) -> void
{
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    if (bytes == 0) {
        if (capacity == 0) {
            capacity = sizeof(std::uint32_t);
            glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(capacity), nullptr,
                         GL_DYNAMIC_DRAW);
        }
        return;
    }
    if (bytes > capacity) {
        capacity = bytes;
        glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(bytes), data,
                     GL_DYNAMIC_DRAW);
    } else {
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(bytes), data);
    }
}

auto gpu_physics::upload_topology(cpu_spring_system& mass_springs) -> bool
{
    mass_springs.ensure_coloring();

    const size_t count = mass_springs.positions.size();
    const size_t spring_count = mass_springs.springs.size();

    adj_offsets.assign(count + 1, 0);
    for (const auto& [a, b] : mass_springs.springs) {
        ++adj_offsets[a + 1];
        ++adj_offsets[b + 1];
    }
    for (size_t i = 1; i < adj_offsets.size(); ++i) {
        adj_offsets[i] += adj_offsets[i - 1];
    }

    adj_neighbors.resize(spring_count * 2);
    std::vector<std::uint32_t> cursor = adj_offsets;
    for (const auto& [a, b] : mass_springs.springs) {
        adj_neighbors[cursor[a]++] = static_cast<std::uint32_t>(b);
        adj_neighbors[cursor[b]++] = static_cast<std::uint32_t>(a);
    }

    spring_pairs.resize(spring_count * 2);
    for (size_t i = 0; i < spring_count; ++i) {
        spring_pairs[2 * i] = static_cast<std::uint32_t>(mass_springs.springs[i].first);
        spring_pairs[2 * i + 1] = static_cast<std::uint32_t>(mass_springs.springs[i].second);
    }

    upload_buffer(adj_offsets_buffer, adj_offsets_capacity, adj_offsets.data(),
                  adj_offsets.size() * sizeof(std::uint32_t));
    upload_buffer(adj_neighbors_buffer, adj_neighbors_capacity, adj_neighbors.data(),
                  adj_neighbors.size() * sizeof(std::uint32_t));
    upload_buffer(springs_buffer, springs_capacity, spring_pairs.data(),
                  spring_pairs.size() * sizeof(std::uint32_t));
    upload_buffer(colored_springs_buffer, colored_springs_capacity,
                  mass_springs.colored_springs.data(),
                  mass_springs.colored_springs.size() * sizeof(std::uint32_t));

    uploaded_topology_version = mass_springs.topology_version;
    return true;
}

auto gpu_physics::dispatch(const unsigned int program, const unsigned int count) -> void
{
    glUseProgram(program);
    glDispatchCompute((count + 63) / 64, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

auto gpu_physics::dispatch_groups(const unsigned int program, const unsigned int groups) -> void
{
    glUseProgram(program);
    glDispatchCompute(groups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

auto gpu_physics::ensure_buffer(unsigned int& buffer, size_t& capacity, const size_t bytes) -> void
{
    if (bytes <= capacity) {
        return;
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    capacity = std::max<size_t>(bytes, sizeof(std::uint32_t));
    glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(capacity), nullptr,
                 GL_DYNAMIC_DRAW);
}

auto gpu_physics::sort_morton(const std::vector<Vector3>& positions, const int depth,
                              const Vector3& root_min, const Vector3& root_max,
                              std::vector<std::uint64_t>& sorted, float& morton_ms, float& sort_ms,
                              const bool download, const int positions_slot) -> bool
{
    morton_ms = 0.0f;
    sort_ms = 0.0f;

    if (morton_program == 0 || radix_histogram_program == 0 || radix_offsets_program == 0 ||
        radix_scatter_program == 0) {
        error = "GPU Morton sort is not initialized";
        return false;
    }
    const size_t count = positions.size();
    if (count == 0) {
        sorted.clear();
        return true;
    }
    if (depth < 1 || depth > 12 || positions_slot < 0 || positions_slot > 1) {
        error = "GPU Morton sort arguments are out of range";
        return false;
    }

    const unsigned int elements = static_cast<unsigned int>(count);
    const unsigned int blocks = (elements + sort_block_size - 1u) / sort_block_size;
    const size_t element_bytes = count * sizeof(sort_element);
    const size_t histogram_bytes = static_cast<size_t>(blocks) * 16u * sizeof(std::uint32_t);

    upload_buffer(sort_positions_buffers[static_cast<size_t>(positions_slot)],
                  sort_positions_capacities[static_cast<size_t>(positions_slot)], positions.data(),
                  count * sizeof(Vector3));
    ensure_buffer(sort_elements_a_buffer, sort_elements_a_capacity, element_bytes);
    ensure_buffer(sort_elements_b_buffer, sort_elements_b_capacity, element_bytes);
    ensure_buffer(sort_histogram_buffer, sort_histogram_capacity, histogram_bytes);
    ensure_buffer(sort_offsets_buffer, sort_offsets_capacity, histogram_bytes);

    const auto morton_start = std::chrono::high_resolution_clock::now();
    glUseProgram(morton_program);
    glUniform1i(morton_count, static_cast<int>(elements));
    glUniform1i(morton_depth, depth);
    glUniform3f(morton_root_min, root_min.x, root_min.y, root_min.z);
    glUniform3f(morton_root_max, root_max.x, root_max.y, root_max.z);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0,
                     sort_positions_buffers[static_cast<size_t>(positions_slot)]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, sort_elements_a_buffer);
    dispatch(morton_program, elements);
    if (download) {
        glFinish();
    }
    morton_ms = std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() -
                                                         morton_start)
                    .count();

    unsigned int src = sort_elements_a_buffer;
    unsigned int dst = sort_elements_b_buffer;
    // The CPU sort container packs the code into the upper 32 bits of a u64. Depths above 10
    // therefore truncate the highest Morton bits; sort the same 32 bits here so both builders
    // produce the same order.
    const int total_bits = std::min(3 * depth, 32);
    const auto radix_start = std::chrono::high_resolution_clock::now();
    const bool profile_sort = std::getenv("MASS_SPRINGS_SORT_PROFILE") != nullptr;
    float histogram_ms = 0.0f;
    float offsets_ms = 0.0f;
    float scatter_ms = 0.0f;

    for (int shift = 0; shift < total_bits; shift += 4) {
        if (profile_sort) {
            glFinish();
        }
        const auto stage_start = std::chrono::high_resolution_clock::now();
        glUseProgram(radix_histogram_program);
        glUniform1i(radix_histogram_count, static_cast<int>(elements));
        glUniform1i(radix_histogram_shift, shift);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, src);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, sort_histogram_buffer);
        dispatch_groups(radix_histogram_program, blocks);
        if (profile_sort) {
            glFinish();
            histogram_ms += std::chrono::duration<float, std::milli>(
                                std::chrono::high_resolution_clock::now() - stage_start)
                                .count();
        }

        const auto offsets_start = std::chrono::high_resolution_clock::now();
        glUseProgram(radix_offsets_program);
        glUniform1i(radix_offsets_block_count, static_cast<int>(blocks));
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, sort_histogram_buffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, sort_offsets_buffer);
        dispatch_groups(radix_offsets_program, 1);
        if (profile_sort) {
            glFinish();
            offsets_ms += std::chrono::duration<float, std::milli>(
                              std::chrono::high_resolution_clock::now() - offsets_start)
                              .count();
        }

        const auto scatter_start = std::chrono::high_resolution_clock::now();
        glUseProgram(radix_scatter_program);
        glUniform1i(radix_scatter_count, static_cast<int>(elements));
        glUniform1i(radix_scatter_shift, shift);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, src);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, dst);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, sort_offsets_buffer);
        dispatch_groups(radix_scatter_program, blocks);
        if (profile_sort) {
            glFinish();
            scatter_ms += std::chrono::duration<float, std::milli>(
                              std::chrono::high_resolution_clock::now() - scatter_start)
                              .count();
        }

        std::swap(src, dst);
    }

    const size_t packed_bytes = count * sizeof(std::uint64_t);
    ensure_buffer(sort_packed_buffer, sort_packed_capacity, packed_bytes);
    glUseProgram(radix_pack_program);
    glUniform1i(radix_pack_count, static_cast<int>(elements));
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, src);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, sort_packed_buffer);
    dispatch(radix_pack_program, elements);
    if (download || profile_sort) {
        glFinish();
    }

    float readback_ms = 0.0f;
    if (download) {
        sorted.resize(count);
        const auto readback_start = std::chrono::high_resolution_clock::now();
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, sort_packed_buffer);
        const auto* mapped = static_cast<const std::uint64_t*>(glMapBufferRange(
            GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(packed_bytes), GL_MAP_READ_BIT));
        if (mapped == nullptr) {
            error = "Could not map the GPU Morton sort result";
            return false;
        }
        std::copy(mapped, mapped + count, sorted.begin());
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        readback_ms = std::chrono::duration<float, std::milli>(
                          std::chrono::high_resolution_clock::now() - readback_start)
                          .count();
    }

    sort_ms = std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() -
                                                       radix_start)
                  .count();

    if (profile_sort) {
        std::fprintf(stderr,
                     "GPU sort profile: count=%zu blocks=%u hist=%.2fms offsets=%.2fms "
                     "scatter=%.2fms readback=%.2fms total=%.2fms\n",
                     count, blocks, histogram_ms, offsets_ms, scatter_ms, readback_ms, sort_ms);
    }

    const GLenum gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        error = "OpenGL error during GPU Morton sort: " + std::to_string(gl_error);
        return false;
    }
    return true;
}

auto gpu_physics::build_tree_impl(const std::vector<Vector3>& positions,
                                  const simulation_settings& cfg, const int slot) -> bool
{
    if (!ok() || positions.empty() || slot < 0 || slot > 1) {
        return false;
    }

    const auto bbox_start = std::chrono::high_resolution_clock::now();
    const auto [root_min, root_max] = octree::compute_bounds(positions);
    const float bbox_ms = std::chrono::duration<float, std::milli>(
                              std::chrono::high_resolution_clock::now() - bbox_start)
                              .count();

    const int depth = std::clamp(cfg.max_depth, 6, 12);
    std::vector<std::uint64_t> unused;
    float morton_ms = 0.0f;
    float sort_ms = 0.0f;
    const auto sort_call_start = std::chrono::high_resolution_clock::now();
    if (!sort_morton(positions, depth, root_min, root_max, unused, morton_ms, sort_ms, false,
                     slot)) {
        return false;
    }
    const float sort_call_ms = std::chrono::duration<float, std::milli>(
                                   std::chrono::high_resolution_clock::now() - sort_call_start)
                                   .count();

    auto& tree = octrees_[static_cast<size_t>(slot)];
    if (!tree.build(sort_packed_buffer, sort_positions_buffers[static_cast<size_t>(slot)],
                    positions.size(), depth, root_max.x - root_min.x, cfg)) {
        error = "GPU octree build failed: " + tree.error_message();
        return false;
    }
    const auto center_start = std::chrono::high_resolution_clock::now();
    Vector3 center = Vector3Zero();
    for (const Vector3& p : positions) {
        center = Vector3Add(center, p);
    }
    tree.set_center(Vector3Scale(center, 1.0f / static_cast<float>(positions.size())));
    const float center_ms = std::chrono::duration<float, std::milli>(
                                std::chrono::high_resolution_clock::now() - center_start)
                                .count();
    tree.set_prefix_stages(bbox_ms, morton_ms, sort_ms);
    if (std::getenv("MASS_SPRINGS_TREE_PROFILE") != nullptr) {
        std::fprintf(stderr, "build_tree: sort_call=%.2f (morton=%.2f sort=%.2f) center=%.2f\n",
                     sort_call_ms, morton_ms, sort_ms, center_ms);
    }
    return true;
}

auto gpu_physics::wait_tree_build() -> void
{
    std::unique_lock<std::mutex> lock(tree_mutex_);
    tree_cv_.wait(lock, [&] { return !tree_build_running_ && !tree_request_pending_; });
    tree_result_ready_ = false;
}

auto gpu_physics::build_tree(const std::vector<Vector3>& positions, const simulation_settings& cfg)
    -> bool
{
    if (positions.empty()) {
        gpu_tree_ready_ = false;
        return true;
    }

    wait_tree_build();
    const int target = gpu_tree_ready_ ? 1 - active_tree_slot_ : 0;
    const auto build_start = std::chrono::high_resolution_clock::now();
    if (!build_tree_impl(positions, cfg, target)) {
        return false;
    }
    active_tree_slot_ = target;
    gpu_tree_count_ = positions.size();
    gpu_tree_build_ms_ = std::chrono::duration<float, std::milli>(
                             std::chrono::high_resolution_clock::now() - build_start)
                             .count();
    gpu_tree_ready_ = true;
    return true;
}

auto gpu_physics::request_tree_build(const std::vector<Vector3>& positions,
                                     const simulation_settings& cfg) -> bool
{
    if (build_context_ == nullptr || positions.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(tree_mutex_);
    if (tree_stop_) {
        return false;
    }
    // A result waiting to be consumed must be activated before its target buffer can be reused.
    if (tree_result_ready_ || tree_build_running_) {
        return false;
    }
    const int target = gpu_tree_ready_ ? 1 - active_tree_slot_ : active_tree_slot_;

    tree_request_positions_ = positions;
    tree_request_cfg_ = cfg;
    tree_request_slot_ = target;
    tree_request_pending_ = true;
    tree_cv_.notify_one();
    return true;
}

auto gpu_physics::poll_tree_build() -> bool
{
    std::unique_lock<std::mutex> lock(tree_mutex_);
    if (!tree_result_ready_) {
        return false;
    }
    tree_result_ready_ = false;
    if (!tree_result_ok_) {
        lock.unlock();
        return false;
    }
    active_tree_slot_ = tree_result_slot_;
    gpu_tree_count_ = tree_result_count_;
    gpu_tree_build_ms_ = tree_result_build_ms_;
    gpu_tree_ready_ = true;
    lock.unlock();
    return true;
}

auto gpu_physics::tree_worker_loop() -> void
{
    if (build_context_ == nullptr || !build_context_->make_current()) {
        return;
    }

    while (true) {
        std::vector<Vector3> positions;
        simulation_settings cfg;
        int slot = 0;
        {
            std::unique_lock<std::mutex> lock(tree_mutex_);
            tree_cv_.wait(lock, [&] { return tree_stop_ || tree_request_pending_; });
            if (tree_stop_) {
                break;
            }
            positions = std::move(tree_request_positions_);
            cfg = tree_request_cfg_;
            slot = tree_request_slot_;
            tree_request_positions_.clear();
            tree_request_pending_ = false;
            tree_build_running_ = true;
        }

        const auto start = std::chrono::high_resolution_clock::now();
        const bool ok = build_tree_impl(positions, cfg, slot);
        const float build_ms = std::chrono::duration<float, std::milli>(
                                   std::chrono::high_resolution_clock::now() - start)
                                   .count();

        {
            std::lock_guard<std::mutex> lock(tree_mutex_);
            tree_result_ok_ = ok;
            tree_result_slot_ = slot;
            tree_result_ready_ = true;
            tree_result_count_ = positions.size();
            tree_result_build_ms_ = build_ms;
            tree_build_running_ = false;
        }
        tree_cv_.notify_all();
    }

    build_context_->release_current();
}

auto gpu_physics::stop_tree_worker() -> void
{
    if (tree_worker_.joinable()) {
        {
            std::lock_guard<std::mutex> lock(tree_mutex_);
            tree_stop_ = true;
        }
        tree_cv_.notify_all();
        tree_worker_.join();
    }
    build_context_ = nullptr;
}

auto gpu_physics::step_gpu_tree(cpu_spring_system& mass_springs, const simulation_settings& cfg,
                                const float dt, const bool solve_constraints) -> bool
{
    if (!ok() || !gpu_tree_ready_) {
        error = "GPU octree is not ready";
        return false;
    }

    const size_t count = mass_springs.positions.size();
    if (count == 0) {
        return true;
    }
    if (gpu_tree_count_ != count) {
        error = "GPU octree does not match the graph size";
        return false;
    }
    if (mass_springs.previous_positions.size() != count ||
        mass_springs.velocities.size() != count) {
        error = "GPU physics velocity arrays do not match the graph";
        return false;
    }

    if (uploaded_topology_version != mass_springs.topology_version) {
        if (!upload_topology(mass_springs)) {
            return false;
        }
        dynamic_state_dirty = true;
    }
    if (dynamic_state_dirty) {
        upload_buffer(positions_buffer, positions_capacity, mass_springs.positions.data(),
                      count * sizeof(Vector3));
        upload_buffer(previous_buffer, previous_capacity, mass_springs.previous_positions.data(),
                      count * sizeof(Vector3));
        upload_buffer(velocities_buffer, velocities_capacity, mass_springs.velocities.data(),
                      count * sizeof(Vector3));
        dynamic_state_dirty = false;
    }

    const size_t force_bytes = count * sizeof(Vector3);
    if (force_bytes > forces_capacity) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, forces_buffer);
        forces_capacity = force_bytes;
        glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(force_bytes), nullptr,
                     GL_DYNAMIC_DRAW);
    }

    glUseProgram(clear_program);
    glUniform1i(clear_count, static_cast<int>(count));
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, forces_buffer);
    dispatch(clear_program, static_cast<unsigned int>(count));

    glUseProgram(springs_program);
    glUniform1i(springs_count, static_cast<int>(count));
    glUniform1f(springs_rest_length, cfg.rest_length);
    glUniform1f(springs_k, cfg.spring_k);
    glUniform1f(springs_damping, cfg.dampening_k);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, forces_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, positions_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, velocities_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, adj_offsets_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, adj_neighbors_buffer);
    dispatch(springs_program, static_cast<unsigned int>(count));

    const auto& active_tree = octrees_[static_cast<size_t>(active_tree_slot_)];
    glUseProgram(repulsion_program);
    glUniform1i(repulsion_root, active_tree.root());
    glUniform1i(repulsion_count, static_cast<int>(count));
    glUniform1f(repulsion_theta2, cfg.theta * cfg.theta);
    glUniform1f(repulsion_force, cfg.bh_force);
    glUniform1f(repulsion_softening, cfg.softening);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, forces_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1,
                     sort_positions_buffers[static_cast<size_t>(active_tree_slot_)]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, active_tree.nodes());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, active_tree.children());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, active_tree.order());
    dispatch(repulsion_program, static_cast<unsigned int>(count));

    glUseProgram(integrate_program);
    glUniform1i(integrate_count, static_cast<int>(count));
    glUniform1f(integrate_dt, dt);
    glUniform1f(integrate_mass, cfg.mass);
    glUniform1f(integrate_damping, cfg.verlet_dampening);
    glUniform1f(integrate_max_step, cfg.max_step);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, positions_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, previous_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, velocities_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, forces_buffer);
    dispatch(integrate_program, static_cast<unsigned int>(count));

    if (solve_constraints && !mass_springs.springs.empty()) {
        glUseProgram(constraints_program);
        glUniform1f(constraints_rest_length, cfg.rest_length);
        glUniform1f(constraints_relaxation, cfg.constraint_relaxation);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, positions_buffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, springs_buffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, colored_springs_buffer);
        for (int iteration = 0; iteration < cfg.constraint_iterations; ++iteration) {
            for (std::uint32_t color = 0; color <= mass_springs.max_spring_color; ++color) {
                const std::uint32_t begin = mass_springs.color_offsets[color];
                const std::uint32_t end = mass_springs.color_offsets[color + 1];
                if (begin == end) {
                    continue;
                }
                glUniform1ui(constraints_offset, begin);
                glUniform1ui(constraints_count, end - begin);
                dispatch(constraints_program, end - begin);
            }
        }
    }

    glUseProgram(0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, positions_buffer);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                       static_cast<GLsizeiptr>(count * sizeof(Vector3)),
                       mass_springs.positions.data());

    const GLenum gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        error = "OpenGL error during GPU octree physics: " + std::to_string(gl_error);
        return false;
    }
    return true;
}

auto gpu_physics::mark_state_dirty() -> void
{
    dynamic_state_dirty = true;
}

auto gpu_physics::sync_to_cpu(cpu_spring_system& mass_springs) -> bool
{
    const size_t count = mass_springs.positions.size();
    if (count == 0 || positions_capacity == 0) {
        return true;
    }
    if (mass_springs.previous_positions.size() != count ||
        mass_springs.velocities.size() != count) {
        error = "GPU physics velocity arrays do not match the graph";
        return false;
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, positions_buffer);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                       static_cast<GLsizeiptr>(count * sizeof(Vector3)),
                       mass_springs.positions.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, previous_buffer);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                       static_cast<GLsizeiptr>(count * sizeof(Vector3)),
                       mass_springs.previous_positions.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, velocities_buffer);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                       static_cast<GLsizeiptr>(count * sizeof(Vector3)),
                       mass_springs.velocities.data());

    const GLenum gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        error = "OpenGL error while syncing GPU state: " + std::to_string(gl_error);
        return false;
    }
    return true;
}

auto gpu_physics::step(cpu_spring_system& mass_springs, const octree& tree,
                       const simulation_settings& cfg, const float dt, const bool solve_constraints)
    -> bool
{
    if (!ok()) {
        error = "GPU physics is not initialized";
        return false;
    }

    const size_t count = mass_springs.positions.size();
    if (count == 0) {
        return true;
    }
    if (tree.snapshot.size() != count || tree.order.size() != count || tree.nodes.empty()) {
        error = "GPU physics state does not match the graph";
        return false;
    }
    if (mass_springs.previous_positions.size() != count ||
        mass_springs.velocities.size() != count) {
        error = "GPU physics velocity arrays do not match the graph";
        return false;
    }

    if (uploaded_topology_version != mass_springs.topology_version) {
        if (!upload_topology(mass_springs)) {
            return false;
        }
        dynamic_state_dirty = true;
    }

    if (dynamic_state_dirty) {
        upload_buffer(positions_buffer, positions_capacity, mass_springs.positions.data(),
                      count * sizeof(Vector3));
        upload_buffer(previous_buffer, previous_capacity, mass_springs.previous_positions.data(),
                      count * sizeof(Vector3));
        upload_buffer(velocities_buffer, velocities_capacity, mass_springs.velocities.data(),
                      count * sizeof(Vector3));
        dynamic_state_dirty = false;
    }
    upload_buffer(queries_buffer, queries_capacity, tree.snapshot.data(),
                  tree.snapshot.size() * sizeof(Vector3));
    upload_buffer(nodes_buffer, nodes_capacity, tree.nodes.data(),
                  tree.nodes.size() * sizeof(octree::node));
    upload_buffer(children_buffer, children_capacity, tree.children.data(),
                  tree.children.size() * sizeof(int));
    upload_buffer(order_buffer, order_capacity, tree.order.data(),
                  tree.order.size() * sizeof(std::uint32_t));

    const size_t force_bytes = count * sizeof(Vector3);
    if (force_bytes > forces_capacity) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, forces_buffer);
        forces_capacity = force_bytes;
        glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(force_bytes), nullptr,
                     GL_DYNAMIC_DRAW);
    }

    // Clear forces.
    glUseProgram(clear_program);
    glUniform1i(clear_count, static_cast<int>(count));
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, forces_buffer);
    dispatch(clear_program, static_cast<unsigned int>(count));

    // Spring forces. One invocation per mass; each mass sums its incident edges, so no atomics
    // are needed and the result is deterministic.
    glUseProgram(springs_program);
    glUniform1i(springs_count, static_cast<int>(count));
    glUniform1f(springs_rest_length, cfg.rest_length);
    glUniform1f(springs_k, cfg.spring_k);
    glUniform1f(springs_damping, cfg.dampening_k);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, forces_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, positions_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, velocities_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, adj_offsets_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, adj_neighbors_buffer);
    dispatch(springs_program, static_cast<unsigned int>(count));

    // Barnes-Hut repulsion.
    glUseProgram(repulsion_program);
    glUniform1i(repulsion_root, tree.root_idx);
    glUniform1i(repulsion_count, static_cast<int>(count));
    glUniform1f(repulsion_theta2, tree.theta * tree.theta);
    glUniform1f(repulsion_force, tree.bh_force);
    glUniform1f(repulsion_softening, tree.softening);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, forces_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, queries_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, nodes_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, children_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, order_buffer);
    dispatch(repulsion_program, static_cast<unsigned int>(count));

    // Verlet integration.
    glUseProgram(integrate_program);
    glUniform1i(integrate_count, static_cast<int>(count));
    glUniform1f(integrate_dt, dt);
    glUniform1f(integrate_mass, cfg.mass);
    glUniform1f(integrate_damping, cfg.verlet_dampening);
    glUniform1f(integrate_max_step, cfg.max_step);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, positions_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, previous_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, velocities_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, forces_buffer);
    dispatch(integrate_program, static_cast<unsigned int>(count));

    // Distance constraints. The edge coloring guarantees that no two edges in one dispatch share
    // a mass, so the position updates are race-free.
    if (solve_constraints && !mass_springs.springs.empty()) {
        glUseProgram(constraints_program);
        glUniform1f(constraints_rest_length, cfg.rest_length);
        glUniform1f(constraints_relaxation, cfg.constraint_relaxation);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, positions_buffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, springs_buffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, colored_springs_buffer);

        for (int iteration = 0; iteration < cfg.constraint_iterations; ++iteration) {
            for (std::uint32_t color = 0; color <= mass_springs.max_spring_color; ++color) {
                const std::uint32_t begin = mass_springs.color_offsets[color];
                const std::uint32_t end = mass_springs.color_offsets[color + 1];
                if (begin == end) {
                    continue;
                }
                glUniform1ui(constraints_offset, begin);
                glUniform1ui(constraints_count, end - begin);
                dispatch(constraints_program, end - begin);
            }
        }
    }

    glUseProgram(0);

    // Read back the state needed by the CPU tree builder, the renderer and CPU fallback.
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, positions_buffer);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                       static_cast<GLsizeiptr>(count * sizeof(Vector3)),
                       mass_springs.positions.data());

    const GLenum gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        error = "OpenGL error during GPU physics: " + std::to_string(gl_error);
        return false;
    }
    return true;
}
