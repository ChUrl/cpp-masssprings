#ifndef SETTINGS_HPP_
#define SETTINGS_HPP_

#include <mutex>
#include <string>

// All tunable runtime settings. The struct is trivially copyable so the UI can publish a whole
// new snapshot atomically and the physics/render threads can read a consistent copy per step.
struct simulation_settings
{
    // Barnes-Hut / octree
    float theta = 1.6f;
    float bh_force = 3.0f;
    float softening = 0.05f;
    int max_depth = 8;          // 6..12, structural: next tree rebuild uses it
    int rebuild_interval = 1;   // >= 1
    bool async_octree = true;   // currently compile-time; restart required
    bool gpu_repulsion = false; // not implemented yet

    // Integration / global motion
    float sim_speed = 4.0f;
    float target_ups = 90.0f;
    float verlet_dampening = 0.05f;
    float mass = 1.0f;
    float max_step = 1.0f;

    // Links / constraints
    float spring_k = 1.0f;
    float dampening_k = 0.5f;
    float rest_length = 3.0f;
    int constraint_iterations = 1;
    float constraint_relaxation = 0.8f;
    int constraint_interval = 1;
    float spawn_spread = 0.1f; // initial explosion offset as a fraction of rest_length

    // Camera
    float fov = 90.0f;
    float camera_distance = 150.0f;
    float camera_smooth_speed = 15.0f;
    float zoom_speed = 2.5f;
    float fov_speed = 1.0f;
    float pan_speed = 2.0f;
    float rot_speed = 1.0f;

    // Rendering
    float vertex_size = 0.75f;
    bool draw_edges = true;
    int edge_stride = 1; // draw every Nth transition (edge LOD)
    bool billboard_nodes = false;
    bool show_labels = true;
    bool show_debug = true;

    // Logging
    int log_level = 0; // 0 = off, 1 = info, 2 = detailed

    // Threading / performance
    int worker_threads = 0; // 0 = auto (hardware_concurrency - 2), applied at startup
    int small_task_block_size = 256;
    int large_task_block_size = 64;
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

#endif
