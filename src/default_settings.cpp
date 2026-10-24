#include "settings.hpp"

// Single place for every default value. Edit this constructor to change the reset defaults.
// The member order must match simulation_settings in include/settings.hpp.
simulation_settings::simulation_settings()
    : theta(1.6f), bh_force(3.0f), softening(0.05f), max_depth(8), rebuild_interval(3),
      async_octree(true), gpu_physics(true), gpu_tree_sort(true), sim_speed(4.0f),
      target_ups(90.0f), verlet_dampening(0.05f), mass(1.0f), max_step(1.0f), spring_k(1.0f),
      dampening_k(0.5f), rest_length(3.0f), constraint_iterations(1), constraint_relaxation(0.8f),
      constraint_interval(2), spawn_spread(0.1f), planar_graph(false), fov(90.0f),
      camera_distance(150.0f), camera_smooth_speed(15.0f), zoom_speed(2.5f), fov_speed(1.0f),
      pan_speed(2.0f), rot_speed(1.0f), vertex_size(0.75f), solution_node_size(1.5f),
      visited_node_size(1.5f), draw_nodes(true), draw_edges(true), edge_stride(1),
      billboard_nodes(true), show_labels(true), show_debug(true), show_performance_overlay(true),
      show_memory_overlay(true), log_level(0), worker_threads(0), search_threads(0),
      small_task_block_size(256), large_task_block_size(64)
{}

auto default_settings() -> simulation_settings
{
    return simulation_settings{};
}
