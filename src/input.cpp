#include "input.hpp"
#include "config.hpp"

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
    register_mouse_pressed_handler(MOUSE_BUTTON_LEFT, &input_handler::select_block);
    register_mouse_pressed_handler(MOUSE_BUTTON_LEFT, &input_handler::add_block);
    register_mouse_pressed_handler(MOUSE_BUTTON_LEFT, &input_handler::start_add_block);
    register_mouse_pressed_handler(MOUSE_BUTTON_MIDDLE, &input_handler::place_goal);
    register_mouse_pressed_handler(MOUSE_BUTTON_RIGHT, &input_handler::camera_start_rotate);
    register_mouse_pressed_handler(MOUSE_BUTTON_RIGHT, &input_handler::remove_block);
    register_mouse_pressed_handler(MOUSE_BUTTON_RIGHT, &input_handler::clear_add_block);

    register_mouse_released_handler(MOUSE_BUTTON_LEFT, &input_handler::camera_stop_pan);
    register_mouse_released_handler(MOUSE_BUTTON_RIGHT, &input_handler::camera_stop_rotate);

    register_key_pressed_handler(KEY_W, &input_handler::move_block_nor);
    register_key_pressed_handler(KEY_D, &input_handler::move_block_eas);
    register_key_pressed_handler(KEY_S, &input_handler::move_block_sou);
    register_key_pressed_handler(KEY_A, &input_handler::move_block_wes);

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

    register_key_pressed_handler(KEY_TAB, &input_handler::toggle_editing);
    register_key_pressed_handler(KEY_F, &input_handler::toggle_restricted_movement);
    register_key_pressed_handler(KEY_T, &input_handler::toggle_target_block);
    register_key_pressed_handler(KEY_Y, &input_handler::toggle_wall_block);
    register_key_pressed_handler(KEY_UP, &input_handler::add_board_row);
    register_key_pressed_handler(KEY_RIGHT, &input_handler::add_board_column);
    register_key_pressed_handler(KEY_DOWN, &input_handler::remove_board_row);
    register_key_pressed_handler(KEY_LEFT, &input_handler::remove_board_column);
    register_key_pressed_handler(KEY_X, &input_handler::clear_goal);
    register_key_pressed_handler(KEY_P, &input_handler::print_state);

    register_key_pressed_handler(KEY_L, &input_handler::toggle_camera_lock);
    register_key_pressed_handler(KEY_LEFT_ALT, &input_handler::toggle_camera_projection);
    register_key_pressed_handler(KEY_U, &input_handler::toggle_camera_mass_center_lock);
}

auto input_handler::mouse_in_menu_pane() const -> bool
{
    return mouse.y < MENU_HEIGHT;
}

auto input_handler::mouse_in_board_pane() const -> bool
{
    return mouse.x < GetScreenWidth() / 2.0 && mouse.y >= MENU_HEIGHT;
}

auto input_handler::mouse_in_graph_pane() const -> bool
{
    return mouse.x >= GetScreenWidth() / 2.0 && mouse.y >= MENU_HEIGHT;
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
    // Enable this if the camera should be pannable even when locked (releasing the lock in the process):
    // camera_lock = false;
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
    if (!mouse_in_graph_pane()) {
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
    if (!mouse_in_graph_pane() || IsKeyDown(KEY_LEFT_CONTROL) || camera.projection == CAMERA_ORTHOGRAPHIC) {
        return;
    }

    const float wheel = GetMouseWheelMove();

    if (IsKeyDown(KEY_LEFT_SHIFT)) {
        camera.distance -= wheel * ZOOM_SPEED * ZOOM_MULTIPLIER;
    } else {
        camera.distance -= wheel * ZOOM_SPEED;
    }
}

auto input_handler::camera_fov() const -> void
{
    if (!mouse_in_graph_pane() || !IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_LEFT_SHIFT)) {
        return;
    }

    const float wheel = GetMouseWheelMove();
    camera.fov -= wheel * FOV_SPEED;
}

auto input_handler::select_block() -> void
{
    const puzzle& current = state.get_current_state();
    if (current.try_get_block(hov_x, hov_y)) {
        sel_x = hov_x;
        sel_y = hov_y;
    }
}

auto input_handler::start_add_block() -> void
{
    const puzzle& current = state.get_current_state();
    if (!editing || current.try_get_block(hov_x, hov_y) || has_block_add_xy) {
        return;
    }

    if (hov_x >= 0 && hov_x < current.width && hov_y >= 0 && hov_y < current.height) {
        block_add_x = hov_x;
        block_add_y = hov_y;
        has_block_add_xy = true;
    }
}

auto input_handler::clear_add_block() -> void
{
    if (!editing || !has_block_add_xy) {
        return;
    }

    block_add_x = -1;
    block_add_y = -1;
    has_block_add_xy = false;
}

auto input_handler::add_block() -> void
{
    const puzzle& current = state.get_current_state();
    if (!editing || current.try_get_block(hov_x, hov_y) || !has_block_add_xy) {
        return;
    }

    const int block_add_width = hov_x - block_add_x + 1;
    const int block_add_height = hov_y - block_add_y + 1;
    if (block_add_width <= 0 || block_add_height <= 0) {
        block_add_x = -1;
        block_add_y = -1;
        has_block_add_xy = false;
    } else if (current.covers(block_add_x, block_add_y, block_add_width, block_add_height)) {
        const std::optional<puzzle>& next =
            current.try_add_block(puzzle::block(block_add_x, block_add_y, block_add_width, block_add_height, false));

        if (next) {
            sel_x = block_add_x;
            sel_y = block_add_y;
            block_add_x = -1;
            block_add_y = -1;
            has_block_add_xy = false;
            state.edit_starting_state(*next);
        }
    }
}

auto input_handler::remove_block() -> void
{
    const puzzle& current = state.get_current_state();
    const std::optional<puzzle::block>& b = current.try_get_block(hov_x, hov_y);
    if (!editing || has_block_add_xy || !b) {
        return;
    }

    const std::optional<puzzle>& next = current.try_remove_block(hov_x, hov_y);
    if (!next) {
        return;
    }

    // Reset selection if we removed the selected block
    if (b->covers(sel_x, sel_y)) {
        sel_x = 0;
        sel_y = 0;
    }

    state.edit_starting_state(*next);
}

auto input_handler::place_goal() const -> void
{
    const puzzle& current = state.get_current_state();
    if (!editing || !current.covers(hov_x, hov_y)) {
        return;
    }

    const std::optional<puzzle>& next = current.try_set_goal(hov_x, hov_y);
    if (!next) {
        return;
    }

    state.edit_starting_state(*next);
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
    camera.projection = camera.projection == CAMERA_PERSPECTIVE ? CAMERA_ORTHOGRAPHIC : CAMERA_PERSPECTIVE;
}

auto input_handler::move_block_nor() -> void
{
    const puzzle& current = state.get_current_state();
    const std::optional<puzzle>& next = current.try_move_block_at(sel_x, sel_y, nor);
    if (!next) {
        return;
    }

    sel_y--;
    state.update_current_state(*next);
}

auto input_handler::move_block_wes() -> void
{
    const puzzle& current = state.get_current_state();
    const std::optional<puzzle>& next = current.try_move_block_at(sel_x, sel_y, wes);
    if (!next) {
        return;
    }

    sel_x--;
    state.update_current_state(*next);
}

auto input_handler::move_block_sou() -> void
{
    const puzzle& current = state.get_current_state();
    const std::optional<puzzle>& next = current.try_move_block_at(sel_x, sel_y, sou);
    if (!next) {
        return;
    }

    sel_y++;
    state.update_current_state(*next);
}

auto input_handler::move_block_eas() -> void
{
    const puzzle& current = state.get_current_state();
    const std::optional<puzzle>& next = current.try_move_block_at(sel_x, sel_y, eas);
    if (!next) {
        return;
    }

    sel_x++;
    state.update_current_state(*next);
}

auto input_handler::print_state() const -> void
{
    infoln("State: \"{}\"", state.get_current_state().state);
}

auto input_handler::load_previous_preset() -> void
{
    if (editing) {
        return;
    }

    block_add_x = -1;
    block_add_y = -1;
    has_block_add_xy = false;
    state.load_previous_preset();
}

auto input_handler::load_next_preset() -> void
{
    if (editing) {
        return;
    }

    block_add_x = -1;
    block_add_y = -1;
    has_block_add_xy = false;
    state.load_next_preset();
}

auto input_handler::goto_starting_state() -> void
{
    state.goto_starting_state();
    sel_x = 0;
    sel_y = 0;
}

auto input_handler::populate_graph() const -> void
{
    state.populate_graph();
}

auto input_handler::clear_graph() const -> void
{
    state.clear_graph_and_add_current();
}

auto input_handler::toggle_mark_solutions() -> void
{
    mark_solutions = !mark_solutions;
}

auto input_handler::toggle_connect_solutions() -> void
{
    connect_solutions = !connect_solutions;
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

auto input_handler::toggle_restricted_movement() const -> void
{
    const puzzle& current = state.get_current_state();
    const std::optional<puzzle>& next = current.try_toggle_restricted();

    if (!editing || !next) {
        return;
    }

    state.edit_starting_state(*next);
}

auto input_handler::toggle_target_block() const -> void
{
    const puzzle& current = state.get_current_state();
    const std::optional<puzzle>& next = current.try_toggle_target(sel_x, sel_y);

    if (!editing || !next) {
        return;
    }

    state.edit_starting_state(*next);
}

auto input_handler::toggle_wall_block() const -> void
{
    const puzzle& current = state.get_current_state();
    const std::optional<puzzle>& next = current.try_toggle_wall(sel_x, sel_y);

    if (!editing || !next) {
        return;
    }

    state.edit_starting_state(*next);
}

auto input_handler::remove_board_column() const -> void
{
    const puzzle& current = state.get_current_state();
    const std::optional<puzzle>& next = current.try_remove_column();

    if (!editing || current.width <= puzzle::MIN_WIDTH || !next) {
        return;
    }

    state.edit_starting_state(*next);
}

auto input_handler::add_board_column() const -> void
{
    const puzzle& current = state.get_current_state();
    const std::optional<puzzle>& next = current.try_add_column();

    if (!editing || current.width >= puzzle::MAX_WIDTH || !next) {
        return;
    }

    state.edit_starting_state(*next);
}

auto input_handler::remove_board_row() const -> void
{
    const puzzle& current = state.get_current_state();
    const std::optional<puzzle>& next = current.try_remove_row();

    if (!editing || current.height <= puzzle::MIN_HEIGHT || !next) {
        return;
    }

    state.edit_starting_state(*next);
}

auto input_handler::add_board_row() const -> void
{
    const puzzle& current = state.get_current_state();
    const std::optional<puzzle>& next = current.try_add_row();

    if (!editing || current.height >= puzzle::MAX_HEIGHT || !next) {
        return;
    }

    state.edit_starting_state(*next);
}

auto input_handler::toggle_editing() -> void
{
    if (editing) {
        has_block_add_xy = false;
        block_add_x = -1;
        block_add_y = -1;
    }

    editing = !editing;
}

auto input_handler::clear_goal() const -> void
{
    const puzzle& current = state.get_current_state();
    const std::optional<puzzle>& next = current.try_clear_goal();

    if (!editing || !next) {
        return;
    }

    state.edit_starting_state(*next);
}

auto input_handler::register_generic_handler(const std::function<void(input_handler&)>& handler) -> void
{
    generic_handlers.push_back({handler});
}

auto input_handler::register_mouse_pressed_handler(const MouseButton button,
                                                   const std::function<void(input_handler&)>& handler) -> void
{
    mouse_pressed_handlers.push_back({{handler}, button});
}

auto input_handler::register_mouse_released_handler(const MouseButton button,
                                                    const std::function<void(input_handler&)>& handler) -> void
{
    mouse_released_handlers.push_back({{handler}, button});
}

auto input_handler::register_key_pressed_handler(const KeyboardKey key,
                                                 const std::function<void(input_handler&)>& handler) -> void
{
    key_pressed_handlers.push_back({{handler}, key});
}

auto input_handler::register_key_released_handler(const KeyboardKey key,
                                                  const std::function<void(input_handler&)>& handler) -> void
{
    key_released_handlers.push_back({{handler}, key});
}

auto input_handler::handle_input() -> void
{
    if (disable) {
        return;
    }

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

    for (const keyboard_handler& handler : key_pressed_handlers) {
        if (IsKeyPressed(handler.key)) {
            handler.handler(*this);
        }
    }

    for (const keyboard_handler& handler : key_released_handlers) {
        if (IsKeyReleased(handler.key)) {
            handler.handler(*this);
        }
    }
}
