#include "user_interface.hpp"
#include "config.hpp"
#include "gpu_context.hpp"
#include "input_handler.hpp"
#include "layout.hpp"
#include "memory_tracker.hpp"
#include "settings.hpp"

#include <algorithm>
#include <cmath>
#include <raylib.h>
#include <thread>
#include <vector>

#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

namespace
{
// Draws text with simple word wrapping and returns the y coordinate below the last line.
auto draw_wrapped_text(const std::string& text, const int x, int y, const int max_width,
                       const Color color, const int font_size, const int hanging_indent = 0) -> int
{
    if (max_width <= 0) {
        return y;
    }

    const float spacing = static_cast<float>(GuiGetStyle(DEFAULT, TEXT_SPACING));
    const float line_height = static_cast<float>(font_size + 4);
    const auto text_width = [&](const std::string& value)
    {
        return MeasureTextEx(GuiGetFont(), value.c_str(), static_cast<float>(font_size), spacing).x;
    };

    size_t paragraph_start = 0;
    while (paragraph_start <= text.size()) {
        const size_t paragraph_end = text.find('\n', paragraph_start);
        const std::string paragraph =
            text.substr(paragraph_start,
                        paragraph_end == std::string::npos ? std::string::npos
                                                           : paragraph_end - paragraph_start);
        paragraph_start = paragraph_end == std::string::npos ? text.size() + 1 : paragraph_end + 1;

        if (paragraph.empty()) {
            y += static_cast<int>(line_height);
            continue;
        }

        bool first_line = true;
        std::string line;
        size_t word_start = 0;
        while (word_start <= paragraph.size()) {
            const size_t word_end = paragraph.find(' ', word_start);
            const std::string word = paragraph.substr(
                word_start,
                word_end == std::string::npos ? std::string::npos : word_end - word_start);
            word_start = word_end == std::string::npos ? paragraph.size() + 1 : word_end + 1;

            const std::string candidate = line.empty() ? word : line + " " + word;
            if (!line.empty() &&
                text_width(candidate) >
                    static_cast<float>(max_width) - (first_line ? 0.0f : hanging_indent)) {
                DrawTextEx(GuiGetFont(), line.c_str(),
                           Vector2{static_cast<float>(x + (first_line ? 0 : hanging_indent)),
                                   static_cast<float>(y)},
                           static_cast<float>(font_size), spacing, color);
                y += static_cast<int>(line_height);
                first_line = false;
                line = word;
            } else {
                line = candidate;
            }
        }

        if (!line.empty()) {
            DrawTextEx(GuiGetFont(), line.c_str(),
                       Vector2{static_cast<float>(x + (first_line ? 0 : hanging_indent)),
                               static_cast<float>(y)},
                       static_cast<float>(font_size), spacing, color);
            y += static_cast<int>(line_height);
        }
    }

    return y;
}
} // namespace

auto user_interface::grid::update_bounds(const int _x, const int _y, const int _width,
                                         const int _height, const int _columns, const int _rows)
    -> void
{
    x = _x;
    y = _y;
    width = _width;
    height = _height;
    columns = _columns;
    rows = _rows;
}

auto user_interface::grid::update_bounds(const int _x, const int _y, const int _width,
                                         const int _height) -> void
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

auto user_interface::grid::bounds(const int _x, const int _y, const int _width,
                                  const int _height) const -> Rectangle
{
    if (_x < 0 || _x + _width > columns || _y < 0 || _y + _height > rows) {
        throw std::invalid_argument("Grid bounds out of range");
    }

    const int cell_width = (width - padding) / columns;
    const int cell_height = (height - padding) / rows;

    return Rectangle(x + _x * cell_width + padding, y + _y * cell_height + padding,
                     _width * cell_width - padding, _height * cell_height - padding);
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

auto user_interface::grid::square_bounds(const int _x, const int _y, const int _width,
                                         const int _height) const -> Rectangle
{
    // Assumes each cell is square, so either width or height are not completely
    // filled

    if (_x < 0 || _x + _width > columns || _y < 0 || _y + _height > rows) {
        throw std::invalid_argument("Grid bounds out of range");
    }

    const int available_width = width - padding * (columns + 1);
    const int available_height = height - padding * (rows + 1);
    const int cell_size = std::min(available_width / columns, available_height / rows);

    const int grid_width = cell_size * columns + padding * (columns + 1);
    const int grid_height = cell_size * rows + padding * (rows + 1);
    const int x_offset = (width - grid_width) / 2;
    const int y_offset = (height - grid_height) / 2;

    return Rectangle(x_offset + _x * (cell_size + padding) + padding,
                     y_offset + _y * (cell_size + padding) + padding,
                     _width * cell_size + padding * (_width - 1),
                     _height * cell_size + padding * (_height - 1));
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
    style.base_color_focused = ColorToInt(Fade(color, 0.55));
    style.base_color_pressed = ColorToInt(Fade(color, 0.9));
    style.base_color_disabled = ColorToInt(Fade(color, 0.5));

    style.border_color_normal = ColorToInt(Fade(color, 1.0));
    style.border_color_focused = ColorToInt(Fade(color, 1.0));
    style.border_color_pressed = ColorToInt(Fade(color, 1.0));
    style.border_color_disabled = ColorToInt(Fade(GRAY, 0.5));

    style.text_color_normal = ColorToInt(Fade(BLACK, 1.0));
    style.text_color_focused = ColorToInt(Fade(BLACK, 1.0));
    style.text_color_pressed = ColorToInt(Fade(BLACK, 1.0));
    style.text_color_disabled = ColorToInt(Fade(BLACK, 0.5));
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
    return {
        {GuiGetStyle(component, BORDER_COLOR_NORMAL), GuiGetStyle(component, BASE_COLOR_NORMAL),
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

auto user_interface::popup_bounds() -> Rectangle
{
    return Rectangle(static_cast<float>(GetScreenWidth()) / 2.0f - POPUP_WIDTH / 2.0f,
                     static_cast<float>(GetScreenHeight()) / 2.0f - POPUP_HEIGHT / 2.0f,
                     POPUP_WIDTH, POPUP_HEIGHT);
}

auto user_interface::draw_button(const Rectangle bounds, const std::string& label,
                                 const Color color, const bool enabled, const int font_size) const
    -> int
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
                                      const std::string& label, const Color color,
                                      const bool enabled, const int font_size) const -> int
{
    const Rectangle bounds = active_grid->bounds(x, y, width, height);
    const int pressed = draw_button(bounds, label, color, enabled, font_size);
    return pointer_in_active_bounds() ? pressed : 0;
}

auto user_interface::draw_toggle_slider(const Rectangle bounds, const std::string& off_label,
                                        const std::string& on_label, int* active, Color color,
                                        bool enabled, int font_size) const -> int
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

auto user_interface::draw_menu_toggle_slider(const int x, const int y, const int width,
                                             const int height, const std::string& off_label,
                                             const std::string& on_label, int* active,
                                             const Color color, const bool enabled,
                                             const int font_size) const -> int
{
    const Rectangle bounds = active_grid->bounds(x, y, width, height);
    const int previous = *active;
    const int pressed =
        draw_toggle_slider(bounds, off_label, on_label, active, color, enabled, font_size);
    if (!pointer_in_active_bounds()) {
        *active = previous;
        return 0;
    }
    return pressed;
}

auto user_interface::draw_spinner(Rectangle bounds, const std::string& label, int* value, int min,
                                  int max, Color color, bool enabled, int font_size) const -> int
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
                                       const std::string& label, int* value, const int min,
                                       const int max, const Color color, const bool enabled,
                                       const int font_size) const -> int
{
    const Rectangle bounds = active_grid->bounds(x, y, width, height);
    const int previous = *value;
    const int result = draw_spinner(bounds, label, value, min, max, color, enabled, font_size);
    if (!pointer_in_active_bounds()) {
        *value = previous;
        return previous;
    }
    return result;
}

auto user_interface::draw_slider(const Rectangle bounds, const std::string& label, const int value,
                                 const int min, const int max, const Color color,
                                 const bool enabled, const int font_size) const -> int
{
    // Save original styling
    const default_style original_default = get_default_style();
    const component_style original_slider = get_component_style(SLIDER);

    // Change styling
    default_style style_default = original_default;
    component_style style_slider = original_slider;
    style_default.text_size = font_size;
    apply_color(style_slider, color);
    set_default_style(style_default);
    set_component_style(SLIDER, style_slider);

    const int _state = GuiGetState();
    if (!enabled || window_open()) {
        GuiSetState(STATE_DISABLED);
    }
    float slider_value = static_cast<float>(value);
    const std::string value_text = std::to_string(value);
    GuiSlider(bounds, label.c_str(), value_text.c_str(), &slider_value, static_cast<float>(min),
              static_cast<float>(max));
    if (!enabled || window_open()) {
        GuiSetState(_state);
    }

    // Restore original styling
    set_default_style(original_default);
    set_component_style(SLIDER, original_slider);

    return std::clamp(static_cast<int>(std::lround(slider_value)), min, max);
}

auto user_interface::draw_menu_slider(const int x, const int y, const int width, const int height,
                                      const std::string& label, const int value, const int min,
                                      const int max, const Color color, const bool enabled,
                                      const int font_size) const -> int
{
    const Rectangle bounds = active_grid->bounds(x, y, width, height);
    const int result = draw_slider(bounds, label, value, min, max, color, enabled, font_size);
    return pointer_in_active_bounds() ? result : value;
}

auto user_interface::draw_progress(const Rectangle bounds, const std::string& label,
                                   const float fraction, const Color color,
                                   const bool indeterminate) const -> void
{
    const default_style original_default = get_default_style();
    const component_style original_progress = get_component_style(PROGRESSBAR);

    default_style style_default = original_default;
    component_style style_progress = original_progress;
    style_default.text_size = FONT_SIZE - 4;
    apply_color(style_progress, color);
    set_default_style(style_default);
    set_component_style(PROGRESSBAR, style_progress);

    const int _state = GuiGetState();
    if (window_open()) {
        GuiSetState(STATE_DISABLED);
    }

    float value = std::clamp(fraction, 0.0f, 1.0f);
    std::string text = label;
    if (indeterminate) {
        value = 0.35f;
        text = std::format("{} ...", label);
    } else {
        text = std::format("{}  {:.0f}%", label, value * 100.0f);
    }
    GuiProgressBar(bounds, "", text.c_str(), &value, 0.0f, 1.0f);

    if (window_open()) {
        GuiSetState(_state);
    }

    set_default_style(original_default);
    set_component_style(PROGRESSBAR, original_progress);
}

auto user_interface::progress(const int x, const int y, const int width, const int height,
                              const std::string& label, const float fraction, const Color color,
                              const bool indeterminate) -> void
{
    draw_progress(active_grid->bounds(x, y, width, height), label, fraction, color, indeterminate);
}

auto user_interface::draw_menu_enum_spinner(const int x, const int y, const int width,
                                            const int height, const std::string& label,
                                            const std::vector<std::string>& values, int* index,
                                            const Color color, const bool enabled) const -> int
{
    const Rectangle bounds = active_grid->bounds(x, y, width, height);
    constexpr float arrow_width = 28.0f;

    const Rectangle left{bounds.x, bounds.y, arrow_width, bounds.height};
    const Rectangle right{bounds.x + bounds.width - arrow_width, bounds.y, arrow_width,
                          bounds.height};
    const Rectangle center{bounds.x + arrow_width, bounds.y, bounds.width - 2 * arrow_width,
                           bounds.height};

    int current = values.empty() ? 0 : std::clamp(*index, 0, static_cast<int>(values.size()) - 1);
    const bool can_go_left = enabled && current > 0;
    const bool can_go_right =
        enabled && !values.empty() && current + 1 < static_cast<int>(values.size());

    const bool pointer_ok = pointer_in_active_bounds();
    if (draw_button(left, "<", color, can_go_left) && pointer_ok) {
        --current;
    }
    if (draw_button(right, ">", color, can_go_right) && pointer_ok) {
        ++current;
    }

    const std::string text =
        values.empty() ? label : std::format("{}{}", label, values[static_cast<size_t>(current)]);

    const int alignment = GuiGetStyle(LABEL, TEXT_ALIGNMENT);
    GuiSetStyle(LABEL, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER);
    draw_label(center, text, color, enabled);
    GuiSetStyle(LABEL, TEXT_ALIGNMENT, alignment);

    *index = current;
    return current;
}

auto user_interface::draw_label(const Rectangle bounds, const std::string& text, const Color color,
                                const bool enabled, const int font_size) const -> int
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

auto user_interface::button(const int x, const int y, const int width, const int height,
                            const std::string& text, const Color color, const bool enabled) -> bool
{
    const bool pressed =
        draw_menu_button(x, y, width, height, text, color, enabled, controls_font_size) != 0;
    return pressed && pointer_in_active_bounds();
}

auto user_interface::toggle(const int x, const int y, const int width, const int height,
                            const std::string& off_label, const std::string& on_label,
                            const bool value, const Color color, const bool enabled) -> bool
{
    int active = value ? 1 : 0;
    draw_menu_toggle_slider(x, y, width, height, off_label, on_label, &active, color, enabled,
                            controls_font_size);
    return pointer_in_active_bounds() ? active != 0 : value;
}

auto user_interface::checkbox(const int x, const int y, const int width, const int height,
                              const std::string& label_text, const bool value, const Color color,
                              const bool enabled) -> bool
{
    bool active = value;
    const Rectangle bounds = active_grid->bounds(x, y, width, height);
    const int state = GuiGetState();
    if (!enabled || window_open()) {
        GuiSetState(STATE_DISABLED);
    }
    GuiCheckBox(Rectangle{bounds.x + 2.0f, bounds.y + (bounds.height - 22.0f) / 2.0f, 22.0f, 22.0f},
                "", &active);
    GuiSetState(state);
    draw_label(Rectangle{bounds.x + 30.0f, bounds.y, bounds.width - 30.0f, bounds.height},
               label_text, enabled ? color : GRAY, enabled, controls_font_size);
    return pointer_in_active_bounds() ? active : value;
}

auto user_interface::spinner(const int x, const int y, const int width, const int height,
                             const std::string& label_text, const int value, const int min,
                             const int max, const Color color, const bool enabled) -> int
{
    int current = value;
    draw_menu_spinner(x, y, width, height, label_text, &current, min, max, color, enabled,
                      controls_font_size);
    return pointer_in_active_bounds() ? current : value;
}

auto user_interface::slider(const int x, const int y, const int width, const int height,
                            const std::string& label_text, const int value, const int min,
                            const int max, const Color color, const bool enabled) -> int
{
    const int current = draw_menu_slider(x, y, width, height, label_text, value, min, max, color,
                                         enabled, controls_font_size);
    return pointer_in_active_bounds() ? current : value;
}

auto user_interface::label(const int x, const int y, const int width, const int height,
                           const std::string& text, const Color color, const bool enabled) -> void
{
    draw_label(active_grid->bounds(x, y, width, height), text, color, enabled, controls_font_size);
}

auto user_interface::window_open() const -> bool
{
    return save_window || puzzle_space_file_window || help_window || ok_message || yes_no_message;
}

auto user_interface::pointer_in_active_bounds() const -> bool
{
    return !active_input_bounds || CheckCollisionPointRec(GetMousePosition(), *active_input_bounds);
}

auto user_interface::draw_menu_header(const Color color) const -> void
{
    const bool editing = state.presenter().editing();
    const bool custom_state = state.has_custom_state();

    // The preset spinner only walks the presets of the active puzzle type; the
    // puzzle type itself is selected with the spinner on the right. The range is
    // drawn one past the end so clicking past the last entry wraps to the first.
    const size_t type_presets = state.get_type_preset_count();
    const size_t type_position = state.get_type_preset_position();
    int preset = custom_state ? -1 : static_cast<int>(type_position);
    draw_menu_spinner(0, 0, 1, 1, "Preset: ", &preset, -1,
                      type_presets > 0 ? static_cast<int>(type_presets) : -1, color,
                      !custom_state && type_presets > 0 && !editing);
    if (!custom_state && type_presets > 0) {
        if (preset >= static_cast<int>(type_presets) || preset > static_cast<int>(type_position)) {
            input.load_next_preset();
        } else if (preset < static_cast<int>(type_position)) {
            input.load_previous_preset();
        }
    }

    // Loading a preset may replace the active puzzle kind, so fetch the presenter only after the
    // switch. Holding a reference across load_next/previous_preset would dangle.
    puzzle_presenter& presenter = state.presenter();
    const bool current_editing = presenter.editing();

    const bool searching = state.search_is_running();
    const bool custom_name = searching ? state.search_origin_is_custom() : custom_state;
    const std::string& comment_text =
        searching ? state.get_search_origin_comment() : state.get_current_preset_comment();
    const std::string stripped = comment_text.size() >= 2 ? comment_text.substr(2) : comment_text;
    std::string name;
    if (searching) {
        name = custom_name ? stripped : std::format("\"{}\"", stripped);
    } else {
        name = state.was_edited() ? "Modified"
                                  : (custom_name ? stripped : std::format("\"{}\"", stripped));
    }
    const std::string status = searching
        ? state.get_search_origin_status()
        : presenter.status_text(state.model(), state.get_current_state_id());
    draw_menu_button(1, 0, 1, 1, status.empty() ? name : std::format("{}: {}", name, status),
                     color);

    const std::vector<std::string> types = state.available_puzzle_types();
    int puzzle_index = 0;
    for (size_t i = 0; i < types.size(); ++i) {
        if (types[i] == state.current_puzzle_type()) {
            puzzle_index = static_cast<int>(i);
            break;
        }
    }

    int next_puzzle = puzzle_index;
    draw_menu_enum_spinner(2, 0, 1, 1, "Puzzle: ", types, &next_puzzle, color,
                           types.size() > 1 && !current_editing);
    if (next_puzzle != puzzle_index && next_puzzle >= 0 &&
        next_puzzle < static_cast<int>(types.size())) {
        state.switch_puzzle(types[static_cast<size_t>(next_puzzle)]);
    }
}

auto user_interface::draw_graph_info(const Color color, const int row) const -> int
{
    draw_menu_button(
        0, row, 2, 1,
        std::format("States: {} ({} winning)", state.get_state_count(), state.get_target_count()),
        color, true, controls_font_size);

    draw_menu_button(0, row + 1, 2, 1, std::format("Transitions: {}", state.get_link_count()),
                     color, true, controls_font_size);

    draw_menu_button(
        0, row + 2, 2, 1,
        state.is_graph_complete()
            ? std::format("{} Moves to Nearest Solution",
                          state.get_path_length() > 0 ? state.get_path_length() - 1 : 0)
            : "Local Graph View",
        color, true, controls_font_size);

    return row + 3;
}

auto user_interface::draw_graph_controls(const Color color, const int row) const -> int
{
    const bool state_space_running =
        state.search_is_running() && state.get_search_kind() == search_kind::state_space;
    const bool other_search_running =
        state.search_is_running() && state.get_search_kind() != search_kind::state_space;
    if (draw_menu_button(0, row, 1, 1,
                         other_search_running      ? "Search Running"
                             : state_space_running ? "Cancel (G)"
                                                   : "Populate (G)",
                         color, !other_search_running, controls_font_size)) {
        input.populate_graph();
    }

    if (draw_menu_button(1, row, 1, 1, "Clear (C)", color, true, controls_font_size)) {
        input.clear_graph();
    }

    int mark_solutions = input.mark_solutions;
    draw_menu_toggle_slider(0, row + 1, 2, 1, "Solution Off (I)", "Solution On (I)",
                            &mark_solutions, color, true, controls_font_size);
    if (mark_solutions != input.mark_solutions) {
        input.toggle_mark_solutions();
    }
    input.mark_path = input.mark_solutions;

    simulation_settings layout_cfg = get_settings();
    int planar = layout_cfg.planar_graph ? 1 : 0;
    draw_menu_toggle_slider(0, row + 2, 2, 1, "3D Graph", "2D Graph", &planar, color, true,
                            controls_font_size);
    if ((planar != 0) != layout_cfg.planar_graph) {
        layout_cfg.planar_graph = planar != 0;
        set_settings(layout_cfg);
        camera.set_planar(layout_cfg.planar_graph);
    }

    int next_row = row + 3;
    const search_snapshot snapshot = state.get_search_snapshot();
    if (state_space_running) {
        draw_progress(active_grid->bounds(0, next_row, 2, 1),
                      std::format("Exploring: {} states | {}",
                                  format_count(snapshot.states_explored),
                                  format_duration(snapshot.elapsed_seconds)),
                      0.0f, color, true);
        ++next_row;
    } else if (snapshot.kind == search_kind::state_space &&
               snapshot.status == search_status::failed && !snapshot.error.empty()) {
        draw_label(active_grid->bounds(0, next_row, 2, 1), snapshot.error, RED, true,
                   controls_font_size);
        ++next_row;
    }
    return next_row;
}

auto user_interface::draw_camera_controls(const Color color, const int row) const -> int
{
    int lock_camera = input.camera_lock;
    draw_menu_toggle_slider(0, row, 1, 1, "Free (L)", "Locked (L)", &lock_camera, color, true,
                            controls_font_size);
    if (lock_camera != input.camera_lock) {
        input.toggle_camera_lock();
    }

    int lock_camera_mass_center = input.camera_mass_center_lock;
    draw_menu_toggle_slider(1, row, 1, 1, "Block (U)", "Graph (U)", &lock_camera_mass_center, color,
                            input.camera_lock, controls_font_size);
    if (lock_camera_mass_center != input.camera_mass_center_lock) {
        input.toggle_camera_mass_center_lock();
    }

    int projection = camera.projection == CAMERA_ORTHOGRAPHIC;
    draw_menu_toggle_slider(0, row + 1, 2, 1, "Perspective (Alt)", "Ortho (Alt)", &projection,
                            color, !camera.planar, controls_font_size);
    if (projection != (camera.projection == CAMERA_ORTHOGRAPHIC)) {
        input.toggle_camera_projection();
    }

    return row + 2;
}

auto user_interface::get_background_color() -> Color
{
    return GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR));
}

auto user_interface::help_popup() -> void
{
    if (!help_window) {
        return;
    }

    const int screen_width = GetScreenWidth();
    const int screen_height = GetScreenHeight();
    const float margin =
        std::min(24.0f, static_cast<float>(std::min(screen_width, screen_height)) * 0.04f);
    const float panel_width = std::min(1100.0f, static_cast<float>(screen_width) - 2.0f * margin);
    const float panel_height = std::min(760.0f, static_cast<float>(screen_height) - 2.0f * margin);
    const Rectangle panel{(static_cast<float>(screen_width) - panel_width) / 2.0f,
                          (static_cast<float>(screen_height) - panel_height) / 2.0f, panel_width,
                          panel_height};

    DrawRectangle(0, 0, screen_width, screen_height, Fade(BLACK, 0.38f));
    draw_overlay_panel(panel);

    constexpr int padding = 16;
    const int inner_left = static_cast<int>(panel.x) + padding;
    const int inner_right = static_cast<int>(panel.x + panel.width) - padding;
    const int content_width = inner_right - inner_left;

    const int title_size = std::clamp(screen_height / 24, 20, 30);
    const int tab_size = std::clamp(screen_height / 36, 14, 18);
    const int body_size = std::clamp(screen_height / 34, 16, 19);
    const int heading_size = body_size + 3;

    draw_overlay_text("Help", inner_left, static_cast<int>(panel.y) + 10, BLACK, title_size);

    const Rectangle close_bounds{static_cast<float>(inner_right - 88),
                                 static_cast<float>(panel.y) + 10.0f, 88.0f, 30.0f};
    const Vector2 mouse = GetMousePosition();
    const bool close_hover = CheckCollisionPointRec(mouse, close_bounds);
    DrawRectangleRec(close_bounds, close_hover ? Fade(RED, 0.35f) : Fade(GRAY, 0.45f));
    DrawRectangleLinesEx(close_bounds, 1.0f, BLACK);
    const Vector2 close_text_size =
        MeasureTextEx(GuiGetFont(), "Close (?)", static_cast<float>(tab_size),
                      static_cast<float>(GuiGetStyle(DEFAULT, TEXT_SPACING)));
    draw_overlay_text(
        "Close (?)",
        static_cast<int>(close_bounds.x + (close_bounds.width - close_text_size.x) / 2),
        static_cast<int>(close_bounds.y + (close_bounds.height - close_text_size.y) / 2), BLACK,
        tab_size);
    if (close_hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        help_window = false;
        return;
    }

    const std::array<std::pair<help_section, const char*>, 3> tabs = {{
        {help_section::hotkeys, "Hotkeys"},
        {help_section::state_space, "State Space"},
        {help_section::puzzle_space, "Puzzle Space"},
    }};

    const Color hotkeys_color = DARKBLUE;
    const Color state_space_color = ORANGE;
    const Color puzzle_space_color = SEARCH_COLOR;
    const auto tab_color = [&](const help_section section)
    {
        switch (section) {
        case help_section::hotkeys:
            return hotkeys_color;
        case help_section::state_space:
            return state_space_color;
        case help_section::puzzle_space:
            return puzzle_space_color;
        }
        return GRAY;
    };

    constexpr int tab_gap = 6;
    constexpr int tab_height = 32;
    const int tab_top = static_cast<int>(panel.y) + 50;
    const int tab_width = (content_width - 2 * tab_gap) / 3;
    for (size_t i = 0; i < tabs.size(); ++i) {
        const auto& [section, label] = tabs[i];
        const Rectangle bounds{
            static_cast<float>(inner_left + static_cast<int>(i) * (tab_width + tab_gap)),
            static_cast<float>(tab_top), static_cast<float>(tab_width),
            static_cast<float>(tab_height)};
        const bool selected = help_tab == section;
        const bool hovered = CheckCollisionPointRec(mouse, bounds);
        const Color accent = tab_color(section);
        DrawRectangleRec(bounds, selected ? accent : Fade(GRAY, hovered ? 0.7f : 0.45f));
        DrawRectangleLinesEx(bounds, 1.0f, BLACK);
        const Vector2 label_size =
            MeasureTextEx(GuiGetFont(), label, static_cast<float>(tab_size),
                          static_cast<float>(GuiGetStyle(DEFAULT, TEXT_SPACING)));
        draw_overlay_text(label, static_cast<int>(bounds.x + (bounds.width - label_size.x) / 2.0f),
                          static_cast<int>(bounds.y + (bounds.height - label_size.y) / 2.0f), BLACK,
                          tab_size);
        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !selected) {
            help_tab = section;
            help_scroll = 0.0f;
        }
    }

    const int content_top = tab_top + tab_height + 10;
    const int content_bottom = static_cast<int>(panel.y + panel.height) - 34;
    const int content_height = std::max(1, content_bottom - content_top);

    struct help_entry
    {
        std::string heading;
        std::vector<std::string> lines;
    };
    std::vector<help_entry> entries;

    if (help_tab == help_section::hotkeys) {
        entries = {
            {"General",
             {"- ?  Toggle this help overlay", "- ,  Open or close Settings",
              "- Ctrl+S  Save the current board as a preset", "- N / M  Previous / next preset",
              "- R  Return to the start of the move history",
              "- P  Print the current state to the log", "- Tab  Toggle sliding-block edit mode"}},
            {"Graph and camera",
             {"- G  Populate the state-space graph; press again to cancel",
              "- C  Clear the graph and start over at the current state",
              "- Space  Step to the optimal next state",
              "- Backspace  Go back one move in the history",
              "- B / V  Jump to the nearest / farthest goal state",
              "- I  Mark goal states and the solution path",
              "- O  Draw lines from the current state to all goal states",
              "- Z  Color nodes by their distance to a goal",
              "- Left drag  Pan    Right drag  Rotate    Wheel  Zoom",
              "- Ctrl+Wheel  Field of view    Alt  Perspective / ortho",
              "- L  Lock the camera    U  Lock it to the graph center",
              "- Q + middle click  Select the state under the cursor"}},
            {"Board",
             {"- Click or drag pieces / cells to play; right-click clears in Sudoku",
              "- WASD or arrows  Move (sliding-block and 8-puzzle)",
              "- F2  Randomize    F3  Reset (puzzle-dependent)", "- 1-4  Select a digit (Sudoku)"}},
            {"Sliding-block edit mode (Tab)",
             {"- T  Toggle target    Y  Toggle wall    F  Free / restricted",
              "- X  Clear goal    Arrows  Add / remove a row or column",
              "- Left-click an empty cell, then the opposite corner to add a block",
              "- Right-click  Remove a block    Middle-click  Place the goal"}},
        };
    } else if (help_tab == help_section::state_space) {
        entries = {
            {"What this mode does",
             {"- Explores one concrete puzzle board. Populate (G) walks every legal move "
              "reachable from the current state and builds the complete reachable graph.",
              "- The search runs in the background and can be cancelled with G. Moves you make "
              "on the board also add states and edges to the graph as you go."}},
            {"What the graph shows",
             {"- Node = one board state (one arrangement). Edge = one legal transition between "
              "two states.",
              "- The layout is a live mass-spring simulation: connected states attract, all "
              "nodes repel. Drag and zoom the camera to inspect it.",
              "- States counts nodes, Transitions edges, and (N winning) the goal states. Once "
              "complete, the panel reports the shortest distance to the nearest solution; "
              "otherwise the graph is only a local view.",
              "- 2D Graph flattens the layout; 3D is the default."}},
            {"Colors and overlays",
             {"- Blue: unvisited. Orange: start and states already visited. Green: goal states "
              "or the current solution path. Red: node under the cursor while Q is held.",
              "- I marks goals and the solution path. O connects the current state to every "
              "goal. Z fades nodes from pink (closest) to dark blue (farthest)."}},
            {"Navigating",
             {"- Q + middle click selects a node. Space follows an optimal move, Backspace "
              "returns in the move history, B / V jump to the nearest / farthest goal, and R "
              "returns to the start."}},
        };
    } else {
        entries = {
            {"What this mode does",
             {"- Searches a family of puzzles instead of one board. Board size, goal, movement "
              "rule, target and walls come from the current board; walls are fixed.",
              "- The builder tries every block inventory (multiset of movable shapes) between "
              "Min and Max Blocks. Each inventory becomes one partition.",
              "- Min / Max Blocks count the target plus movable blocks; walls are excluded. "
              "Min Moves discards clusters whose hardest solution is shorter."}},
            {"Partitions and clusters",
             {"- A partition is one exact set of block shapes. A move never changes that set, "
              "so partitions are explored independently and in parallel.",
              "- A cluster is one connected, solvable group of placements inside a partition. "
              "Selecting a result loads its hardest state; harder clusters need more moves."}},
            {"Search phases",
             {"- Preparing search: build block inventories and candidate starting positions.",
              "- Counting placements: fast exact count of every distinct placement, and of "
              "the whole search space.",
              "- Exploring placements: build the partition work list and memory estimate, then "
              "enumerate and index each partition, find components and distances.",
              "- Sorting results: rank and display the discovered clusters."}},
            {"Reading the progress items",
             {"- Progress: candidates already explored / candidates counted. Clusters: matching "
              "clusters found so far.",
              "- Building index: builds a perfect hash from placement key to state id. The "
              "percentage is the slowest in-flight build; W indexing = workers building, "
              "P partitions = active partitions, S states = size of the partition being built.",
              "- Aggregating: the component and distance sweep over the indexed placements; the "
              "same counters apply.",
              "- Elapsed / ETA: wall time and a smoothed estimate based on the current rate."}},
            {"Results",
             {"- Each result lists blocks, hardest solution length and state count. Moves First "
              "ranks by moves; Blocks First ranks by blocks, then fewest states, then moves.",
              "- Save Space / Load Space write and read results. To explore a loaded result, "
              "switch to State Space and press G."}},
        };
    }

    const Color accent = tab_color(help_tab);
    const int text_width = content_width - 12;
    BeginScissorMode(inner_left, content_top, content_width, content_height);
    int y = content_top - static_cast<int>(help_scroll);
    for (const help_entry& entry : entries) {
        draw_overlay_text(entry.heading, inner_left, y, accent, heading_size);
        y += heading_size + 6;
        for (const std::string& line : entry.lines) {
            y = draw_wrapped_text(line, inner_left, y, text_width, BLACK, body_size, 14);
        }
        y += 8;
    }
    EndScissorMode();

    const int content_used = y + static_cast<int>(help_scroll) - content_top;
    const int max_scroll = std::max(0, content_used - content_height);
    if (CheckCollisionPointRec(
            mouse,
            Rectangle{static_cast<float>(inner_left), static_cast<float>(content_top),
                      static_cast<float>(content_width), static_cast<float>(content_height)})) {
        help_scroll -= GetMouseWheelMove() * 48.0f;
    }
    help_scroll = Clamp(help_scroll, 0.0f, static_cast<float>(max_scroll));

    if (max_scroll > 0) {
        const float track_height = static_cast<float>(content_height);
        const float thumb_height = std::max(24.0f,
                                            track_height * static_cast<float>(content_height) /
                                                static_cast<float>(content_used));
        const float thumb_y = static_cast<float>(content_top) +
            (track_height - thumb_height) * (help_scroll / static_cast<float>(max_scroll));
        const int scrollbar_x = inner_left + content_width - 5;
        DrawRectangle(scrollbar_x, content_top, 5, content_height, Fade(BLACK, 0.08f));
        DrawRectangle(scrollbar_x, static_cast<int>(thumb_y), 5, static_cast<int>(thumb_height),
                      Fade(BLACK, 0.45f));
    }

    draw_overlay_text("Mouse wheel scrolls    ? closes", inner_left,
                      static_cast<int>(panel.y + panel.height) - 24, DARKGRAY, body_size - 2);
}

auto user_interface::draw_save_preset_popup() -> void
{
    if (!save_window) {
        return;
    }

    // Returns the pressed button index
    const int button = GuiTextInputBox(popup_bounds(), "Save as Preset", "Enter Preset Name",
                                       "Ok;Cancel", preset_comment.data(), 255, nullptr);
    if (button == 1) {
        state.save_current_to_preset_file(preset_comment.data());
    }
    if (button == 0 || button == 1 || button == 2) {
        save_window = false;
        TextCopy(preset_comment.data(), "\0");
    }
}

auto user_interface::draw_puzzle_space_file_popup() -> void
{
    if (!puzzle_space_file_window) {
        return;
    }

    const std::string title = puzzle_space_file_save ? "Save Puzzle Space" : "Load Puzzle Space";
    const std::string action = puzzle_space_file_save ? "Saved" : "Loaded";
    const int button = GuiTextInputBox(popup_bounds(), title.c_str(), "Enter filename", "Ok;Cancel",
                                       puzzle_space_filename.data(), 255, nullptr);
    if (button == 1) {
        const std::string path = puzzle_space_filename.data();
        const std::optional<std::string> error = puzzle_space_file_save
            ? state.save_puzzle_space_file(path)
            : state.load_puzzle_space_file(path);
        if (error) {
            message_title =
                puzzle_space_file_save ? "Can't Save Puzzle Space" : "Can't Load Puzzle Space";
            message_message = *error;
        } else {
            const puzzle_space_result* results = state.get_puzzle_space_results();
            const size_t count = results != nullptr ? results->clusters.size() : 0;
            message_title = puzzle_space_file_save ? "Puzzle Space Saved" : "Puzzle Space Loaded";
            message_message =
                std::format("{} {} clusters from \"{}\".", action, format_count(count), path);
        }
        ok_message = true;
    }
    if (button == 0 || button == 1 || button == 2) {
        puzzle_space_file_window = false;
    }
}

auto user_interface::draw_ok_message_box() -> void
{
    if (!ok_message) {
        return;
    }

    const int button =
        GuiMessageBox(popup_bounds(), message_title.data(), message_message.data(), "Ok");
    if (button == 0 || button == 1) {
        message_title = "";
        message_message = "";
        ok_message = false;
    }
}

auto user_interface::draw_yes_no_message_box() -> void
{
    if (!yes_no_message) {
        return;
    }

    const std::string buttons = std::format("{};{}", yes_no_yes_label, yes_no_no_label);
    const int button =
        GuiMessageBox(popup_bounds(), message_title.data(), message_message.data(), buttons.data());
    if (button == 1 && yes_no_handler) {
        yes_no_handler();
    } else if ((button == 0 || button == 2) && yes_no_handler_no) {
        yes_no_handler_no();
    }
    if (button == 0 || button == 1 || button == 2) {
        message_title = "";
        message_message = "";
        yes_no_handler = nullptr;
        yes_no_handler_no = nullptr;
        yes_no_yes_label = "Yes";
        yes_no_no_label = "No";
        yes_no_message = false;
    }
}

auto user_interface::draw_main_menu() -> void
{
    active_input_bounds.reset();
    menu_grid.update_bounds(0, 0, GetScreenWidth(), COMPACT_MENU_HEIGHT, MENU_COLS,
                            COMPACT_MENU_ROWS);
    active_grid = &menu_grid;

    draw_menu_header(GRAY);

    // Second compact row: settings overlay and save.
    if (draw_menu_button(0, 1, 1, 1, input.settings_open ? "Hide Settings (,)" : "Settings (,)",
                         ORANGE)) {
        input.toggle_settings();
    }
    if (draw_menu_button(1, 1, 2, 1, "Save as Preset", GRAY)) {
        if (const std::optional<std::string>& reason = state.validate_current()) {
            message_title = "Can't Save Preset";
            message_message = std::format("Invalid Board: {}.", *reason);
            ok_message = true;
        } else {
            save_window = true;
        }
    }
}

auto user_interface::draw_controls_pane() -> void
{
    if (input.settings_open) {
        return;
    }

    const Rectangle bounds = controls_pane_bounds();
    const int panel_x = static_cast<int>(bounds.x);
    const int panel_y = static_cast<int>(bounds.y);
    const int panel_width = static_cast<int>(bounds.width);
    const int panel_height = static_cast<int>(bounds.height);

    // Always visible: part of the layout rather than a floating overlay. The font scales with the
    // pane so the two-column control grid keeps roughly the same number of characters per button.
    controls_font_size = pane_font_size();

    DrawRectangleRec(bounds, get_background_color());
    DrawRectangleLinesEx(bounds, 1.0f, BLACK);

    const int title_y = panel_y + 6;
    draw_label(Rectangle{static_cast<float>(panel_x + MENU_PAD), static_cast<float>(title_y),
                         static_cast<float>(panel_width - 2 * MENU_PAD), 30.0f},
               "Controls", BLACK, true, std::min(30, controls_font_size + 8));

    const int content_left = panel_x + MENU_PAD;
    const int content_top = panel_y + 40;
    const int content_bottom = panel_y + panel_height - 28;
    const int content_width = panel_width - 2 * MENU_PAD;
    const int content_height = std::max(1, content_bottom - content_top);

    constexpr int max_rows = 64;
    grid controls_grid(0, 0, 0, 0, 2, max_rows, MENU_PAD);
    controls_grid.update_bounds(content_left, content_top - static_cast<int>(controls_scroll),
                                content_width, max_rows * MENU_ROW_HEIGHT, 2, max_rows);
    active_grid = &controls_grid;

    int row = 0;
    const auto section = [&](const std::string& title, const Color section_color)
    {
        draw_label(active_grid->bounds(0, row, 2, 1), title, section_color, true,
                   controls_font_size);
        ++row;
    };

    active_input_bounds =
        Rectangle{static_cast<float>(content_left), static_cast<float>(content_top),
                  static_cast<float>(content_width), static_cast<float>(content_height)};
    BeginScissorMode(content_left, content_top, content_width, content_height);

    section("Puzzle", BLUE);
    row = state.presenter().draw_puzzle_controls(*this, row, BLUE, state, input);

    if (state.presenter().supports_editing() && state.presenter().editing()) {
        section("Edit", PURPLE);
        row = state.presenter().draw_edit_controls(*this, row, PURPLE, state);
    }

    section("Search", SEARCH_COLOR);
    const bool can_search_space = state.presenter().supports_puzzle_space();
    const int previous_mode = state.get_search_mode() == search_mode::puzzle_space ? 1 : 0;
    int puzzle_space_mode = previous_mode;
    draw_menu_toggle_slider(0, row, 2, 1, "State Space", "Puzzle Space", &puzzle_space_mode,
                            SEARCH_COLOR, can_search_space, controls_font_size);
    if (puzzle_space_mode != previous_mode) {
        state.set_search_mode(puzzle_space_mode != 0 ? search_mode::puzzle_space
                                                     : search_mode::state_space);
    }
    ++row;

    if (state.get_search_mode() == search_mode::puzzle_space && can_search_space) {
        section("Puzzle Space", SEARCH_COLOR);
        row = state.presenter().draw_puzzle_space_controls(*this, row, SEARCH_COLOR, state, input);
    } else {
        if (state.get_search_mode() == search_mode::puzzle_space && !can_search_space) {
            state.set_search_mode(search_mode::state_space);
        }
        section("Graph", ORANGE);
        row = draw_graph_info(ORANGE, row);
        row = draw_graph_controls(RED, row);
    }

    section("Camera", DARKGREEN);
    row = draw_camera_controls(DARKGREEN, row);

    EndScissorMode();
    active_input_bounds.reset();

    active_grid = &menu_grid;

    const int total_content_height = row * MENU_ROW_HEIGHT;
    const int max_scroll = std::max(0, total_content_height - content_height);
    if (CheckCollisionPointRec(
            GetMousePosition(),
            Rectangle{static_cast<float>(content_left), static_cast<float>(content_top),
                      static_cast<float>(content_width), static_cast<float>(content_height)})) {
        controls_scroll -= GetMouseWheelMove() * 48.0f;
    }
    controls_scroll = Clamp(controls_scroll, 0.0f, static_cast<float>(max_scroll));

    draw_label(Rectangle{static_cast<float>(panel_x + MENU_PAD),
                         static_cast<float>(panel_y + panel_height - 24),
                         static_cast<float>(panel_width - 2 * MENU_PAD), 20.0f},
               state.describe_current(), GRAY, true, OVERLAY_FONT_SIZE);
}

auto user_interface::add_performance_sample(const double physics_ms, const double render_ms) -> void
{
    performance_samples[performance_head] = {GetTime(), static_cast<float>(physics_ms),
                                             static_cast<float>(render_ms)};
    performance_head = (performance_head + 1) % performance_capacity;
    performance_count = std::min(performance_count + 1, performance_capacity);
}

auto user_interface::draw_performance_graph(const int graph_x, const int graph_y, const int graph_w,
                                            const int graph_h) const -> void
{
    draw_overlay_panel(Rectangle{static_cast<float>(graph_x), static_cast<float>(graph_y),
                                 static_cast<float>(graph_w), static_cast<float>(graph_h)});

    const int axis_left = 44;
    const int axis_right = 10;
    const int axis_top = 28;
    const int axis_bottom = 20;
    const int plot_x = graph_x + axis_left;
    const int plot_y = graph_y + axis_top;
    const int plot_w = graph_w - axis_left - axis_right;
    const int plot_h = graph_h - axis_top - axis_bottom;

    const double now = GetTime();
    const double window_start = now - 10.0;
    const Color physics_color = RED;
    const Color render_color = BLUE;

    float max_ms = 16.7f; // baseline: one 60 Hz frame
    size_t visible_count = 0;
    std::vector<float> visible_values;
    visible_values.reserve(performance_count * 2);
    for (size_t i = 0; i < performance_count; ++i) {
        const size_t index = (performance_head + performance_capacity - performance_count + i) %
            performance_capacity;
        const performance_sample& sample = performance_samples[index];
        if (sample.time < window_start) {
            continue;
        }
        ++visible_count;
        visible_values.push_back(sample.physics_ms);
        visible_values.push_back(sample.render_ms);
    }
    // Scale to the 95th percentile so occasional hitches don't flatten the graph;
    // values above the top are clamped to the top edge.
    if (!visible_values.empty()) {
        const size_t percentile_index =
            std::min(visible_values.size() - 1, static_cast<size_t>(visible_values.size() * 0.95));
        std::nth_element(visible_values.begin(), visible_values.begin() + percentile_index,
                         visible_values.end());
        max_ms = std::max(max_ms, visible_values[percentile_index] * 1.1f);
    }
    max_ms = std::ceil(max_ms / 5.0f) * 5.0f;

    for (int i = 0; i <= 4; ++i) {
        const int y = plot_y + plot_h - (plot_h * i) / 4;
        DrawLine(plot_x, y, plot_x + plot_w, y, Fade(BLACK, 0.14f));
        draw_overlay_text(TextFormat("%.1f", max_ms * static_cast<float>(i) / 4.0f), graph_x + 4,
                          y - 7, DARKGRAY);
    }
    draw_overlay_text("ms", graph_x + 4, graph_y + 4, DARKGRAY);

    for (int i = 0; i <= 2; ++i) {
        const int x = plot_x + (plot_w * i) / 2;
        DrawLine(x, plot_y, x, plot_y + plot_h, Fade(BLACK, 0.10f));
        if (i == 2) {
            const float spacing = static_cast<float>(GuiGetStyle(DEFAULT, TEXT_SPACING));
            const int label_width =
                static_cast<int>(MeasureTextEx(GuiGetFont(), "now", OVERLAY_FONT_SIZE, spacing).x);
            draw_overlay_text("now", x - label_width, plot_y + plot_h + 4, DARKGRAY);
        } else {
            draw_overlay_text(TextFormat("-%ds", (2 - i) * 5), x + (i == 0 ? 2 : -8),
                              plot_y + plot_h + 4, DARKGRAY);
        }
    }

    int legend_x = plot_x;
    const int legend_step = std::max(90, plot_w / 2);
    const auto legend = [&](const char* name, const Color color, const float value)
    {
        DrawLineEx(Vector2(static_cast<float>(legend_x), static_cast<float>(graph_y + 14)),
                   Vector2(static_cast<float>(legend_x + 18), static_cast<float>(graph_y + 14)),
                   2.0f, color);
        draw_overlay_text(TextFormat("%s %.1fms", name, value), legend_x + 22, graph_y + 6, color);
        legend_x += legend_step;
    };
    const performance_sample* latest = performance_count > 0
        ? &performance_samples[(performance_head + performance_capacity - 1) % performance_capacity]
        : nullptr;
    legend("Phys", physics_color, latest ? latest->physics_ms : 0.0f);
    legend("Rend", render_color, latest ? latest->render_ms : 0.0f);

    if (visible_count >= 2 && plot_w > 4 && plot_h > 4) {
        const size_t stride = std::max<size_t>(1, visible_count / static_cast<size_t>(plot_w));
        std::array<std::vector<Vector2>, 2> lines;
        for (auto& line : lines) {
            line.reserve(visible_count / stride + 2);
        }
        const std::array<Color, 2> line_colors = {physics_color, render_color};

        size_t visible_index = 0;
        for (size_t i = 0; i < performance_count; ++i) {
            const size_t index = (performance_head + performance_capacity - performance_count + i) %
                performance_capacity;
            const performance_sample& sample = performance_samples[index];
            if (sample.time < window_start) {
                continue;
            }

            const bool last = visible_index + 1 == visible_count;
            if (visible_index % stride == 0 || last) {
                const float x = static_cast<float>(plot_x) +
                    static_cast<float>((sample.time - window_start) / 10.0) * plot_w;
                const std::array<float, 2> values = {sample.physics_ms, sample.render_ms};
                for (size_t series = 0; series < values.size(); ++series) {
                    const float y = static_cast<float>(plot_y + plot_h) -
                        std::min(values[series], max_ms) / max_ms * plot_h;
                    lines[series].push_back(Vector2(x, y));
                }
            }
            ++visible_index;
        }

        for (size_t series = 0; series < lines.size(); ++series) {
            if (lines[series].size() >= 2) {
                DrawLineStrip(lines[series].data(), static_cast<int>(lines[series].size()),
                              line_colors[series]);
            }
        }
    }
}

auto user_interface::draw_settings_pane() -> void
{
    if (!input.settings_open) {
        return;
    }

    // The Settings pane replaces the Controls pane in the left column.
    const Rectangle bounds = controls_pane_bounds();
    const int panel_x = static_cast<int>(bounds.x);
    const int panel_y = static_cast<int>(bounds.y);
    const int panel_width = static_cast<int>(bounds.width);
    const int panel_height = static_cast<int>(bounds.height);
    const int font_size = pane_font_size();

    DrawRectangleRec(bounds, get_background_color());
    DrawRectangleLinesEx(bounds, 1.0f, BLACK);

    const int title_y = panel_y + 6;
    draw_label(Rectangle{static_cast<float>(panel_x + MENU_PAD), static_cast<float>(title_y),
                         static_cast<float>(panel_width - 2 * MENU_PAD), 30.0f},
               "Settings", BLACK, true, std::min(30, font_size + 8));

    simulation_settings cfg = get_settings();

    // Section tabs: three columns, two rows.
    const std::array<std::pair<settings_section, const char*>, 6> sections = {{
        {settings_section::barnes_hut, "Barnes-Hut"},
        {settings_section::physics, "Physics"},
        {settings_section::links, "Links"},
        {settings_section::camera, "Camera"},
        {settings_section::rendering, "Rendering"},
        {settings_section::performance, "Performance"},
    }};

    constexpr int tab_gap = 3;
    constexpr int tab_height = 28;
    const int tab_left = panel_x + MENU_PAD;
    const int tab_top = panel_y + 36;
    const int tab_width = (panel_width - 2 * MENU_PAD - 2 * tab_gap) / 3;
    for (size_t i = 0; i < sections.size(); ++i) {
        const auto& [section, label] = sections[i];
        const int column = static_cast<int>(i) % 3;
        const int line = static_cast<int>(i) / 3;
        const bool selected = settings_tab == section;
        if (draw_button(Rectangle{static_cast<float>(tab_left + column * (tab_width + tab_gap)),
                                  static_cast<float>(tab_top + line * (tab_height + tab_gap)),
                                  static_cast<float>(tab_width), static_cast<float>(tab_height)},
                        label, selected ? ORANGE : GRAY, true, font_size)) {
            settings_tab = section;
            settings_scroll = 0.0f;
        }
    }

    constexpr int footer_gap = 4;
    constexpr int footer_height = 28;
    const int footer_y = panel_y + panel_height - footer_height - MENU_PAD;
    const int footer_width = (panel_width - 2 * MENU_PAD - footer_gap) / 2;
    if (draw_button(Rectangle{static_cast<float>(tab_left), static_cast<float>(footer_y),
                              static_cast<float>(footer_width), static_cast<float>(footer_height)},
                    "Reset All", RED, true, font_size)) {
        cfg = default_settings();
        camera.fov = cfg.fov;
        camera.distance = cfg.camera_distance;
    }
    if (draw_button(Rectangle{static_cast<float>(tab_left + footer_width + footer_gap),
                              static_cast<float>(footer_y), static_cast<float>(footer_width),
                              static_cast<float>(footer_height)},
                    "Close", GRAY, true, font_size)) {
        input.settings_open = false;
        save_settings();
    }

    // Scrollable parameter pane between the section tabs and the footer.
    const int pane_x = panel_x + MENU_PAD;
    const int pane_width = panel_width - 2 * MENU_PAD;
    const int pane_top = tab_top + 2 * (tab_height + tab_gap) + 4;
    const int pane_bottom = footer_y - footer_gap;
    const int settings_pane_height = std::max(1, pane_bottom - pane_top);
    const int info_font_size = std::max(12, font_size - 2);

    active_input_bounds =
        Rectangle{static_cast<float>(pane_x), static_cast<float>(pane_top),
                  static_cast<float>(pane_width), static_cast<float>(settings_pane_height)};
    BeginScissorMode(pane_x, pane_top, pane_width, settings_pane_height);
    float row_y = static_cast<float>(pane_top) - settings_scroll;

    const auto float_row = [&](const char* label, float& value, float min, float max)
    {
        draw_label(
            Rectangle{static_cast<float>(pane_x), row_y, static_cast<float>(pane_width), 20.0f},
            std::format("{}: {:.3f}", label, value), BLACK, true, font_size);
        const float previous = value;
        GuiSlider(Rectangle{static_cast<float>(pane_x), row_y + 20.0f,
                            static_cast<float>(pane_width), 22.0f},
                  "", "", &value, min, max);
        if (!pointer_in_active_bounds()) {
            value = previous;
        }
        row_y += 50.0f;
    };
    const auto int_row = [&](const char* label, int& value, int min, int max)
    {
        draw_label(
            Rectangle{static_cast<float>(pane_x), row_y, static_cast<float>(pane_width), 20.0f},
            label, BLACK, true, font_size);
        const int previous = value;
        GuiSpinner(
            Rectangle{static_cast<float>(pane_x + pane_width - 110), row_y + 20.0f, 110.0f, 22.0f},
            "", "", &value, min, max, false);
        if (!pointer_in_active_bounds()) {
            value = previous;
        }
        row_y += 48.0f;
    };
    const auto bool_row = [&](const char* label, bool& value, bool enabled = true)
    {
        const int gui_state = GuiGetState();
        if (!enabled) {
            GuiSetState(STATE_DISABLED);
        }
        const bool previous = value;
        GuiCheckBox(Rectangle{static_cast<float>(pane_x), row_y + 2.0f, 22.0f, 22.0f}, "", &value);
        GuiSetState(gui_state);
        if (!pointer_in_active_bounds()) {
            value = previous;
        }
        draw_label(Rectangle{static_cast<float>(pane_x + 30), row_y,
                             static_cast<float>(pane_width - 30), 26.0f},
                   label, enabled ? BLACK : GRAY, true, font_size);
        row_y += 28.0f;
    };
    const auto info_row = [&](const std::string& text)
    {
        draw_label(
            Rectangle{static_cast<float>(pane_x), row_y, static_cast<float>(pane_width), 24.0f},
            text, DARKGRAY, true, info_font_size);
        row_y += 26.0f;
    };

    switch (settings_tab) {
    case settings_section::barnes_hut:
        float_row("Theta", cfg.theta, 0.4f, 2.5f);
        float_row("Repulsion strength", cfg.bh_force, 0.0f, 10.0f);
        float_row("Softening", cfg.softening, 0.001f, 1.0f);
        int_row("Tree depth", cfg.max_depth, 6, 12);
        int_row("Rebuild interval", cfg.rebuild_interval, 1, 4);
        bool_row("Async octree build", cfg.async_octree);
        break;
    case settings_section::physics:
        float_row("Simulation speed", cfg.sim_speed, 0.25f, 8.0f);
        float_row("Target UPS", cfg.target_ups, 30.0f, 240.0f);
        float_row("Verlet damping", cfg.verlet_dampening, 0.0f, 0.5f);
        float_row("Mass", cfg.mass, 0.1f, 10.0f);
        float_row("Max step", cfg.max_step, 0.1f, 5.0f);
        break;
    case settings_section::links:
        float_row("Spring K", cfg.spring_k, 0.0f, 5.0f);
        float_row("Spring damping", cfg.dampening_k, 0.0f, 1.5f);
        float_row("Minimum length", cfg.rest_length, 0.5f, 15.0f);
        int_row("Projection iterations", cfg.constraint_iterations, 1, 4);
        float_row("Projection relaxation", cfg.constraint_relaxation, 0.1f, 1.0f);
        int_row("Projection interval", cfg.constraint_interval, 1, 4);
        float_row("Initial spread (next populate)", cfg.spawn_spread, 0.0f, 1.0f);
        break;
    case settings_section::camera:
        {
            const float old_fov = cfg.fov;
            const float old_distance = cfg.camera_distance;
            float_row("Field of view", cfg.fov, 10.0f, 540.0f);
            float_row("Distance", cfg.camera_distance, 2.0f, 2000.0f);
            float_row("Smoothing speed", cfg.camera_smooth_speed, 1.0f, 60.0f);
            float_row("Zoom speed", cfg.zoom_speed, 0.1f, 10.0f);
            float_row("FOV speed", cfg.fov_speed, 0.1f, 10.0f);
            float_row("Pan speed", cfg.pan_speed, 0.1f, 10.0f);
            float_row("Rotate speed", cfg.rot_speed, 0.1f, 10.0f);
            if (std::fabs(cfg.fov - old_fov) > 1e-5f) {
                camera.fov = cfg.fov;
            }
            if (std::fabs(cfg.camera_distance - old_distance) > 1e-5f) {
                camera.distance = cfg.camera_distance;
            }
            break;
        }
    case settings_section::rendering:
        float_row("Node size", cfg.vertex_size, 0.0f, 3.0f);
        float_row("Solution node size", cfg.solution_node_size, 0.0f, 6.0f);
        float_row("Visited node size", cfg.visited_node_size, 0.0f, 6.0f);
        bool_row("Draw nodes", cfg.draw_nodes);
        bool_row("Draw edges", cfg.draw_edges);
        int_row("Edge stride", cfg.edge_stride, 1, 16);
        bool_row("Billboard nodes", cfg.billboard_nodes);
        bool_row("Show view labels", cfg.show_labels);
        bool_row("Show physics debug", cfg.show_debug);
        bool_row("Show performance graph", cfg.show_performance_overlay);
        bool_row("Show RAM overlay", cfg.show_memory_overlay);
        break;
    case settings_section::performance:
        {
            int_row("Log level (0/1/2)", cfg.log_level, 0, 2);
            bool_row("GPU physics (OpenGL 4.3)", cfg.gpu_physics, gpu::available());
            bool_row("GPU octree (experimental)", cfg.gpu_tree_sort, gpu::available());
            info_row(std::format("Backend: {}", gpu::active() ? "GPU" : "CPU"));
            if (!gpu::available()) {
                info_row(std::format("GPU unavailable: {}", gpu::runtime_status().error));
            }
            int_row("Worker threads (0 = auto)", cfg.worker_threads, 0, 64);
            info_row("Applies on restart");
            info_row(std::format("Current worker threads: {}", active_worker_threads()));
            int_row("Search threads (0 = auto)", cfg.search_threads, 0, 64);
            info_row("Applies to next search");
            info_row(std::format("Current search threads: {}",
                                 state.search_is_running()
                                     ? state.search_thread_count()
                                     : resolve_search_threads(cfg.search_threads)));
            int_row("Small task block", cfg.small_task_block_size, 1, 4096);
            int_row("Large task block", cfg.large_task_block_size, 1, 4096);
            break;
        }
    }

    EndScissorMode();
    active_input_bounds.reset();

    const int content_height = static_cast<int>(row_y + settings_scroll) - pane_top;
    const int max_scroll = std::max(0, content_height - settings_pane_height);
    if (CheckCollisionPointRec(GetMousePosition(),
                               Rectangle(static_cast<float>(pane_x), static_cast<float>(pane_top),
                                         static_cast<float>(pane_width),
                                         static_cast<float>(settings_pane_height)))) {
        settings_scroll = Clamp(settings_scroll - GetMouseWheelMove() * 48.0f, 0.0f,
                                static_cast<float>(max_scroll));
    } else {
        settings_scroll = Clamp(settings_scroll, 0.0f, static_cast<float>(max_scroll));
    }

    set_settings(cfg);
}

auto user_interface::draw_board() -> void
{
    puzzle_presenter& presenter = state.presenter();
    const state_id id = state.get_current_state_id();

    puzzle_canvas canvas;
    canvas.bounds = board_canvas_bounds();
    canvas.columns = presenter.board_columns(state.model(), id);
    canvas.rows = presenter.board_rows(state.model(), id);
    canvas.padding = BOARD_PADDING;

    presenter.draw_board(canvas, state);
}

auto user_interface::set_renderer_memory(const size_t cpu_bytes, const size_t gpu_bytes) -> void
{
    renderer_cpu_bytes = cpu_bytes;
    renderer_gpu_bytes = gpu_bytes;
}

auto user_interface::draw_overlay_panel(const Rectangle bounds) const -> void
{
    DrawRectangle(static_cast<int>(bounds.x) + 3, static_cast<int>(bounds.y) + 3,
                  static_cast<int>(bounds.width), static_cast<int>(bounds.height),
                  Fade(BLACK, 0.16f));
    DrawRectangle(static_cast<int>(bounds.x), static_cast<int>(bounds.y),
                  static_cast<int>(bounds.width), static_cast<int>(bounds.height),
                  Fade(get_background_color(), 0.9f));
    DrawRectangleLinesEx(bounds, 1.0f, BLACK);
}

auto user_interface::draw_overlay_text(const std::string& text, const int x, const int y,
                                       const Color color, const int font_size) const -> void
{
    DrawTextEx(GuiGetFont(), text.c_str(), Vector2{static_cast<float>(x), static_cast<float>(y)},
               static_cast<float>(font_size),
               static_cast<float>(GuiGetStyle(DEFAULT, TEXT_SPACING)), color);
}

auto user_interface::draw_performance_overlay() -> void
{
    const Rectangle bounds = performance_overlay_bounds();
    draw_performance_graph(static_cast<int>(bounds.x), static_cast<int>(bounds.y),
                           static_cast<int>(bounds.width), static_cast<int>(bounds.height));
}

auto user_interface::draw_memory_overlay() -> void
{
    memory_usage usage = state.memory_usage();
    usage.renderer_cpu = renderer_cpu_bytes;
    usage.renderer_gpu = renderer_gpu_bytes;

    const Rectangle bounds = memory_overlay_bounds();
    draw_overlay_panel(bounds);

    const int left = static_cast<int>(bounds.x) + 10;
    const int width = static_cast<int>(bounds.width) - 20;
    int y = static_cast<int>(bounds.y) + 8;
    const auto line = [&](const std::string& text, const Color color = BLACK)
    {
        draw_label(Rectangle{static_cast<float>(left), static_cast<float>(y),
                             static_cast<float>(width), static_cast<float>(OVERLAY_LINE_HEIGHT)},
                   text, color, true, OVERLAY_FONT_SIZE);
        y += OVERLAY_LINE_HEIGHT;
    };

    const memory_tracker::snapshot heap = memory_tracker::get_snapshot();
    line(std::format("Tracked RAM: {}", format_memory(usage.total())));
    line(std::format("C++ heap: {}", format_memory(heap.live_bytes)));
    line(std::format("Peak heap: {}", format_memory(heap.peak_bytes)));
    const search_snapshot search = state.get_search_snapshot();
    if (search.running() && search.kind == search_kind::puzzle_space) {
        line(std::format("Search (est.): {} (peak {})", format_memory(usage.search),
                         format_memory(search.estimated_peak_bytes)));
    } else {
        line(std::format("Search (est.): {}", format_memory(usage.search)));
    }
    line(std::format("Model: {}", format_memory(usage.model)));
    line(std::format("Graph: {}", format_memory(usage.graph)));
    line(std::format("Results: {}", format_memory(usage.results)));
    line(std::format("Physics CPU: {}", format_memory(usage.physics_cpu)));
    line(std::format("Octree: {}", format_memory(usage.physics_octree)));
    line(std::format("Readback: {}", format_memory(usage.physics_readback)));
    line(std::format("GPU physics: {}", format_memory(usage.gpu_physics)));
    line(std::format("GPU octree: {}", format_memory(usage.gpu_octree)));
    line(std::format("Renderer CPU: {}", format_memory(usage.renderer_cpu)));
    line(std::format("Renderer GPU: {}", format_memory(usage.renderer_gpu)));
    line("Own allocations only", DARKGRAY);
}

auto user_interface::draw_graph_overlay(int fps, int ups, size_t mass_count, size_t spring_count)
    -> void
{
    graph_overlay_grid.update_bounds(right_pane_x() + OVERLAY_MARGIN,
                                     COMPACT_MENU_HEIGHT + OVERLAY_MARGIN);
    debug_overlay_grid.update_bounds(right_pane_x() + OVERLAY_MARGIN,
                                     GetScreenHeight() - OVERLAY_MARGIN - 75);

    const simulation_settings cfg = get_settings();
    if (cfg.show_labels) {
        Rectangle panel = graph_overlay_grid.bounds(0, 0, 1, 4);
        panel.x -= MENU_PAD;
        panel.y -= MENU_PAD;
        panel.width += 2 * MENU_PAD;
        panel.height += 2 * MENU_PAD;
        draw_overlay_panel(panel);

        draw_label(graph_overlay_grid.bounds(0, 0, 1, 1),
                   std::format("Dist: {:0>7.2f}", camera.distance), BLACK, true, OVERLAY_FONT_SIZE);
        draw_label(graph_overlay_grid.bounds(0, 1, 1, 1),
                   std::format("FoV:  {:0>6.2f}", camera.fov), BLACK, true, OVERLAY_FONT_SIZE);
        draw_label(graph_overlay_grid.bounds(0, 2, 1, 1), std::format("FPS:  {:0>3}", fps), LIME,
                   true, OVERLAY_FONT_SIZE);
        draw_label(graph_overlay_grid.bounds(0, 3, 1, 1), std::format("UPS:  {:0>3}", ups), ORANGE,
                   true, OVERLAY_FONT_SIZE);
    }

    if (cfg.show_debug) {
        Rectangle panel = debug_overlay_grid.bounds(0, 0, 1, 3);
        panel.x -= MENU_PAD;
        panel.y -= MENU_PAD;
        panel.width += 2 * MENU_PAD;
        panel.height += 2 * MENU_PAD;
        draw_overlay_panel(panel);

        draw_label(debug_overlay_grid.bounds(0, 0, 1, 1), std::format("Physics Debug:"), BLACK,
                   true, OVERLAY_FONT_SIZE);
        draw_label(debug_overlay_grid.bounds(0, 1, 1, 1), std::format("Masses:  {}", mass_count),
                   BLACK, true, OVERLAY_FONT_SIZE);
        draw_label(debug_overlay_grid.bounds(0, 2, 1, 1), std::format("Springs: {}", spring_count),
                   BLACK, true, OVERLAY_FONT_SIZE);
    }
}

auto user_interface::draw(const int fps, const int ups, const size_t mass_count,
                          const size_t spring_count) -> void
{
    const auto visitor = overloads{[&](const show_ok_message& msg)
                                   {
                                       message_title = msg.title;
                                       message_message = msg.message;
                                       ok_message = true;
                                   },
                                   [&](const show_yes_no_message& msg)
                                   {
                                       message_title = msg.title;
                                       message_message = msg.message;
                                       yes_no_yes_label = "Yes";
                                       yes_no_no_label = "No";
                                       yes_no_handler = msg.on_yes;
                                       yes_no_handler_no = nullptr;
                                       yes_no_message = true;
                                   },
                                   [&](const show_save_preset_window& msg) { save_window = true; },
                                   [&](const show_puzzle_space_file_window& msg)
                                   {
                                       puzzle_space_file_window = true;
                                       puzzle_space_file_save = msg.save;
                                   },
                                   [&](const toggle_help_window&)
                                   {
                                       help_window = !help_window;
                                       if (help_window) {
                                           help_scroll = 0.0f;
                                       }
                                   }};

    while (!input.ui_commands.empty()) {
        const ui_command& cmd = input.ui_commands.front();

        cmd.visit(visitor);

        input.ui_commands.pop();
    }

    input.text_input_open = save_window || puzzle_space_file_window;
    input.disable = window_open();

    draw_graph_overlay(fps, ups, mass_count, spring_count);
    const simulation_settings overlay_settings = get_settings();
    if (overlay_settings.show_performance_overlay) {
        draw_performance_overlay();
    }
    if (overlay_settings.show_memory_overlay) {
        draw_memory_overlay();
    }
    draw_controls_pane();
    draw_settings_pane();
    help_popup();
    draw_save_preset_popup();
    draw_puzzle_space_file_popup();
    draw_ok_message_box();
    draw_yes_no_message_box();

    // Popups may have opened or closed during this draw. Reflect that before the next input pass.
    input.text_input_open = save_window || puzzle_space_file_window;
}
