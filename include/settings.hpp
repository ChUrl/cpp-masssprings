#ifndef SETTINGS_HPP_
#define SETTINGS_HPP_

#include <mutex>
#include <string>

// All tunable runtime settings. The struct is trivially copyable so the UI can publish a whole
// new snapshot atomically and the physics/render threads can read a consistent copy per step.
struct simulation_settings
{
    // Default values live in src/default_settings.cpp. Change them there.
    simulation_settings();

    // Barnes-Hut / octree
    float theta;
    float bh_force;
    float softening;
    int max_depth;        // 6..12, structural: next tree rebuild uses it
    int rebuild_interval; // >= 1
    bool async_octree;    // applied on the next physics step
    bool gpu_physics;     // OpenGL compute pipeline; applied on the next physics step
    bool gpu_tree_sort;   // experimental: GPU-resident octree (leaves + hierarchy)

    // Integration / global motion
    float sim_speed;
    float target_ups;
    float verlet_dampening;
    float mass;
    float max_step;

    // Links / constraints
    float spring_k;
    float dampening_k;
    float rest_length;
    int constraint_iterations;
    float constraint_relaxation;
    int constraint_interval;
    float spawn_spread; // initial explosion offset as a fraction of rest_length
    bool planar_graph;  // constrain the layout to the XY plane and use a 2D camera

    // Camera
    float fov;
    float camera_distance;
    float camera_smooth_speed;
    float zoom_speed;
    float fov_speed;
    float pan_speed;
    float rot_speed;

    // Rendering
    float vertex_size;
    float solution_node_size; // green: winning/path nodes
    float visited_node_size;  // orange: visited/start nodes
    bool draw_nodes;
    bool draw_edges;
    int edge_stride; // draw every Nth transition (edge LOD)
    bool billboard_nodes;
    bool show_labels;
    bool show_debug;
    bool show_performance_overlay;
    bool show_memory_overlay;

    // Logging
    int log_level; // 0 = off, 1 = info, 2 = detailed

    // Threading / performance
    int worker_threads; // 0 = auto (hardware_concurrency - 2), applied at startup
    int search_threads; // 0 = auto (hardware_concurrency - 2) for exploration jobs
    int small_task_block_size;
    int large_task_block_size;
};

// Current settings snapshot. Written by the UI, read by the physics/render threads. Access goes
// through the functions below so a whole struct is copied under the lock.
extern std::mutex g_settings_mutex;
extern simulation_settings g_settings;

[[nodiscard]] auto get_settings() -> simulation_settings;
auto set_settings(const simulation_settings& settings) -> void;

[[nodiscard]] auto default_settings() -> simulation_settings;
auto load_settings(const std::string& path = "settings.ini") -> void;
auto save_settings(const std::string& path = "settings.ini") -> void;

// Number of worker threads in the physics thread pool that is actually running.
auto set_active_worker_threads(int count) -> void;
[[nodiscard]] auto active_worker_threads() -> int;

#endif
