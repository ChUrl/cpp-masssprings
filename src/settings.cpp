#include "settings.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace
{
auto trim(std::string value) -> std::string
{
    const auto not_space = [](const unsigned char c) { return std::isspace(c) == 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

auto parse_bool(const std::string& value) -> bool
{
    return value == "1" || value == "true" || value == "on";
}
} // namespace

std::mutex g_settings_mutex;
simulation_settings g_settings;
std::atomic<int> g_log_level{0};

auto get_settings() -> simulation_settings
{
    std::lock_guard<std::mutex> lock(g_settings_mutex);
    return g_settings;
}

auto set_settings(const simulation_settings& settings) -> void
{
    {
        std::lock_guard<std::mutex> lock(g_settings_mutex);
        g_settings = settings;
    }
    g_log_level.store(settings.log_level, std::memory_order_relaxed);
}

auto default_settings() -> simulation_settings
{
    return simulation_settings{};
}

auto load_settings(const std::string& path) -> void
{
    std::ifstream file(path);
    if (!file) {
        return;
    }

    simulation_settings s;
    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        const auto equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }

        const std::string key = trim(line.substr(0, equals));
        const std::string value = trim(line.substr(equals + 1));

        try {
            if (key == "theta")
                s.theta = std::stof(value);
            else if (key == "bh_force")
                s.bh_force = std::stof(value);
            else if (key == "softening")
                s.softening = std::stof(value);
            else if (key == "max_depth")
                s.max_depth = std::stoi(value);
            else if (key == "rebuild_interval")
                s.rebuild_interval = std::stoi(value);
            else if (key == "async_octree")
                s.async_octree = parse_bool(value);
            else if (key == "gpu_repulsion")
                s.gpu_repulsion = parse_bool(value);
            else if (key == "sim_speed")
                s.sim_speed = std::stof(value);
            else if (key == "target_ups")
                s.target_ups = std::stof(value);
            else if (key == "verlet_dampening")
                s.verlet_dampening = std::stof(value);
            else if (key == "mass")
                s.mass = std::stof(value);
            else if (key == "max_step")
                s.max_step = std::stof(value);
            else if (key == "spring_k")
                s.spring_k = std::stof(value);
            else if (key == "dampening_k")
                s.dampening_k = std::stof(value);
            else if (key == "rest_length")
                s.rest_length = std::stof(value);
            else if (key == "constraint_iterations")
                s.constraint_iterations = std::stoi(value);
            else if (key == "constraint_relaxation")
                s.constraint_relaxation = std::stof(value);
            else if (key == "constraint_interval")
                s.constraint_interval = std::stoi(value);
            else if (key == "spawn_spread")
                s.spawn_spread = std::stof(value);
            else if (key == "fov")
                s.fov = std::stof(value);
            else if (key == "camera_distance")
                s.camera_distance = std::stof(value);
            else if (key == "camera_smooth_speed")
                s.camera_smooth_speed = std::stof(value);
            else if (key == "zoom_speed")
                s.zoom_speed = std::stof(value);
            else if (key == "fov_speed")
                s.fov_speed = std::stof(value);
            else if (key == "pan_speed")
                s.pan_speed = std::stof(value);
            else if (key == "rot_speed")
                s.rot_speed = std::stof(value);
            else if (key == "vertex_size")
                s.vertex_size = std::stof(value);
            else if (key == "draw_edges")
                s.draw_edges = parse_bool(value);
            else if (key == "edge_stride")
                s.edge_stride = std::stoi(value);
            else if (key == "billboard_nodes")
                s.billboard_nodes = parse_bool(value);
            else if (key == "show_labels")
                s.show_labels = parse_bool(value);
            else if (key == "show_debug")
                s.show_debug = parse_bool(value);
            else if (key == "log_level")
                s.log_level = std::clamp(std::stoi(value), 0, 2);
            else if (key == "worker_threads")
                s.worker_threads = std::stoi(value);
            else if (key == "small_task_block_size")
                s.small_task_block_size = std::stoi(value);
            else if (key == "large_task_block_size")
                s.large_task_block_size = std::stoi(value);
        }
        catch (const std::exception&) {
            // Ignore malformed values and keep the default.
        }
    }

    set_settings(s);
}

auto save_settings(const std::string& path) -> void
{
    const simulation_settings s = get_settings();

    std::ofstream file(path);
    if (!file) {
        return;
    }

    file << std::setprecision(6);
    file << "# MassSprings settings\n";
    file << "theta=" << s.theta << '\n';
    file << "bh_force=" << s.bh_force << '\n';
    file << "softening=" << s.softening << '\n';
    file << "max_depth=" << s.max_depth << '\n';
    file << "rebuild_interval=" << s.rebuild_interval << '\n';
    file << "async_octree=" << (s.async_octree ? 1 : 0) << '\n';
    file << "gpu_repulsion=" << (s.gpu_repulsion ? 1 : 0) << '\n';
    file << "sim_speed=" << s.sim_speed << '\n';
    file << "target_ups=" << s.target_ups << '\n';
    file << "verlet_dampening=" << s.verlet_dampening << '\n';
    file << "mass=" << s.mass << '\n';
    file << "max_step=" << s.max_step << '\n';
    file << "spring_k=" << s.spring_k << '\n';
    file << "dampening_k=" << s.dampening_k << '\n';
    file << "rest_length=" << s.rest_length << '\n';
    file << "constraint_iterations=" << s.constraint_iterations << '\n';
    file << "constraint_relaxation=" << s.constraint_relaxation << '\n';
    file << "constraint_interval=" << s.constraint_interval << '\n';
    file << "spawn_spread=" << s.spawn_spread << '\n';
    file << "fov=" << s.fov << '\n';
    file << "camera_distance=" << s.camera_distance << '\n';
    file << "camera_smooth_speed=" << s.camera_smooth_speed << '\n';
    file << "zoom_speed=" << s.zoom_speed << '\n';
    file << "fov_speed=" << s.fov_speed << '\n';
    file << "pan_speed=" << s.pan_speed << '\n';
    file << "rot_speed=" << s.rot_speed << '\n';
    file << "vertex_size=" << s.vertex_size << '\n';
    file << "draw_edges=" << (s.draw_edges ? 1 : 0) << '\n';
    file << "edge_stride=" << s.edge_stride << '\n';
    file << "billboard_nodes=" << (s.billboard_nodes ? 1 : 0) << '\n';
    file << "show_labels=" << (s.show_labels ? 1 : 0) << '\n';
    file << "show_debug=" << (s.show_debug ? 1 : 0) << '\n';
    file << "log_level=" << s.log_level << '\n';
    file << "worker_threads=" << s.worker_threads << '\n';
    file << "small_task_block_size=" << s.small_task_block_size << '\n';
    file << "large_task_block_size=" << s.large_task_block_size << '\n';
}
