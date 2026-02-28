#include "user_interface.hpp"
#include "config.hpp"

#include <raylib.h>

#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

#ifdef TRACY
    #include <tracy/Tracy.hpp>
#endif

auto user_interface::grid::update_bounds(const int _x, const int _y, const int _width, const int _height,
                                         const int _columns, const int _rows) -> void
{
    x = _x;
    y = _y;
    width = _width;
    height = _height;
    columns = _columns;
    rows = _rows;
}

auto user_interface::grid::update_bounds(const int _x, const int _y, const int _width, const int _height) -> void
{
    x = _x;
    y = _y;
    width = _width;
    height = _height;
}

auto user_interface::grid::update_bounds(const int _x, const int _y) -> void
{
    x = _x;
    y = _y;
}

auto user_interface::grid::bounds() const -> Rectangle
{
    Rectangle bounds{0, 0, static_cast<float>(columns), static_cast<float>(rows)};
    bounds.x -= padding;
    bounds.y -= padding;
    bounds.width += 2 * padding;
    bounds.height += 2 * padding;
    return bounds;
}

auto user_interface::grid::bounds(const int _x, const int _y, const int _width, const int _height) const -> Rectangle
{
    if (_x < 0 || _x + _width > columns || _y < 0 || _y + _height > rows) {
        errln("Grid bounds are outside range.");
        exit(1);
    }

    const int cell_width = (width - padding) / columns;
    const int cell_height = (height - padding) / rows;

    return Rectangle(x + _x * cell_width + padding, y + _y * cell_height + padding, _width * cell_width - padding,
                     _height * cell_height - padding);
}

auto user_interface::grid::square_bounds() const -> Rectangle
{
    Rectangle bounds = square_bounds(0, 0, columns, rows);
    bounds.x -= padding;
    bounds.y -= padding;
    bounds.width += 2 * padding;
    bounds.height += 2 * padding;
    return bounds;
}

auto user_interface::grid::square_bounds(const int _x, const int _y, const int _width, const int _height) const
    -> Rectangle
{
    // Assumes each cell is square, so either width or height are not completely
    // filled

    if (_x < 0 || _x + _width > columns || _y < 0 || _y + _height > rows) {
        errln("Grid bounds are outside range.");
        exit(1);
    }

    const int available_width = width - padding * (columns + 1);
    const int available_height = height - padding * (rows + 1);
    const int cell_size = std::min(available_width / columns, available_height / rows);

    const int grid_width = cell_size * columns + padding * (columns + 1);
    const int grid_height = cell_size * rows + padding * (rows + 1);
    const int x_offset = (width - grid_width) / 2;
    const int y_offset = (height - grid_height) / 2;

    return Rectangle(x_offset + _x * (cell_size + padding) + padding, y_offset + _y * (cell_size + padding) + padding,
                     _width * cell_size + padding * (_width - 1), _height * cell_size + padding * (_height - 1));
}

auto user_interface::init() -> void
{
    const Font font = LoadFontEx(FONT, FONT_SIZE, nullptr, 0);
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    GuiSetFont(font);

    default_style style = get_default_style();
    style.text_size = FONT_SIZE;
    apply_color(style, GRAY);

    set_default_style(style);
}

auto user_interface::apply_color(style& style, const Color color) -> void
{
    style.base_color_normal = ColorToInt(Fade(color, 0.8));
    style.base_color_focused = ColorToInt(Fade(color, 0.3));
    style.base_color_pressed = ColorToInt(Fade(color, 0.8));
    style.base_color_disabled = ColorToInt(Fade(color, 0.5));

    style.border_color_normal = ColorToInt(Fade(color, 1.0));
    style.border_color_focused = ColorToInt(Fade(color, 0.7));
    style.border_color_pressed = ColorToInt(Fade(color, 1.0));
    style.border_color_disabled = ColorToInt(Fade(GRAY, 0.5));

    style.text_color_normal = ColorToInt(Fade(BLACK, 1.0));
    style.text_color_focused = ColorToInt(Fade(BLACK, 1.0));
    style.text_color_pressed = ColorToInt(Fade(BLACK, 1.0));
    style.text_color_disabled = ColorToInt(Fade(BLACK, 0.5));
}

auto user_interface::apply_block_color(style& style, const Color color) -> void
{
    style.base_color_normal = ColorToInt(Fade(color, 0.5));
    style.base_color_focused = ColorToInt(Fade(color, 0.3));
    style.base_color_pressed = ColorToInt(Fade(color, 0.8));
    style.base_color_disabled = ColorToInt(Fade(color, 0.5));

    style.border_color_normal = ColorToInt(Fade(color, 1.0));
    style.border_color_focused = ColorToInt(Fade(color, 0.7));
    style.border_color_pressed = ColorToInt(Fade(color, 1.0));
    style.border_color_disabled = ColorToInt(Fade(GRAY, 0.5));
}

auto user_interface::apply_text_color(style& style, const Color color) -> void
{
    style.text_color_normal = ColorToInt(Fade(color, 1.0));
    style.text_color_focused = ColorToInt(Fade(color, 1.0));
    style.text_color_pressed = ColorToInt(Fade(color, 1.0));
    style.text_color_disabled = ColorToInt(Fade(BLACK, 0.5));
}

auto user_interface::get_default_style() -> default_style
{
    // Could've iterated over the values, but then it wouldn't be as nice to
    // access...
    return {{GuiGetStyle(DEFAULT, BORDER_COLOR_NORMAL), GuiGetStyle(DEFAULT, BASE_COLOR_NORMAL),
             GuiGetStyle(DEFAULT, TEXT_COLOR_NORMAL), GuiGetStyle(DEFAULT, BORDER_COLOR_FOCUSED),
             GuiGetStyle(DEFAULT, BASE_COLOR_FOCUSED), GuiGetStyle(DEFAULT, TEXT_COLOR_FOCUSED),
             GuiGetStyle(DEFAULT, BORDER_COLOR_PRESSED), GuiGetStyle(DEFAULT, BASE_COLOR_PRESSED),
             GuiGetStyle(DEFAULT, TEXT_COLOR_PRESSED), GuiGetStyle(DEFAULT, BORDER_COLOR_DISABLED),
             GuiGetStyle(DEFAULT, BASE_COLOR_DISABLED), GuiGetStyle(DEFAULT, TEXT_COLOR_DISABLED)},
            GuiGetStyle(DEFAULT, BACKGROUND_COLOR),
            GuiGetStyle(DEFAULT, LINE_COLOR),
            GuiGetStyle(DEFAULT, TEXT_SIZE),
            GuiGetStyle(DEFAULT, TEXT_SPACING),
            GuiGetStyle(DEFAULT, TEXT_LINE_SPACING),
            GuiGetStyle(DEFAULT, TEXT_ALIGNMENT_VERTICAL),
            GuiGetStyle(DEFAULT, TEXT_WRAP_MODE)};
}

auto user_interface::set_default_style(const default_style& style) -> void
{
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, style.border_color_normal);
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, style.base_color_normal);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, style.text_color_normal);

    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, style.border_color_focused);
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, style.base_color_focused);
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, style.text_color_focused);

    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, style.border_color_pressed);
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, style.base_color_pressed);
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, style.text_color_pressed);

    GuiSetStyle(DEFAULT, BORDER_COLOR_DISABLED, style.border_color_disabled);
    GuiSetStyle(DEFAULT, BASE_COLOR_DISABLED, style.base_color_disabled);
    GuiSetStyle(DEFAULT, TEXT_COLOR_DISABLED, style.text_color_disabled);

    GuiSetStyle(DEFAULT, BACKGROUND_COLOR, style.background_color);
    GuiSetStyle(DEFAULT, LINE_COLOR, style.line_color);

    GuiSetStyle(DEFAULT, TEXT_SIZE, style.text_size);
    GuiSetStyle(DEFAULT, TEXT_SPACING, style.text_spacing);
    GuiSetStyle(DEFAULT, TEXT_LINE_SPACING, style.text_line_spacing);
    GuiSetStyle(DEFAULT, TEXT_ALIGNMENT_VERTICAL, style.text_alignment_vertical);
    GuiSetStyle(DEFAULT, TEXT_WRAP_MODE, style.text_wrap_mode);
}

auto user_interface::get_component_style(const int component) -> component_style
{
    return {{GuiGetStyle(component, BORDER_COLOR_NORMAL), GuiGetStyle(component, BASE_COLOR_NORMAL),
             GuiGetStyle(component, TEXT_COLOR_NORMAL), GuiGetStyle(component, BORDER_COLOR_FOCUSED),
             GuiGetStyle(component, BASE_COLOR_FOCUSED), GuiGetStyle(component, TEXT_COLOR_FOCUSED),
             GuiGetStyle(component, BORDER_COLOR_PRESSED), GuiGetStyle(component, BASE_COLOR_PRESSED),
             GuiGetStyle(component, TEXT_COLOR_PRESSED), GuiGetStyle(component, BORDER_COLOR_DISABLED),
             GuiGetStyle(component, BASE_COLOR_DISABLED), GuiGetStyle(component, TEXT_COLOR_DISABLED)},
            GuiGetStyle(component, BORDER_WIDTH),
            GuiGetStyle(component, TEXT_PADDING),
            GuiGetStyle(component, TEXT_ALIGNMENT)};
}

auto user_interface::set_component_style(const int component, const component_style& style) -> void
{
    GuiSetStyle(component, BORDER_COLOR_NORMAL, style.border_color_normal);
    GuiSetStyle(component, BASE_COLOR_NORMAL, style.base_color_normal);
    GuiSetStyle(component, TEXT_COLOR_NORMAL, style.text_color_normal);

    GuiSetStyle(component, BORDER_COLOR_FOCUSED, style.border_color_focused);
    GuiSetStyle(component, BASE_COLOR_FOCUSED, style.base_color_focused);
    GuiSetStyle(component, TEXT_COLOR_FOCUSED, style.text_color_focused);

    GuiSetStyle(component, BORDER_COLOR_PRESSED, style.border_color_pressed);
    GuiSetStyle(component, BASE_COLOR_PRESSED, style.base_color_pressed);
    GuiSetStyle(component, TEXT_COLOR_PRESSED, style.text_color_pressed);

    GuiSetStyle(component, BORDER_COLOR_DISABLED, style.border_color_disabled);
    GuiSetStyle(component, BASE_COLOR_DISABLED, style.base_color_disabled);
    GuiSetStyle(component, TEXT_COLOR_DISABLED, style.text_color_disabled);

    GuiSetStyle(component, BORDER_WIDTH, style.border_width);
    GuiSetStyle(component, TEXT_PADDING, style.text_padding);
    GuiSetStyle(component, TEXT_ALIGNMENT, style.text_alignment);
}

auto user_interface::draw_button(const Rectangle bounds, const std::string& label, const Color color,
                                 const bool enabled, const int font_size) const -> int
{
    // Save original styling
    const default_style original_default = get_default_style();
    const component_style original_button = get_component_style(BUTTON);

    // Change styling
    default_style style_default = original_default;
    component_style style_button = original_button;
    style_default.text_size = font_size;
    apply_color(style_button, color);
    set_default_style(style_default);
    set_component_style(BUTTON, style_button);

    const int _state = GuiGetState();
    if (!enabled || window_open()) {
        GuiSetState(STATE_DISABLED);
    }
    const int pressed = GuiButton(bounds, label.data());
    if (!enabled || window_open()) {
        GuiSetState(_state);
    }

    // Restore original styling
    set_default_style(original_default);
    set_component_style(BUTTON, original_button);

    return pressed;
}

auto user_interface::draw_menu_button(const int x, const int y, const int width, const int height,
                                      const std::string& label, const Color color, const bool enabled,
                                      const int font_size) const -> int
{
    const Rectangle bounds = menu_grid.bounds(x, y, width, height);
    return draw_button(bounds, label, color, enabled, font_size);
}

auto user_interface::draw_toggle_slider(const Rectangle bounds, const std::string& off_label,
                                        const std::string& on_label, int* active, Color color, bool enabled,
                                        int font_size) const -> int
{
    // Save original styling
    const default_style original_default = get_default_style();
    const component_style original_slider = get_component_style(SLIDER);
    const component_style original_toggle = get_component_style(TOGGLE);

    // Change styling
    default_style style_default = original_default;
    component_style style_slider = original_slider;
    component_style style_toggle = original_toggle;
    style_default.text_size = font_size;
    apply_color(style_slider, color);
    apply_color(style_toggle, color);
    set_default_style(style_default);
    set_component_style(SLIDER, style_slider);
    set_component_style(TOGGLE, style_toggle);

    const int _state = GuiGetState();
    if (!enabled || window_open()) {
        GuiSetState(STATE_DISABLED);
    }
    int pressed = GuiToggleSlider(bounds, std::format("{};{}", off_label, on_label).data(), active);
    if (!enabled || window_open()) {
        GuiSetState(_state);
    }

    // Restore original styling
    set_default_style(original_default);
    set_component_style(SLIDER, original_slider);
    set_component_style(TOGGLE, original_toggle);

    return pressed;
}

auto user_interface::draw_menu_toggle_slider(const int x, const int y, const int width, const int height,
                                             const std::string& off_label, const std::string& on_label, int* active,
                                             const Color color, const bool enabled, const int font_size) const -> int
{
    const Rectangle bounds = menu_grid.bounds(x, y, width, height);
    return draw_toggle_slider(bounds, off_label, on_label, active, color, enabled, font_size);
}

auto user_interface::draw_spinner(Rectangle bounds, const std::string& label, int* value, int min, int max, Color color,
                                  bool enabled, int font_size) const -> int
{
    // Save original styling
    const default_style original_default = get_default_style();
    const component_style original_valuebox = get_component_style(VALUEBOX);
    const component_style original_button = get_component_style(BUTTON);

    // Change styling
    default_style style_default = original_default;
    component_style style_valuebox = original_valuebox;
    component_style style_button = original_button;
    style_default.text_size = font_size;
    apply_color(style_valuebox, color);
    apply_color(style_button, color);
    set_default_style(style_default);
    set_component_style(VALUEBOX, style_valuebox);
    set_component_style(BUTTON, style_button);

    const int _state = GuiGetState();
    if (!enabled || window_open()) {
        GuiSetState(STATE_DISABLED);
    }
    int pressed = GuiSpinner(bounds, "", label.data(), value, min, max, false);
    if (!enabled || window_open()) {
        GuiSetState(_state);
    }

    // Restore original styling
    set_default_style(original_default);
    set_component_style(VALUEBOX, original_valuebox);
    set_component_style(BUTTON, style_button);

    return pressed;
}

auto user_interface::draw_menu_spinner(const int x, const int y, const int width, const int height,
                                       const std::string& label, int* value, const int min, const int max,
                                       const Color color, const bool enabled, const int font_size) const -> int
{
    const Rectangle bounds = menu_grid.bounds(x, y, width, height);
    return draw_spinner(bounds, label, value, min, max, color, enabled, font_size);
}

auto user_interface::draw_label(const Rectangle bounds, const std::string& text, const Color color, const bool enabled,
                                const int font_size) const -> int
{
    // Save original styling
    const default_style original_default = get_default_style();
    const component_style original_label = get_component_style(LABEL);

    // Change styling
    default_style style_default = original_default;
    component_style style_label = original_label;
    style_default.text_size = font_size;
    apply_text_color(style_label, color);
    set_default_style(style_default);
    set_component_style(LABEL, style_label);

    const int _state = GuiGetState();
    if (!enabled || window_open()) {
        GuiSetState(STATE_DISABLED);
    }
    const int pressed = GuiLabel(bounds, text.data());
    if (!enabled || window_open()) {
        GuiSetState(_state);
    }

    // Restore original styling
    set_default_style(original_default);
    set_component_style(LABEL, original_label);

    return pressed;
}

auto user_interface::draw_board_block(const int x, const int y, const int width, const int height, const Color color,
                                      const bool enabled) const -> bool
{
    component_style s = get_component_style(BUTTON);
    apply_block_color(s, color);

    const Rectangle bounds = board_grid.square_bounds(x, y, width, height);

    const bool focused = CheckCollisionPointRec(input.mouse - Vector2(0, MENU_HEIGHT), bounds);
    const bool pressed = puzzle::block(x, y, width, height, false).covers(input.sel_x, input.sel_y);

    // Background to make faded colors work
    DrawRectangleRec(bounds, RAYWHITE);

    Color base = GetColor(s.base_color_normal);
    Color border = GetColor(s.base_color_normal);
    if (pressed) {
        base = GetColor(s.base_color_pressed);
        border = GetColor(s.base_color_pressed);
    }
    if (focused) {
        base = GetColor(s.base_color_focused);
        border = GetColor(s.base_color_focused);
    }
    if (focused && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        base = GetColor(s.base_color_pressed);
        border = GetColor(s.base_color_pressed);
    }
    if (!enabled) {
        base = BOARD_COLOR_RESTRICTED;
    }
    DrawRectangleRec(bounds, base);
    if (enabled) {
        DrawRectangleLinesEx(bounds, 2.0, border);
    }

    return focused && enabled;
}

auto user_interface::window_open() const -> bool
{
    return save_window || help_window;
}

auto user_interface::draw_menu_header(const Color color) const -> void
{
    int preset = state.get_current_preset();
    draw_menu_spinner(0, 0, 1, 1, "Preset: ", &preset, -1, state.get_preset_count(), color, !input.editing);
    if (preset > static_cast<int>(state.get_current_preset())) {
        input.load_next_preset();
    } else if (preset < static_cast<int>(state.get_current_preset())) {
        input.load_previous_preset();
    }

    draw_menu_button(1, 0, 1, 1,
                     std::format("Puzzle: \"{}\"{}", state.get_current_preset_comment().substr(2),
                                 state.was_edited() ? " (Modified)" : ""),
                     color);

    int editing = input.editing;
    draw_menu_toggle_slider(2, 0, 1, 1, "Puzzle Mode (Tab)", "Edit Mode (Tab)", &editing, color);
    if (editing != input.editing) {
        input.toggle_editing();
    }
}

auto user_interface::draw_graph_info(const Color color) const -> void
{
    draw_menu_button(0, 1, 1, 1,
                     std::format("Found {} States ({} Winning)", state.get_state_count(), state.get_target_count()),
                     color);

    draw_menu_button(1, 1, 1, 1, std::format("Found {} Transitions", state.get_link_count()), color);

    draw_menu_button(
        2, 1, 1, 1,
        std::format("{} Moves to Nearest Solution", state.get_path_length() > 0 ? state.get_path_length() - 1 : 0),
        color);
}

auto user_interface::draw_graph_controls(const Color color) const -> void
{
    if (draw_menu_button(0, 2, 1, 1, "Populate Graph (G)", color)) {
        input.populate_graph();
    }

    // int mark_path = input.mark_path;
    // DrawMenuToggleSlider(2, 2, 1, 1, "Path Hidden (U)", "Path Shown (U)",
    //                      &mark_path, color);
    // if (mark_path != input.mark_path) {
    //   input.ToggleMarkPath();
    // }

    if (draw_menu_button(1, 2, 1, 1, "Clear Graph (C)", color)) {
        input.clear_graph();
    }

    int mark_solutions = input.mark_solutions;
    draw_menu_toggle_slider(2, 2, 1, 1, "Solution Hidden (I)", "Solution Shown (I)", &mark_solutions, color);
    if (mark_solutions != input.mark_solutions) {
        input.toggle_mark_solutions();
    }
    input.mark_path = input.mark_solutions;
}

auto user_interface::draw_camera_controls(const Color color) const -> void
{
    int lock_camera = input.camera_lock;
    draw_menu_toggle_slider(0, 3, 1, 1, "Free Camera (L)", "Locked Camera (L)", &lock_camera, color);
    if (lock_camera != input.camera_lock) {
        input.toggle_camera_lock();
    }

    int lock_camera_mass_center = input.camera_mass_center_lock;
    draw_menu_toggle_slider(1, 3, 1, 1, "Current Block (U)", "Graph Center (U)", &lock_camera_mass_center, color,
                            input.camera_lock);
    if (lock_camera_mass_center != input.camera_mass_center_lock) {
        input.toggle_camera_mass_center_lock();
    }

    int projection = camera.projection == CAMERA_ORTHOGRAPHIC;
    draw_menu_toggle_slider(2, 3, 1, 1, "Perspective (Alt)", "Orthographic (Alt)", &projection, color);
    if (projection != (camera.projection == CAMERA_ORTHOGRAPHIC)) {
        input.toggle_camera_projection();
    }
}

auto user_interface::draw_puzzle_controls(const Color color) const -> void
{
    auto nth = [&](const int n)
    {
        if (n == 11 || n == 12 || n == 13) {
            return "th";
        }
        if (n % 10 == 1) {
            return "st";
        }
        if (n % 10 == 2) {
            return "nd";
        }
        if (n % 10 == 3) {
            return "rd";
        }
        return "th";
    };

    const int visits = state.get_current_visits();
    draw_menu_button(0, 4, 1, 1,
                     std::format("{} Moves ({}{} Time at this State)", state.get_total_moves(), visits, nth(visits)),
                     color);

    if (draw_menu_button(1, 4, 1, 1, "Make Optimal Move (Space)", color, state.has_distances())) {
        input.goto_optimal_next_state();
    }

    if (draw_menu_button(2, 4, 1, 1, "Undo Last Move (Backspace)", color, state.has_history())) {
        input.goto_previous_state();
    }

    if (draw_menu_button(0, 5, 1, 1, "Go to Nearest Solution (B)", color, state.has_distances())) {
        input.goto_closest_target_state();
    }

    if (draw_menu_button(1, 5, 1, 1, "Go to Worst State (V)", color, state.has_distances())) {
        input.goto_most_distant_state();
    }

    if (draw_menu_button(2, 5, 1, 1, "Go to Starting State (R)", color)) {
        input.goto_starting_state();
    }
}

auto user_interface::draw_edit_controls(const Color color) const -> void
{
    const puzzle& current = state.get_current_state();

    // Toggle Target Block
    if (draw_menu_button(0, 4, 1, 1, "Toggle Target Block (T)", color)) {
        input.toggle_target_block();
    }

    // Toggle Wall Block
    if (draw_menu_button(0, 5, 1, 1, "Toggle Wall Block (Y)", color)) {
        input.toggle_wall_block();
    }

    // Toggle Restricted/Free Block Movement
    int free = !current.restricted;
    draw_menu_toggle_slider(1, 4, 1, 1, "Restricted (F)", "Free (F)", &free, color);
    if (free != !current.restricted) {
        input.toggle_restricted_movement();
    }

    // Clear Goal
    if (draw_menu_button(1, 5, 1, 1, "Clear Goal (X)", color)) {
    }

    // Column Count Spinner
    int columns = current.width;
    draw_menu_spinner(2, 4, 1, 1, "Cols: ", &columns, puzzle::MIN_WIDTH, puzzle::MAX_WIDTH, color);
    if (columns > current.width) {
        input.add_board_column();
    } else if (columns < current.width) {
        input.remove_board_column();
    }

    // Row Count Spinner
    int rows = current.height;
    draw_menu_spinner(2, 5, 1, 1, "Rows: ", &rows, puzzle::MIN_WIDTH, puzzle::MAX_WIDTH, color);
    if (rows > current.height) {
        input.add_board_row();
    } else if (rows < current.height) {
        input.remove_board_row();
    }
}

auto user_interface::draw_menu_footer(const Color color) -> void
{
    draw_menu_button(0, 6, 2, 1, std::format("State: \"{}\"", state.get_current_state().state), color);

    if (draw_menu_button(2, 6, 1, 1, "Save as Preset", color)) {
        save_window = true;
    }
}

auto user_interface::get_background_color() -> Color
{
    return GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR));
}

auto user_interface::help_popup() -> void
{}

auto user_interface::draw_save_preset_popup() -> void
{
    if (!save_window) {
        return;
    }

    // Returns the pressed button index
    const int button =
        GuiTextInputBox(Rectangle((GetScreenWidth() - POPUP_WIDTH) / 2.0f, (GetScreenHeight() - POPUP_HEIGHT) / 2.0f,
                                  POPUP_WIDTH, POPUP_HEIGHT),
                        "Save as Preset", "Enter Preset Name", "Ok;Cancel", preset_name.data(), 255, nullptr);
    if (button == 1) {
        state.append_preset_file(preset_name.data());
    }
    if (button == 0 || button == 1 || button == 2) {
        save_window = false;
        TextCopy(preset_name.data(), "\0");
    }
}

auto user_interface::draw_main_menu() -> void
{
    menu_grid.update_bounds(0, 0, GetScreenWidth(), MENU_HEIGHT);

    draw_menu_header(GRAY);
    draw_graph_info(ORANGE);
    draw_graph_controls(RED);
    draw_camera_controls(DARKGREEN);

    if (input.editing) {
        draw_edit_controls(PURPLE);
    } else {
        draw_puzzle_controls(BLUE);
    }

    draw_menu_footer(GRAY);
}

auto user_interface::draw_puzzle_board() -> void
{
    const puzzle& current = state.get_current_state();

    board_grid.update_bounds(0, MENU_HEIGHT, GetScreenWidth() / 2, GetScreenHeight() - MENU_HEIGHT, current.width,
                             current.height);

    // Draw outer border
    const Rectangle bounds = board_grid.square_bounds();
    DrawRectangleRec(bounds, current.won() ? BOARD_COLOR_WON : BOARD_COLOR_RESTRICTED);

    // Draw inner borders
    DrawRectangle(bounds.x + BOARD_PADDING, bounds.y + BOARD_PADDING, bounds.width - 2 * BOARD_PADDING,
                  bounds.height - 2 * BOARD_PADDING, current.restricted ? BOARD_COLOR_RESTRICTED : BOARD_COLOR_FREE);

    // Draw target opening
    // TODO: Only draw single direction (in corner) if restricted (use target block principal direction)
    const std::optional<puzzle::block> target_block = current.try_get_target_block();
    if (current.has_win_condition() && target_block.has_value()) {
        const int target_x = current.target_x;
        const int target_y = current.target_y;
        auto [x, y, width, height] =
            board_grid.square_bounds(target_x, target_y, target_block.value().width, target_block.value().height);

        const Color opening_color = Fade(current.won() ? BOARD_COLOR_WON : BOARD_COLOR_RESTRICTED, 0.3);

        if (target_x == 0) {
            // Left opening
            DrawRectangle(x - BOARD_PADDING, y, BOARD_PADDING, height, RAYWHITE);
            DrawRectangle(x - BOARD_PADDING, y, BOARD_PADDING, height, opening_color);
        }
        if (target_x + target_block.value().width == current.width) {
            // Right opening
            DrawRectangle(x + width, y, BOARD_PADDING, height, RAYWHITE);
            DrawRectangle(x + width, y, BOARD_PADDING, height, opening_color);
        }
        if (target_y == 0) {
            // Top opening
            DrawRectangle(x, y - BOARD_PADDING, width, BOARD_PADDING, RAYWHITE);
            DrawRectangle(x, y - BOARD_PADDING, width, BOARD_PADDING, opening_color);
        }
        if (target_y + target_block.value().height == current.height) {
            // Bottom opening
            DrawRectangle(x, y + height, width, BOARD_PADDING, RAYWHITE);
            DrawRectangle(x, y + height, width, BOARD_PADDING, opening_color);
        }
    }

    // Draw empty cells. Also set hovered blocks
    input.hov_x = -1;
    input.hov_y = -1;
    for (int x = 0; x < board_grid.columns; ++x) {
        for (int y = 0; y < board_grid.rows; ++y) {
            DrawRectangleRec(board_grid.square_bounds(x, y, 1, 1), RAYWHITE);

            Rectangle hov_bounds = board_grid.square_bounds(x, y, 1, 1);
            hov_bounds.x -= BOARD_PADDING;
            hov_bounds.y -= BOARD_PADDING;
            hov_bounds.width += BOARD_PADDING;
            hov_bounds.height += BOARD_PADDING;
            if (CheckCollisionPointRec(GetMousePosition() - Vector2(0, MENU_HEIGHT), hov_bounds)) {
                input.hov_x = x;
                input.hov_y = y;
            }
        }
    }

    // Draw blocks
    for (const puzzle::block& b : current) {
        Color c = BLOCK_COLOR;
        if (b.target) {
            c = TARGET_BLOCK_COLOR;
        } else if (b.immovable) {
            c = WALL_COLOR;
        }

        if (!b.valid() || b.x < 0 || b.y < 0 || b.width <= 0 || b.height <= 0) {
            warnln("Iterator returned invalid block for state \"{}\".", current.state);
            continue;
        }

        draw_board_block(b.x, b.y, b.width, b.height, c, !b.immovable);
    }

    // Draw block placing
    if (input.editing && input.has_block_add_xy) {
        if (current.covers(input.block_add_x, input.block_add_y) && input.hov_x >= input.block_add_x &&
            input.hov_y >= input.block_add_y) {
            bool collides = false;
            for (const puzzle::block& b : current) {
                if (b.collides(puzzle::block(input.block_add_x, input.block_add_y, input.hov_x - input.block_add_x + 1,
                                             input.hov_y - input.block_add_y + 1, false))) {
                    collides = true;
                    break;
                }
            }
            if (!collides) {
                draw_board_block(input.block_add_x, input.block_add_y, input.hov_x - input.block_add_x + 1,
                                 input.hov_y - input.block_add_y + 1, PURPLE);
            }
        }
    }
}

auto user_interface::draw_graph_overlay(int fps, int ups, size_t mass_count, size_t spring_count) -> void
{
    graph_overlay_grid.update_bounds(GetScreenWidth() / 2, MENU_HEIGHT);
    debug_overlay_grid.update_bounds(GetScreenWidth() / 2, GetScreenHeight() - 75);

    draw_label(graph_overlay_grid.bounds(0, 0, 1, 1), std::format("Dist: {:0>7.2f}", camera.distance), BLACK);
    draw_label(graph_overlay_grid.bounds(0, 1, 1, 1), std::format("FoV:  {:0>6.2f}", camera.fov), BLACK);
    draw_label(graph_overlay_grid.bounds(0, 2, 1, 1), std::format("FPS:  {:0>3}", fps), LIME);
    draw_label(graph_overlay_grid.bounds(0, 3, 1, 1), std::format("UPS:  {:0>3}", ups), ORANGE);

    // Debug
    draw_label(debug_overlay_grid.bounds(0, 0, 1, 1), std::format("Debug:"), BLACK);
    draw_label(debug_overlay_grid.bounds(0, 1, 1, 1), std::format("Masses:  {}", mass_count), BLACK);
    draw_label(debug_overlay_grid.bounds(0, 2, 1, 1), std::format("Springs: {}", spring_count), BLACK);
}

auto user_interface::update() const -> void
{
    input.disable = window_open();
}
