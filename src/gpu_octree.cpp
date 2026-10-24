#include "gpu_octree.hpp"

#include <GL/glew.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>

namespace
{
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

constexpr size_t node_bytes = 32;
constexpr size_t hash_entry_bytes = 16;
} // namespace

gpu_octree::~gpu_octree()
{
    shutdown();
}

auto gpu_octree::compile_program(const std::string& path, unsigned int& program) -> bool
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

auto gpu_octree::init(const std::string& shader_dir) -> bool
{
    shutdown();
    if (!compile_program(shader_dir + "/gpu_tree_resolve.comp", resolve_program) ||
        !compile_program(shader_dir + "/gpu_tree_leaves.comp", leaves_program) ||
        !compile_program(shader_dir + "/gpu_tree_clear.comp", clear_program) ||
        !compile_program(shader_dir + "/gpu_tree_insert.comp", insert_program) ||
        !compile_program(shader_dir + "/gpu_tree_finalize.comp", finalize_program) ||
        !compile_program(shader_dir + "/gpu_tree_advance.comp", advance_program)) {
        shutdown();
        return false;
    }

    resolve_count = glGetUniformLocation(resolve_program, "count");
    leaves_count = glGetUniformLocation(leaves_program, "count");
    leaves_depth = glGetUniformLocation(leaves_program, "depth");
    leaves_extent = glGetUniformLocation(leaves_program, "rootExtent");
    leaves_mass = glGetUniformLocation(leaves_program, "mass");

    clear_hash_size = glGetUniformLocation(clear_program, "hashSize");

    insert_current_depth = glGetUniformLocation(insert_program, "currentDepth");
    insert_hash_mask = glGetUniformLocation(insert_program, "hashMask");

    finalize_hash_size = glGetUniformLocation(finalize_program, "hashSize");
    finalize_current_depth = glGetUniformLocation(finalize_program, "currentDepth");
    finalize_root_extent = glGetUniformLocation(finalize_program, "rootExtent");

    if (resolve_count < 0 || leaves_count < 0 || leaves_depth < 0 || leaves_extent < 0 ||
        leaves_mass < 0 || clear_hash_size < 0 || insert_current_depth < 0 ||
        insert_hash_mask < 0 || finalize_hash_size < 0 || finalize_current_depth < 0 ||
        finalize_root_extent < 0) {
        error = "One or more GPU octree uniforms are missing";
        shutdown();
        return false;
    }

    glGenBuffers(1, &nodes_buffer);
    glGenBuffers(1, &children_buffer);
    glGenBuffers(1, &order_buffer);
    glGenBuffers(1, &refs_a_buffer);
    glGenBuffers(1, &refs_b_buffer);
    glGenBuffers(1, &hash_entries_buffer);
    glGenBuffers(1, &records_buffer);
    glGenBuffers(1, &counters_buffer);
    return true;
}

auto gpu_octree::shutdown() -> void
{
    const auto delete_buffer = [](unsigned int& buffer)
    {
        if (buffer != 0) {
            glDeleteBuffers(1, &buffer);
            buffer = 0;
        }
    };
    delete_buffer(nodes_buffer);
    delete_buffer(children_buffer);
    delete_buffer(order_buffer);
    delete_buffer(refs_a_buffer);
    delete_buffer(refs_b_buffer);
    delete_buffer(hash_entries_buffer);
    delete_buffer(records_buffer);
    delete_buffer(counters_buffer);

    const auto delete_program = [](unsigned int& program)
    {
        if (program != 0) {
            glDeleteProgram(program);
            program = 0;
        }
    };
    delete_program(resolve_program);
    delete_program(leaves_program);
    delete_program(clear_program);
    delete_program(insert_program);
    delete_program(finalize_program);
    delete_program(advance_program);

    nodes_capacity = 0;
    children_capacity = 0;
    order_capacity = 0;
    refs_a_capacity = 0;
    refs_b_capacity = 0;
    hash_capacity = 0;
    records_capacity = 0;
    counters_capacity = 0;
    node_count_ = 0;
    child_count_ = 0;
    root_ = 0;
}

auto gpu_octree::ensure_buffer(unsigned int& buffer, size_t& capacity, const size_t bytes) -> void
{
    if (bytes <= capacity) {
        return;
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    capacity = std::max<size_t>(bytes, sizeof(std::uint32_t));
    glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(capacity), nullptr,
                 GL_DYNAMIC_DRAW);
}

auto gpu_octree::dispatch(const unsigned int program, const unsigned int count) -> void
{
    glUseProgram(program);
    glDispatchCompute((count + 63) / 64, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

auto gpu_octree::build(const unsigned int sorted_buffer, const unsigned int positions_buffer,
                       const size_t mass_count, const int depth, const float root_extent,
                       const simulation_settings& cfg) -> bool
{
    if (!ok()) {
        error = "GPU octree is not initialized";
        return false;
    }
    if (mass_count == 0) {
        node_count_ = 0;
        child_count_ = 0;
        root_ = 0;
        center_ = Vector3Zero();
        return true;
    }

    const auto build_start = std::chrono::high_resolution_clock::now();
    size_t hash_size = 16;
    while (hash_size < mass_count * 2) {
        hash_size <<= 1;
    }
    const unsigned int ref_groups = static_cast<unsigned int>((mass_count + 63) / 64);

    ensure_buffer(nodes_buffer, nodes_capacity, mass_count * 2 * node_bytes);
    ensure_buffer(children_buffer, children_capacity, mass_count * 2 * sizeof(std::int32_t));
    ensure_buffer(order_buffer, order_capacity, mass_count * sizeof(std::uint32_t));
    ensure_buffer(refs_a_buffer, refs_a_capacity, mass_count * 4 * sizeof(std::uint32_t));
    ensure_buffer(refs_b_buffer, refs_b_capacity, mass_count * 4 * sizeof(std::uint32_t));
    ensure_buffer(hash_entries_buffer, hash_capacity, hash_size * hash_entry_bytes);
    ensure_buffer(records_buffer, records_capacity, mass_count * 4 * sizeof(std::uint32_t));
    ensure_buffer(counters_buffer, counters_capacity, 8 * sizeof(std::uint32_t));

    // counters: node_count, child_count, out_ref_count, record_count, root
    std::array<std::uint32_t, 8> counters{};
    counters[0] = static_cast<std::uint32_t>(mass_count);
    counters[5] = static_cast<std::uint32_t>(mass_count);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, counters_buffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(counters), counters.data());

    const auto leaves_start = std::chrono::high_resolution_clock::now();
    glUseProgram(resolve_program);
    glUniform1i(resolve_count, static_cast<int>(mass_count));
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, sorted_buffer);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glUseProgram(leaves_program);
    glUniform1i(leaves_count, static_cast<int>(mass_count));
    glUniform1i(leaves_depth, depth);
    glUniform1f(leaves_extent, root_extent);
    glUniform1f(leaves_mass, cfg.mass);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, sorted_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, positions_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, nodes_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, order_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, refs_a_buffer);
    dispatch(leaves_program, static_cast<unsigned int>(mass_count));
    stage_ms_[3] = std::chrono::duration<float, std::milli>(
                       std::chrono::high_resolution_clock::now() - leaves_start)
                       .count();

    if (std::getenv("MASS_SPRINGS_TREE_PROFILE") != nullptr) {
        const GLenum leaves_error = glGetError();
        std::array<std::uint32_t, 4> first_ref{};
        std::array<std::uint32_t, 1> first_order{};
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, refs_a_buffer);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(first_ref), first_ref.data());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, order_buffer);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(first_order), first_order.data());
        std::fprintf(stderr, "leaves: error=%u ref=(%u,%u,%u,%u) order=%u\n", leaves_error,
                     first_ref[0], first_ref[1], first_ref[2], first_ref[3], first_order[0]);
    }

    unsigned int refs_in = refs_a_buffer;
    unsigned int refs_out = refs_b_buffer;
    const auto levels_start = std::chrono::high_resolution_clock::now();

    for (int current_depth = depth - 1; current_depth >= 0; --current_depth) {
        // A level has at most 8^depth cells, so the hash table can shrink aggressively above
        // the leaf level without a CPU readback of the live reference count.
        const size_t cell_count = static_cast<size_t>(1) << std::min(3 * current_depth, 60);
        size_t level_hash_size = 16;
        while (level_hash_size < cell_count * 2) {
            level_hash_size <<= 1;
        }
        level_hash_size = std::min(level_hash_size, hash_size);
        const unsigned int hash_mask = static_cast<unsigned int>(level_hash_size - 1);
        const unsigned int hash_groups = static_cast<unsigned int>((level_hash_size + 127) / 128);

        glUseProgram(clear_program);
        glUniform1ui(clear_hash_size, static_cast<unsigned int>(level_hash_size));
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, hash_entries_buffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, counters_buffer);
        glDispatchCompute(hash_groups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glUseProgram(insert_program);
        glUniform1i(insert_current_depth, current_depth);
        glUniform1ui(insert_hash_mask, hash_mask);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, refs_in);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, hash_entries_buffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, records_buffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, counters_buffer);
        glDispatchCompute(ref_groups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glUseProgram(finalize_program);
        glUniform1ui(finalize_hash_size, static_cast<unsigned int>(level_hash_size));
        glUniform1i(finalize_current_depth, current_depth);
        glUniform1f(finalize_root_extent, root_extent);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, hash_entries_buffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, records_buffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, nodes_buffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, children_buffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, refs_out);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, counters_buffer);
        glDispatchCompute(hash_groups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glUseProgram(advance_program);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, counters_buffer);
        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        std::swap(refs_in, refs_out);
    }
    stage_ms_[4] = std::chrono::duration<float, std::milli>(
                       std::chrono::high_resolution_clock::now() - levels_start)
                       .count();

    const auto readback_start = std::chrono::high_resolution_clock::now();
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, counters_buffer);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(counters), counters.data());
    node_count_ = counters[0];
    child_count_ = counters[1];
    root_ = static_cast<int>(counters[4]);

    stage_ms_[4] += std::chrono::duration<float, std::milli>(
                        std::chrono::high_resolution_clock::now() - readback_start)
                        .count();
    stage_ms_[5] = 0.0f;
    depth_ = depth;

    const GLenum gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        error = "OpenGL error during GPU octree build: " + std::to_string(gl_error);
        return false;
    }
    (void)build_start;
    return true;
}
