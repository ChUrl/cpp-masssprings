#include "gui.hpp"
#include "config.hpp"

#include <raylib.h>

#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

#ifdef TRACY
#include "tracy.hpp"
#include <tracy/Tracy.hpp>
#endif

auto Grid::UpdateBounds(int _x, int _y, int _width, int _height, int _columns,
                        int _rows) -> void {
  x = _x;
  y = _y;
  width = _width;
  height = _height;
  columns = _columns;
  rows = _rows;
}

auto Grid::UpdateBounds(int _x, int _y, int _width, int _height) -> void {
  x = _x;
  y = _y;
  width = _width;
  height = _height;
}

auto Grid::UpdateBounds(int _x, int _y) -> void {
  x = _x;
  y = _y;
}

auto Grid::Bounds() const -> Rectangle {
  Rectangle bounds = Bounds(0, 0, columns, rows);
  bounds.x -= padding;
  bounds.y -= padding;
  bounds.width += 2 * padding;
  bounds.height += 2 * padding;
  return bounds;
}

auto Grid::Bounds(int _x, int _y, int _width, int _height) const -> Rectangle {
  if (_x < 0 || _x + _width > columns || _y < 0 || _y + _height > rows) {
    std::cout << std::format("Grid bounds are outside range.") << std::endl;
    exit(1);
  }

  int cell_width = (width - padding) / columns;
  int cell_height = (height - padding) / rows;

  return Rectangle(
      x + _x * cell_width + padding, y + _y * cell_height + padding,
      _width * cell_width - padding, _height * cell_height - padding);
}

auto Grid::SquareBounds() const -> Rectangle {
  Rectangle bounds = SquareBounds(0, 0, columns, rows);
  bounds.x -= padding;
  bounds.y -= padding;
  bounds.width += 2 * padding;
  bounds.height += 2 * padding;
  return bounds;
}

auto Grid::SquareBounds(int _x, int _y, int _width, int _height) const
    -> Rectangle {
  // Assumes each cell is square, so either width or height are not completely
  // filled

  if (_x < 0 || _x + _width > columns || _y < 0 || _y + _height > rows) {
    std::cout << std::format("Grid bounds are outside range.") << std::endl;
    exit(1);
  }

  int available_width = width - padding * (columns + 1);
  int available_height = height - padding * (rows + 1);
  int cell_size = std::min(available_width / columns, available_height / rows);

  int grid_width = cell_size * columns + padding * (columns + 1);
  int grid_height = cell_size * rows + padding * (rows + 1);
  int x_offset = (width - grid_width) / 2;
  int y_offset = (height - grid_height) / 2;

  return Rectangle(x_offset + _x * (cell_size + padding) + padding,
                   y_offset + _y * (cell_size + padding) + padding,
                   _width * cell_size + padding * (_width - 1),
                   _height * cell_size + padding * (_height - 1));
}

auto Gui::Init() const -> void {
  Font font = LoadFontEx(FONT, FONT_SIZE, 0, 0);
  SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
  GuiSetFont(font);

  DefaultStyle style = GetDefaultStyle();
  style.text_size = FONT_SIZE;
  ApplyColor(style, GRAY);

  SetDefaultStyle(style);
}

auto Gui::ApplyColor(Style &style, Color color) const -> void {
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

auto Gui::ApplyBlockColor(Style &style, Color color) const -> void {
  style.base_color_normal = ColorToInt(Fade(color, 0.5));
  style.base_color_focused = ColorToInt(Fade(color, 0.3));
  style.base_color_pressed = ColorToInt(Fade(color, 0.8));
  style.base_color_disabled = ColorToInt(Fade(color, 0.5));

  style.border_color_normal = ColorToInt(Fade(color, 1.0));
  style.border_color_focused = ColorToInt(Fade(color, 0.7));
  style.border_color_pressed = ColorToInt(Fade(color, 1.0));
  style.border_color_disabled = ColorToInt(Fade(GRAY, 0.5));
}

auto Gui::ApplyTextColor(Style &style, Color color) const -> void {
  style.text_color_normal = ColorToInt(Fade(color, 1.0));
  style.text_color_focused = ColorToInt(Fade(color, 1.0));
  style.text_color_pressed = ColorToInt(Fade(color, 1.0));
  style.text_color_disabled = ColorToInt(Fade(BLACK, 0.5));
}

auto Gui::GetDefaultStyle() const -> DefaultStyle {
  // Could've iterated over the values but then it wouldn't be as nice to
  // access...
  return {{GuiGetStyle(DEFAULT, BORDER_COLOR_NORMAL),
           GuiGetStyle(DEFAULT, BASE_COLOR_NORMAL),
           GuiGetStyle(DEFAULT, TEXT_COLOR_NORMAL),

           GuiGetStyle(DEFAULT, BORDER_COLOR_FOCUSED),
           GuiGetStyle(DEFAULT, BASE_COLOR_FOCUSED),
           GuiGetStyle(DEFAULT, TEXT_COLOR_FOCUSED),

           GuiGetStyle(DEFAULT, BORDER_COLOR_PRESSED),
           GuiGetStyle(DEFAULT, BASE_COLOR_PRESSED),
           GuiGetStyle(DEFAULT, TEXT_COLOR_PRESSED),

           GuiGetStyle(DEFAULT, BORDER_COLOR_DISABLED),
           GuiGetStyle(DEFAULT, BASE_COLOR_DISABLED),
           GuiGetStyle(DEFAULT, TEXT_COLOR_DISABLED)},

          GuiGetStyle(DEFAULT, BACKGROUND_COLOR),
          GuiGetStyle(DEFAULT, LINE_COLOR),

          GuiGetStyle(DEFAULT, TEXT_SIZE),
          GuiGetStyle(DEFAULT, TEXT_SPACING),
          GuiGetStyle(DEFAULT, TEXT_LINE_SPACING),
          GuiGetStyle(DEFAULT, TEXT_ALIGNMENT_VERTICAL),
          GuiGetStyle(DEFAULT, TEXT_WRAP_MODE)};
}

auto Gui::SetDefaultStyle(const DefaultStyle &style) const -> void {
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

auto Gui::GetComponentStyle(int component) const -> ComponentStyle {
  return {{GuiGetStyle(component, BORDER_COLOR_NORMAL),
           GuiGetStyle(component, BASE_COLOR_NORMAL),
           GuiGetStyle(component, TEXT_COLOR_NORMAL),

           GuiGetStyle(component, BORDER_COLOR_FOCUSED),
           GuiGetStyle(component, BASE_COLOR_FOCUSED),
           GuiGetStyle(component, TEXT_COLOR_FOCUSED),

           GuiGetStyle(component, BORDER_COLOR_PRESSED),
           GuiGetStyle(component, BASE_COLOR_PRESSED),
           GuiGetStyle(component, TEXT_COLOR_PRESSED),

           GuiGetStyle(component, BORDER_COLOR_DISABLED),
           GuiGetStyle(component, BASE_COLOR_DISABLED),
           GuiGetStyle(component, TEXT_COLOR_DISABLED)},

          GuiGetStyle(component, BORDER_WIDTH),
          GuiGetStyle(component, TEXT_PADDING),
          GuiGetStyle(component, TEXT_ALIGNMENT)};
}

auto Gui::SetComponentStyle(int component, const ComponentStyle &style) const
    -> void {
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

auto Gui::DrawButton(Rectangle bounds, const std::string &label, Color color,
                     bool enabled, int font_size) const -> int {
  // Save original styling
  const DefaultStyle original_default = GetDefaultStyle();
  const ComponentStyle original_button = GetComponentStyle(BUTTON);

  // Change styling
  DefaultStyle style_default = original_default;
  ComponentStyle style_button = original_button;
  style_default.text_size = font_size;
  ApplyColor(style_button, color);
  SetDefaultStyle(style_default);
  SetComponentStyle(BUTTON, style_button);

  const int _state = GuiGetState();
  if (!enabled || WindowOpen()) {
    GuiSetState(STATE_DISABLED);
  }
  int pressed = GuiButton(bounds, label.data());
  if (!enabled || WindowOpen()) {
    GuiSetState(_state);
  }

  // Restore original styling
  SetDefaultStyle(original_default);
  SetComponentStyle(BUTTON, original_button);

  return pressed;
}

auto Gui::DrawMenuButton(int x, int y, int width, int height,
                         const std::string &label, Color color, bool enabled,
                         int font_size) const -> int {
  Rectangle bounds = menu_grid.Bounds(x, y, width, height);
  return DrawButton(bounds, label, color, enabled, font_size);
}

auto Gui::DrawToggleSlider(Rectangle bounds, const std::string &off_label,
                           const std::string &on_label, int *active,
                           Color color, bool enabled, int font_size) const
    -> int {
  // Save original styling
  const DefaultStyle original_default = GetDefaultStyle();
  const ComponentStyle original_slider = GetComponentStyle(SLIDER);
  const ComponentStyle original_toggle = GetComponentStyle(TOGGLE);

  // Change styling
  DefaultStyle style_default = original_default;
  ComponentStyle style_slider = original_slider;
  ComponentStyle style_toggle = original_toggle;
  style_default.text_size = font_size;
  ApplyColor(style_slider, color);
  ApplyColor(style_toggle, color);
  SetDefaultStyle(style_default);
  SetComponentStyle(SLIDER, style_slider);
  SetComponentStyle(TOGGLE, style_toggle);

  const int _state = GuiGetState();
  if (!enabled || WindowOpen()) {
    GuiSetState(STATE_DISABLED);
  }
  int pressed = GuiToggleSlider(
      bounds, std::format("{};{}", off_label, on_label).data(), active);
  if (!enabled || WindowOpen()) {
    GuiSetState(_state);
  }

  // Restore original styling
  SetDefaultStyle(original_default);
  SetComponentStyle(SLIDER, original_slider);
  SetComponentStyle(TOGGLE, original_toggle);

  return pressed;
}

auto Gui::DrawMenuToggleSlider(int x, int y, int width, int height,
                               const std::string &off_label,
                               const std::string &on_label, int *active,
                               Color color, bool enabled, int font_size) const
    -> int {
  Rectangle bounds = menu_grid.Bounds(x, y, width, height);
  return DrawToggleSlider(bounds, off_label, on_label, active, color, enabled,
                          font_size);
};

auto Gui::DrawSpinner(Rectangle bounds, const std::string &label, int *value,
                      int min, int max, Color color, bool enabled,
                      int font_size) const -> int {
  // Save original styling
  const DefaultStyle original_default = GetDefaultStyle();
  const ComponentStyle original_valuebox = GetComponentStyle(VALUEBOX);
  const ComponentStyle original_button = GetComponentStyle(BUTTON);

  // Change styling
  DefaultStyle style_default = original_default;
  ComponentStyle style_valuebox = original_valuebox;
  ComponentStyle style_button = original_button;
  style_default.text_size = font_size;
  ApplyColor(style_valuebox, color);
  ApplyColor(style_button, color);
  SetDefaultStyle(style_default);
  SetComponentStyle(VALUEBOX, style_valuebox);
  SetComponentStyle(BUTTON, style_button);

  const int _state = GuiGetState();
  if (!enabled || WindowOpen()) {
    GuiSetState(STATE_DISABLED);
  }
  int pressed = GuiSpinner(bounds, "", label.data(), value, min, max, false);
  if (!enabled || WindowOpen()) {
    GuiSetState(_state);
  }

  // Restore original styling
  SetDefaultStyle(original_default);
  SetComponentStyle(VALUEBOX, original_valuebox);
  SetComponentStyle(BUTTON, style_button);

  return pressed;
}

auto Gui::DrawMenuSpinner(int x, int y, int width, int height,
                          const std::string &label, int *value, int min,
                          int max, Color color, bool enabled,
                          int font_size) const -> int {
  Rectangle bounds = menu_grid.Bounds(x, y, width, height);
  return DrawSpinner(bounds, label, value, min, max, color, enabled, font_size);
}

auto Gui::DrawLabel(Rectangle bounds, const std::string &text, Color color,
                    bool enabled, int font_size) const -> int {
  // Save original styling
  const DefaultStyle original_default = GetDefaultStyle();
  const ComponentStyle original_label = GetComponentStyle(LABEL);

  // Change styling
  DefaultStyle style_default = original_default;
  ComponentStyle style_label = original_label;
  style_default.text_size = font_size;
  ApplyTextColor(style_label, color);
  SetDefaultStyle(style_default);
  SetComponentStyle(LABEL, style_label);

  const int _state = GuiGetState();
  if (!enabled || WindowOpen()) {
    GuiSetState(STATE_DISABLED);
  }
  int pressed = GuiLabel(bounds, text.data());
  if (!enabled || WindowOpen()) {
    GuiSetState(_state);
  }

  // Restore original styling
  SetDefaultStyle(original_default);
  SetComponentStyle(LABEL, original_label);

  return pressed;
}

auto Gui::DrawBoardBlock(int x, int y, int width, int height, Color color,
                         bool enabled) const -> bool {
  ComponentStyle style = GetComponentStyle(BUTTON);
  ApplyBlockColor(style, color);

  Rectangle bounds = board_grid.SquareBounds(x, y, width, height);

  bool focused =
      CheckCollisionPointRec(input.mouse - Vector2(0, MENU_HEIGHT), bounds);
  bool pressed =
      Block(x, y, width, height, false).Covers(input.sel_x, input.sel_y);

  // Background to make faded colors work
  DrawRectangleRec(bounds, RAYWHITE);

  Color base = GetColor(style.base_color_normal);
  Color border = GetColor(style.base_color_normal);
  if (pressed) {
    base = GetColor(style.base_color_pressed);
    border = GetColor(style.base_color_pressed);
  }
  if (focused) {
    base = GetColor(style.base_color_focused);
    border = GetColor(style.base_color_focused);
  }
  if (focused && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    base = GetColor(style.base_color_pressed);
    border = GetColor(style.base_color_pressed);
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

auto Gui::WindowOpen() const -> bool { return save_window || help_window; }

auto Gui::DrawMenuHeader(Color color) const -> void {
  int preset = state.current_preset;
  DrawMenuSpinner(0, 0, 1, 1, "Preset: ", &preset, -1, state.presets.size(),
                  color, !input.editing);
  if (preset > state.current_preset) {
    input.NextPreset();
  } else if (preset < state.current_preset) {
    input.PreviousPreset();
  }

  DrawMenuButton(1, 0, 1, 1,
                 std::format("Puzzle: \"{}\"",
                             state.comments.at(state.current_preset).substr(2)),
                 color);

  int editing = input.editing;
  DrawMenuToggleSlider(2, 0, 1, 1, "Puzzle Mode (Tab)", "Edit Mode (Tab)",
                       &editing, color);
  if (editing != input.editing) {
    input.ToggleEditing();
  }
}

auto Gui::DrawGraphInfo(Color color) const -> void {
  DrawMenuButton(0, 1, 1, 1,
                 std::format("Found {} States ({} Winning)",
                             state.states.size(), state.winning_states.size()),
                 color);

  DrawMenuButton(1, 1, 1, 1,
                 std::format("Found {} Transitions", state.springs.size()),
                 color);

  DrawMenuButton(2, 1, 1, 1,
                 std::format("{} Moves to Nearest Solution",
                             state.winning_path.size() > 0
                                 ? state.winning_path.size() - 1
                                 : 0),
                 color);
}

auto Gui::DrawGraphControls(Color color) const -> void {
  if (DrawMenuButton(0, 2, 1, 1, "Populate Graph (G)", color)) {
    input.FillGraph();
  }

  // int mark_path = input.mark_path;
  // DrawMenuToggleSlider(2, 2, 1, 1, "Path Hidden (U)", "Path Shown (U)",
  //                      &mark_path, color);
  // if (mark_path != input.mark_path) {
  //   input.ToggleMarkPath();
  // }

  if (DrawMenuButton(1, 2, 1, 1, "Clear Graph (C)", color)) {
    input.ClearGraph();
  }

  int mark_solutions = input.mark_solutions;
  DrawMenuToggleSlider(2, 2, 1, 1, "Solution Hidden (I)", "Solution Shown (I)",
                       &mark_solutions, color);
  if (mark_solutions != input.mark_solutions) {
    input.ToggleMarkSolutions();
  }
  input.mark_path = input.mark_solutions;
}

auto Gui::DrawCameraControls(Color color) const -> void {
  int lock_camera = input.camera_lock;
  DrawMenuToggleSlider(0, 3, 1, 1, "Free Camera (L)", "Locked Camera (L)",
                       &lock_camera, color);
  if (lock_camera != input.camera_lock) {
    input.ToggleCameraLock();
  }

  int lock_camera_mass_center = input.camera_mass_center_lock;
  DrawMenuToggleSlider(1, 3, 1, 1, "Current Block (Y)", "Graph Center (Y)",
                       &lock_camera_mass_center, color, input.camera_lock);
  if (lock_camera_mass_center != input.camera_mass_center_lock) {
    input.ToggleCameraMassCenterLock();
  }

  int projection = camera.projection == CAMERA_ORTHOGRAPHIC;
  DrawMenuToggleSlider(2, 3, 1, 1, "Perspective (Alt)", "Orthographic (Alt)",
                       &projection, color);
  if (projection != (camera.projection == CAMERA_ORTHOGRAPHIC)) {
    input.ToggleCameraProjection();
  }
}

auto Gui::DrawPuzzleControls(Color color) const -> void {
  auto nth = [&](int n) {
    if (n == 11 || n == 12 || n == 13)
      return "th";
    if (n % 10 == 1)
      return "st";
    if (n % 10 == 2)
      return "nd";
    if (n % 10 == 3)
      return "rd";
    return "th";
  };
  int visits = state.visited_states.at(state.current_state);
  DrawMenuButton(0, 4, 1, 1,
                 std::format("{} Moves ({}{} Time at this State)",
                             state.total_moves, visits, nth(visits)),
                 color);

  if (DrawMenuButton(1, 4, 1, 1, "Make Optimal Move (Space)", color,
                     !state.target_distances.Empty())) {
    input.MakeOptimalMove();
  }

  if (DrawMenuButton(2, 4, 1, 1, "Undo Last Move (Backspace)", color,
                     state.history.size() > 0)) {
    input.UndoLastMove();
  }

  if (DrawMenuButton(0, 5, 1, 1, "Go to Nearest Solution (B)", color,
                     !state.target_distances.Empty())) {
    input.GoToNearestTarget();
  }

  if (DrawMenuButton(1, 5, 1, 1, "Go to Worst State (V)", color,
                     !state.target_distances.Empty())) {
    input.GoToWorstState();
  }

  if (DrawMenuButton(2, 5, 1, 1, "Go to Starting State (R)", color)) {
    input.ResetState();
  }
}

auto Gui::DrawEditControls(Color color) const -> void {
  // Toggle Target Block
  if (DrawMenuButton(0, 4, 1, 1, "Toggle Target Block (T)", color)) {
    input.ToggleTargetBlock();
  }

  // Toggle Wall Block
  if (DrawMenuButton(0, 5, 1, 1, "Toggle Wall Block (Y)", color)) {
    input.ToggleWallBlock();
  }

  // Toggle Restricted/Free Block Movement
  int free = !state.current_state.restricted;
  DrawMenuToggleSlider(1, 4, 1, 1, "Restricted (F)", "Free (F)", &free, color);
  if (free != !state.current_state.restricted) {
    input.ToggleRestrictedMovement();
  }

  // Clear Goal
  if (DrawMenuButton(1, 5, 1, 1, "Clear Goal (X)", color)) {
  }

  // Column Count Spinner
  int columns = state.current_state.width;
  DrawMenuSpinner(2, 4, 1, 1, "Cols: ", &columns, 1, 9, color);
  if (columns > state.current_state.width) {
    input.AddBoardColumn();
  } else if (columns < state.current_state.width) {
    input.RemoveBoardColumn();
  }

  // Row Count Spinner
  int rows = state.current_state.height;
  DrawMenuSpinner(2, 5, 1, 1, "Rows: ", &rows, 1, 9, color);
  if (rows > state.current_state.height) {
    input.AddBoardRow();
  } else if (rows < state.current_state.height) {
    input.RemoveBoardRow();
  }
}

auto Gui::DrawMenuFooter(Color color) -> void {
  DrawMenuButton(0, 6, 2, 1,
                 std::format("State: \"{}\"", state.current_state.state),
                 color);

  if (DrawMenuButton(2, 6, 1, 1, "Save as Preset", color)) {
    save_window = true;
  }
}

auto Gui::GetBackgroundColor() const -> Color {
  return GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR));
}

auto Gui::HelpPopup() -> void {}

auto Gui::DrawSavePresetPopup() -> void {
  if (!save_window) {
    return;
  }

  int width = 450;
  int height = 150;
  // Returns the pressed button index
  int button = GuiTextInputBox(Rectangle((GetScreenWidth() - width) / 2.0,
                                         (GetScreenHeight() - height) / 2.0,
                                         width, height),
                               "Save as Preset", "Enter Preset Name",
                               "Ok;Cancel", preset_name.data(), 255, NULL);
  if (button == 1) {
    state.AppendPresetFile(preset_name.data());
  }
  if ((button == 0) || (button == 1) || (button == 2)) {
    save_window = false;
    TextCopy(preset_name.data(), "\0");
  }
}

auto Gui::DrawMainMenu() -> void {
  menu_grid.UpdateBounds(0, 0, GetScreenWidth(), MENU_HEIGHT);

  DrawMenuHeader(GRAY);
  DrawGraphInfo(ORANGE);
  DrawGraphControls(RED);
  DrawCameraControls(DARKGREEN);

  if (input.editing) {
    DrawEditControls(PURPLE);
  } else {
    DrawPuzzleControls(BLUE);
  }

  DrawMenuFooter(GRAY);
}

auto Gui::DrawPuzzleBoard() -> void {
  board_grid.UpdateBounds(
      0, MENU_HEIGHT, GetScreenWidth() / 2, GetScreenHeight() - MENU_HEIGHT,
      state.current_state.width, state.current_state.height);

  // Draw outer border
  Rectangle bounds = board_grid.SquareBounds();
  DrawRectangleRec(bounds, state.current_state.IsWon()
                               ? BOARD_COLOR_WON
                               : BOARD_COLOR_RESTRICTED);

  // Draw inner borders
  DrawRectangle(bounds.x + BOARD_PADDING, bounds.y + BOARD_PADDING,
                bounds.width - 2 * BOARD_PADDING,
                bounds.height - 2 * BOARD_PADDING,
                state.current_state.restricted ? BOARD_COLOR_RESTRICTED
                                               : BOARD_COLOR_FREE);

  // Draw target opening
  if (state.current_state.HasWinCondition()) {
    int target_x = state.current_state.target_x;
    int target_y = state.current_state.target_y;
    Block target_block = state.current_state.GetTargetBlock();
    Rectangle target_bounds = board_grid.SquareBounds(
        target_x, target_y, target_block.width, target_block.height);

    Color opening_color = Fade(
        state.current_state.IsWon() ? BOARD_COLOR_WON : BOARD_COLOR_RESTRICTED,
        0.3);

    if (target_x == 0) {

      // Left opening
      DrawRectangle(target_bounds.x - BOARD_PADDING, target_bounds.y,
                    BOARD_PADDING, target_bounds.height, RAYWHITE);
      DrawRectangle(target_bounds.x - BOARD_PADDING, target_bounds.y,
                    BOARD_PADDING, target_bounds.height, opening_color);
    }
    if (target_x + target_block.width == state.current_state.width) {

      // Right opening
      DrawRectangle(target_bounds.x + target_bounds.width, target_bounds.y,
                    BOARD_PADDING, target_bounds.height, RAYWHITE);
      DrawRectangle(target_bounds.x + target_bounds.width, target_bounds.y,
                    BOARD_PADDING, target_bounds.height, opening_color);
    }
    if (target_y == 0) {

      // Top opening
      DrawRectangle(target_bounds.x, target_bounds.y - BOARD_PADDING,
                    target_bounds.width, BOARD_PADDING, RAYWHITE);
      DrawRectangle(target_bounds.x, target_bounds.y - BOARD_PADDING,
                    target_bounds.width, BOARD_PADDING, opening_color);
    }
    if (target_y + target_block.height == state.current_state.height) {

      // Bottom opening
      DrawRectangle(target_bounds.x, target_bounds.y + target_bounds.height,
                    target_bounds.width, BOARD_PADDING, RAYWHITE);
      DrawRectangle(target_bounds.x, target_bounds.y + target_bounds.height,
                    target_bounds.width, BOARD_PADDING, opening_color);
    }
  }

  // Draw empty cells. Also set hovered blocks
  input.hov_x = -1;
  input.hov_y = -1;
  for (int x = 0; x < board_grid.columns; ++x) {
    for (int y = 0; y < board_grid.rows; ++y) {
      DrawRectangleRec(board_grid.SquareBounds(x, y, 1, 1), RAYWHITE);

      Rectangle hov_bounds = board_grid.SquareBounds(x, y, 1, 1);
      hov_bounds.x -= BOARD_PADDING;
      hov_bounds.y -= BOARD_PADDING;
      hov_bounds.width += BOARD_PADDING;
      hov_bounds.height += BOARD_PADDING;
      if (CheckCollisionPointRec(GetMousePosition() - Vector2(0, MENU_HEIGHT),
                                 hov_bounds)) {
        input.hov_x = x;
        input.hov_y = y;
      }
    }
  }

  // Draw blocks
  for (const Block &block : state.current_state) {
    Color c = BLOCK_COLOR;
    if (block.target) {
      c = TARGET_BLOCK_COLOR;
    } else if (block.immovable) {
      c = WALL_COLOR;
    }

    DrawBoardBlock(block.x, block.y, block.width, block.height, c,
                   !block.immovable);
  }

  // Draw block placing
  if (input.editing && input.has_block_add_xy) {
    if (input.hov_x >= 0 && input.hov_x < state.current_state.width &&
        input.hov_y >= 0 && input.hov_y < state.current_state.height &&
        input.hov_x >= input.block_add_x && input.hov_y >= input.block_add_y) {
      bool collides = false;
      for (const Block &block : state.current_state) {
        if (block.Collides(Block(input.block_add_x, input.block_add_y,
                                 input.hov_x - input.block_add_x + 1,
                                 input.hov_y - input.block_add_y + 1, false))) {
          collides = true;
          break;
        }
      }
      if (!collides) {
        DrawBoardBlock(input.block_add_x, input.block_add_y,
                       input.hov_x - input.block_add_x + 1,
                       input.hov_y - input.block_add_y + 1, PURPLE, false);
      }
    }
  }
}

auto Gui::DrawGraphOverlay(int fps, int ups) -> void {
  graph_overlay_grid.UpdateBounds(GetScreenWidth() / 2, MENU_HEIGHT);

  DrawLabel(graph_overlay_grid.Bounds(0, 0, 1, 1),
            std::format("Dist: {:0>7.2f}", camera.distance), BLACK);
  DrawLabel(graph_overlay_grid.Bounds(0, 1, 1, 1),
            std::format("FoV:  {:0>6.2f}", camera.fov), BLACK);
  DrawLabel(graph_overlay_grid.Bounds(0, 2, 1, 1),
            std::format("FPS:  {:0>3}", fps), LIME);
  DrawLabel(graph_overlay_grid.Bounds(0, 3, 1, 1),
            std::format("UPS:  {:0>3}", ups), ORANGE);
}

auto Gui::Update() -> void { input.disable = WindowOpen(); }
