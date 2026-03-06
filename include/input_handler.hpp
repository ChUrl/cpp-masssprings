#ifndef INPUT_HANDLER_HPP_
#define INPUT_HANDLER_HPP_

#include "orbit_camera.hpp"
#include "state_manager.hpp"

#include <functional>
#include <queue>
#include <raylib.h>
#include <raymath.h>

struct show_ok_message
{
    std::string title;
    std::string message;
};

struct show_yes_no_message
{
    std::string title;
    std::string message;
    std::function<void()> on_yes;
};

struct show_save_preset_window
{};

using ui_command = std::variant<show_ok_message, show_yes_no_message, show_save_preset_window>;

class input_handler
{
    struct generic_handler
    {
        std::function<void(input_handler&)> handler;
    };

    struct mouse_handler : generic_handler
    {
        MouseButton button;
    };

    struct keyboard_handler : generic_handler
    {
        KeyboardKey key;
    };

private:
    std::vector<generic_handler> generic_handlers;
    std::vector<mouse_handler> mouse_pressed_handlers;
    std::vector<mouse_handler> mouse_released_handlers;
    std::vector<keyboard_handler> key_pressed_handlers;
    std::vector<keyboard_handler> key_released_handlers;

public:
    state_manager& state;
    orbit_camera& camera;

    std::queue<ui_command> ui_commands;

    bool disable = false;

    // Block selection
    int hov_x = -1;
    int hov_y = -1;
    int sel_x = 0;
    int sel_y = 0;

    // Editing
    bool editing = false;
    bool has_block_add_xy = false;
    int block_add_x = -1;
    int block_add_y = -1;

    // Graph display
    bool mark_path = false;
    bool mark_solutions = false;
    bool connect_solutions = false;
    bool color_by_distance = false;

    // Camera
    bool camera_lock = true;
    bool camera_mass_center_lock = true;
    bool camera_panning = false;
    bool camera_rotating = false;

    // Mouse dragging
    Vector2 mouse = Vector2Zero();
    Vector2 last_mouse = Vector2Zero();

public:
    input_handler(state_manager& _state, orbit_camera& _camera) : state(_state), camera(_camera)
    {
        init_handlers();
    }

    input_handler(const input_handler& copy) = delete;
    auto operator=(const input_handler& copy) -> input_handler& = delete;
    input_handler(input_handler&& move) = delete;
    auto operator=(input_handler&& move) -> input_handler& = delete;

private:
    auto init_handlers() -> void;

public:
    // Helpers
    [[nodiscard]] auto mouse_in_menu_pane() const -> bool;
    [[nodiscard]] auto mouse_in_board_pane() const -> bool;
    [[nodiscard]] auto mouse_in_graph_pane() const -> bool;

    // Mouse actions
    auto mouse_hover() -> void;
    auto camera_start_pan() -> void;
    auto camera_pan() const -> void;
    auto camera_stop_pan() -> void;
    auto camera_start_rotate() -> void;
    auto camera_rotate() const -> void;
    auto camera_stop_rotate() -> void;
    auto camera_zoom() const -> void;
    auto camera_fov() const -> void;
    auto select_block() -> void;
    auto start_add_block() -> void;
    auto clear_add_block() -> void;
    auto add_block() -> void;
    auto remove_block() -> void;
    auto place_goal() const -> void;

    // Key actions
    auto toggle_camera_lock() -> void;
    auto toggle_camera_mass_center_lock() -> void;
    auto toggle_camera_projection() const -> void;
    auto move_block_nor() -> void;
    auto move_block_wes() -> void;
    auto move_block_sou() -> void;
    auto move_block_eas() -> void;
    auto print_state() const -> void;
    auto load_previous_preset() -> void;
    auto load_next_preset() -> void;
    auto goto_starting_state() -> void;
    auto populate_graph() const -> void;
    auto clear_graph() -> void;
    auto toggle_mark_solutions() -> void;
    auto toggle_connect_solutions() -> void;
    auto toggle_color_by_distance() -> void;
    auto toggle_mark_path() -> void;
    auto goto_optimal_next_state() const -> void;
    auto goto_most_distant_state() const -> void;
    auto goto_closest_target_state() const -> void;
    auto goto_previous_state() const -> void;
    auto toggle_restricted_movement() const -> void;
    auto toggle_target_block() const -> void;
    auto toggle_wall_block() const -> void;
    auto remove_board_column() const -> void;
    auto add_board_column() const -> void;
    auto remove_board_row() const -> void;
    auto add_board_row() const -> void;
    auto toggle_editing() -> void;
    auto clear_goal() const -> void;
    auto save_preset() -> void;

    // General
    auto register_generic_handler(const std::function<void(input_handler&)>& handler) -> void;

    auto register_mouse_pressed_handler(MouseButton button,
                                        const std::function<void(input_handler&)>& handler) -> void;

    auto register_mouse_released_handler(MouseButton button,
                                         const std::function<void(input_handler&)>& handler)
        -> void;

    auto register_key_pressed_handler(KeyboardKey key,
                                      const std::function<void(input_handler&)>& handler) -> void;

    auto register_key_released_handler(KeyboardKey key,
                                       const std::function<void(input_handler&)>& handler) -> void;

    auto handle_input() -> void;
};

#endif