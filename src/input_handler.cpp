#include "input_handler.hpp"
#include "config.hpp"
#include "layout.hpp"
#include "settings.hpp"

#include <raylib.h>

auto input_handler::init_handlers() -> void
{
    // The order matters if multiple handlers are registered to the same key

    register_generic_handler(&input_handler::camera_pan);
    register_generic_handler(&input_handler::camera_rotate);
    register_generic_handler(&input_handler::camera_zoom);
    register_generic_handler(&input_handler::camera_fov);
    register_generic_handler(&input_handler::mouse_hover);

    register_mouse_pressed_handler(MOUSE_BUTTON_LEFT, &input_handler::camera_start_pan);
    register_mouse_pressed_handler(MOUSE_BUTTON_MIDDLE, &input_handler::select_state);
    register_mouse_pressed_handler(MOUSE_BUTTON_RIGHT, &input_handler::camera_start_rotate);

    register_mouse_released_handler(MOUSE_BUTTON_LEFT, &input_handler::camera_stop_pan);
    register_mouse_released_handler(MOUSE_BUTTON_RIGHT, &input_handler::camera_stop_rotate);

    register_key_pressed_handler(KEY_N, &input_handler::load_previous_preset);
    register_key_pressed_handler(KEY_M, &input_handler::load_next_preset);
    register_key_pressed_handler(KEY_R, &input_handler::goto_starting_state);
    register_key_pressed_handler(KEY_V, &input_handler::goto_most_distant_state);
    register_key_pressed_handler(KEY_B, &input_handler::goto_closest_target_state);
    register_key_pressed_handler(KEY_SPACE, &input_handler::goto_optimal_next_state);
    register_key_pressed_handler(KEY_BACKSPACE, &input_handler::goto_previous_state);

    register_key_pressed_handler(KEY_G, &input_handler::populate_graph);
    register_key_pressed_handler(KEY_C, &input_handler::clear_graph);
    register_key_pressed_handler(KEY_I, &input_handler::toggle_mark_solutions);
    register_key_pressed_handler(KEY_O, &input_handler::toggle_connect_solutions);
    register_key_pressed_handler(KEY_Z, &input_handler::toggle_color_by_distance);

    register_key_pressed_handler(KEY_COMMA, &input_handler::toggle_settings);
    register_key_pressed_handler(KEY_TAB, &input_handler::toggle_editing);
    register_key_pressed_handler(KEY_P, &input_handler::print_state);
    register_key_pressed_handler(KEY_S, &input_handler::save_preset); // + CTRL

    register_key_pressed_handler(KEY_L, &input_handler::toggle_camera_lock);
    register_key_pressed_handler(KEY_LEFT_ALT, &input_handler::toggle_camera_projection);
    register_key_pressed_handler(KEY_U, &input_handler::toggle_camera_mass_center_lock);
}

auto input_handler::mouse_in_menu_pane() const -> bool
{
    return mouse.y < COMPACT_MENU_HEIGHT;
}

auto input_handler::mouse_in_board_pane() const -> bool
{
    return CheckCollisionPointRec(mouse, board_pane_bounds());
}

auto input_handler::mouse_in_graph_pane() const -> bool
{
    const simulation_settings cfg = get_settings();
    if (cfg.show_performance_overlay &&
        CheckCollisionPointRec(mouse, performance_overlay_bounds())) {
        return false;
    }
    if (cfg.show_memory_overlay && CheckCollisionPointRec(mouse, memory_overlay_bounds())) {
        return false;
    }
    return CheckCollisionPointRec(mouse, graph_pane_bounds());
}

auto input_handler::mouse_hover() -> void
{
    last_mouse = mouse;
    mouse = GetMousePosition();
}

auto input_handler::camera_start_pan() -> void
{
    if (!mouse_in_graph_pane()) {
        return;
    }

    camera_panning = true;
    // Enable this if the camera should be pannable even when locked (releasing the lock in the
    // process): camera_lock = false;
}

auto input_handler::camera_pan() const -> void
{
    if (camera_panning) {
        camera.pan(last_mouse, mouse);
    }
}

auto input_handler::camera_stop_pan() -> void
{
    camera_panning = false;
}

auto input_handler::camera_start_rotate() -> void
{
    if (!mouse_in_graph_pane() || camera.planar) {
        return;
    }

    camera_rotating = true;
}

auto input_handler::camera_rotate() const -> void
{
    if (camera_rotating) {
        camera.rotate(last_mouse, mouse);
    }
}

auto input_handler::camera_stop_rotate() -> void
{
    camera_rotating = false;
}

auto input_handler::camera_zoom() const -> void
{
    if (!mouse_in_graph_pane() || IsKeyDown(KEY_LEFT_CONTROL)) {
        return;
    }

    const float wheel = GetMouseWheelMove();
    const float zoom_speed = get_settings().zoom_speed;

    if (camera.projection == CAMERA_ORTHOGRAPHIC) {
        // Orthographic zoom is controlled by the field of view.
        camera.fov -= wheel * zoom_speed * FOV_MULTIPLIER;
        return;
    }

    if (IsKeyDown(KEY_LEFT_SHIFT)) {
        camera.distance -= wheel * zoom_speed * ZOOM_MULTIPLIER;
    } else {
        camera.distance -= wheel * zoom_speed;
    }
}

auto input_handler::camera_fov() const -> void
{
    if (!mouse_in_graph_pane() || !IsKeyDown(KEY_LEFT_CONTROL)) {
        return;
    }

    const float wheel = GetMouseWheelMove();
    const float fov_speed = get_settings().fov_speed;

    if (IsKeyDown(KEY_LEFT_SHIFT)) {
        camera.fov -= wheel * fov_speed * FOV_MULTIPLIER;
    } else {
        camera.fov -= wheel * fov_speed;
    }
}

auto input_handler::select_state() const -> void
{
    if (!mouse_in_graph_pane() || collision_mass == static_cast<size_t>(-1)) {
        return;
    }

    state.update_current_state(collision_mass);
}

auto input_handler::toggle_settings() -> void
{
    settings_open = !settings_open;
    if (!settings_open) {
        save_settings();
    }
}

auto input_handler::toggle_camera_lock() -> void
{
    if (!camera_lock) {
        camera_panning = false;
    }

    camera_lock = !camera_lock;
}

auto input_handler::toggle_camera_mass_center_lock() -> void
{
    if (!camera_mass_center_lock) {
        camera_lock = true;
        camera_panning = false;
    }

    camera_mass_center_lock = !camera_mass_center_lock;
}

auto input_handler::toggle_camera_projection() const -> void
{
    if (camera.planar) {
        return;
    }

    camera.projection =
        camera.projection == CAMERA_PERSPECTIVE ? CAMERA_ORTHOGRAPHIC : CAMERA_PERSPECTIVE;
}

auto input_handler::print_state() const -> void
{
    infoln("State: \"{}\"", state.describe_current());
}

auto input_handler::load_previous_preset() -> void
{
    if (state.presenter().editing()) {
        return;
    }

    const auto handler = [&] { state.load_previous_preset(); };

    if (state.was_edited()) {
        ui_commands.emplace(show_yes_no_message{"Switch Preset?", "Edits Will Be Lost.", handler});
    } else {
        handler();
    }
}

auto input_handler::load_next_preset() -> void
{
    if (state.presenter().editing()) {
        return;
    }

    const auto handler = [&] { state.load_next_preset(); };

    if (state.was_edited()) {
        ui_commands.emplace(show_yes_no_message{"Switch Preset?", "Edits Will Be Lost.", handler});
    } else {
        handler();
    }
}

auto input_handler::goto_starting_state() -> void
{
    const auto handler = [&] { state.goto_starting_state(); };

    ui_commands.emplace(
        show_yes_no_message{"Reset Board?", "This Clears the Move History.", handler});
}

auto input_handler::populate_graph() const -> void
{
    if (state.search_is_running()) {
        if (state.get_search_kind() == search_kind::state_space) {
            state.cancel_search();
        }
        return;
    }
    state.start_state_space_search();
}

auto input_handler::clear_graph() -> void
{
    const auto handler = [&] { state.clear_graph_and_add_current(); };

    ui_commands.emplace(
        show_yes_no_message{"Clear Graph?", "This Clears the Move History.", handler});
}

auto input_handler::toggle_mark_solutions() -> void
{
    mark_solutions = !mark_solutions;
}

auto input_handler::toggle_connect_solutions() -> void
{
    connect_solutions = !connect_solutions;
}

auto input_handler::toggle_color_by_distance() -> void
{
    color_by_distance = !color_by_distance;
}

auto input_handler::toggle_mark_path() -> void
{
    mark_path = !mark_path;
}

auto input_handler::goto_optimal_next_state() const -> void
{
    state.goto_optimal_next_state();
}

auto input_handler::goto_most_distant_state() const -> void
{
    state.goto_most_distant_state();
}

auto input_handler::goto_closest_target_state() const -> void
{
    state.goto_closest_target_state();
}

auto input_handler::goto_previous_state() const -> void
{
    state.goto_previous_state();
}

auto input_handler::toggle_editing() -> void
{
    if (state.presenter().supports_editing()) {
        state.presenter().toggle_editing();
    }
}

auto input_handler::save_preset() -> void
{
    if (!IsKeyDown(KEY_LEFT_CONTROL)) {
        return;
    }

    if (const std::optional<std::string>& reason = state.validate_current()) {
        ui_commands.emplace(
            show_ok_message{"Can't Save Preset", std::format("Invalid Board: {}.", *reason)});
    } else {
        ui_commands.emplace(show_save_preset_window{});
    }
}

auto input_handler::register_generic_handler(const std::function<void(input_handler&)>& handler)
    -> void
{
    generic_handlers.push_back({handler});
}

auto input_handler::register_mouse_pressed_handler(
    const MouseButton button, const std::function<void(input_handler&)>& handler) -> void
{
    mouse_pressed_handlers.push_back({{handler}, button});
}

auto input_handler::register_mouse_released_handler(
    const MouseButton button, const std::function<void(input_handler&)>& handler) -> void
{
    mouse_released_handlers.push_back({{handler}, button});
}

auto input_handler::register_key_pressed_handler(const KeyboardKey key,
                                                 const std::function<void(input_handler&)>& handler)
    -> void
{
    key_pressed_handlers.push_back({{handler}, key});
}

auto input_handler::register_key_released_handler(
    const KeyboardKey key, const std::function<void(input_handler&)>& handler) -> void
{
    key_released_handlers.push_back({{handler}, key});
}

auto input_handler::handle_input() -> void
{
    // Text fields own all keyboard input. Message boxes and floating panels do not: hotkeys remain
    // available while they are open, only pointer input is blocked behind them.
    if (text_input_open) {
        camera_panning = false;
        camera_rotating = false;
        return;
    }

    // The help overlay is toggled by the actual "?" character, so it also works on keyboard
    // layouts where the slash/question key is not KEY_SLASH.
    for (int character = GetCharPressed(); character != 0; character = GetCharPressed()) {
        if (character == '?') {
            ui_commands.emplace(toggle_help_window{});
        }
    }

    if (last_puzzle_generation != state.generation()) {
        last_puzzle_generation = state.generation();
        collision_mass = static_cast<size_t>(-1);
        state.presenter().reset();
    }

    const bool pointer_blocked = disable;
    if (pointer_blocked) {
        camera_panning = false;
        camera_rotating = false;
    } else {
        for (const auto& [handler] : generic_handlers) {
            handler(*this);
        }

        for (const mouse_handler& handler : mouse_pressed_handlers) {
            if (IsMouseButtonPressed(handler.button)) {
                handler.handler(*this);
            }
        }

        for (const mouse_handler& handler : mouse_released_handlers) {
            if (IsMouseButtonReleased(handler.button)) {
                handler.handler(*this);
            }
        }

        puzzle_canvas canvas;
        canvas.bounds = board_canvas_bounds();
        canvas.columns =
            state.presenter().board_columns(state.model(), state.get_current_state_id());
        canvas.rows = state.presenter().board_rows(state.model(), state.get_current_state_id());
        canvas.padding = BOARD_PADDING;

        if (mouse_in_board_pane()) {
            for (const MouseButton button :
                 {MOUSE_BUTTON_LEFT, MOUSE_BUTTON_MIDDLE, MOUSE_BUTTON_RIGHT}) {
                if (IsMouseButtonPressed(button)) {
                    state.presenter().on_board_press(button, canvas, state);
                }
                if (IsMouseButtonReleased(button)) {
                    state.presenter().on_board_release(button, canvas, state);
                }
            }
        }
    }

    for (const keyboard_handler& handler : key_pressed_handlers) {
        if (IsKeyPressed(handler.key)) {
            handler.handler(*this);
        }
    }

    const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    for (const KeyboardKey key :
         {KEY_W, KEY_A, KEY_S, KEY_D, KEY_T, KEY_Y, KEY_F, KEY_X, KEY_UP, KEY_DOWN, KEY_LEFT,
          KEY_RIGHT, KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR, KEY_F2, KEY_F3}) {
        if (IsKeyPressed(key) && state.presenter().on_key(key, ctrl, shift, state)) {
            break;
        }
    }

    for (const keyboard_handler& handler : key_released_handlers) {
        if (IsKeyReleased(handler.key)) {
            handler.handler(*this);
        }
    }
}
