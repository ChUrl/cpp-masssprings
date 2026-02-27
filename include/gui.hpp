#ifndef __GUI_HPP_
#define __GUI_HPP_

#include "camera.hpp"
#include "config.hpp"
#include "input.hpp"
#include "state.hpp"

#include <raygui.h>
#include <raylib.h>

class Grid {
public:
  int x;
  int y;
  int width;
  int height;
  int columns;
  int rows;
  const int padding;

public:
  Grid(int _x, int _y, int _width, int _height, int _columns, int _rows,
       int _padding)
      : x(_x), y(_y), width(_width), height(_height), columns(_columns),
        rows(_rows), padding(_padding) {}

public:
  auto UpdateBounds(int _x, int _y, int _width, int _height, int _columns,
                    int _rows) -> void;

  auto UpdateBounds(int _x, int _y, int _width, int _height) -> void;

  auto UpdateBounds(int _x, int _y) -> void;

  auto Bounds() const -> Rectangle;

  auto Bounds(int _x, int _y, int _width, int _height) const -> Rectangle;

  auto SquareBounds() const -> Rectangle;

  auto SquareBounds(int _x, int _y, int _width, int _height) const -> Rectangle;
};

class Gui {
  struct Style {
    int border_color_normal;
    int base_color_normal;
    int text_color_normal;

    int border_color_focused;
    int base_color_focused;
    int text_color_focused;

    int border_color_pressed;
    int base_color_pressed;
    int text_color_pressed;

    int border_color_disabled;
    int base_color_disabled;
    int text_color_disabled;
  };

  struct DefaultStyle : Style {
    int background_color;
    int line_color;

    int text_size;
    int text_spacing;
    int text_line_spacing;
    int text_alignment_vertical;
    int text_wrap_mode;
  };

  struct ComponentStyle : Style {
    int border_width;
    int text_padding;
    int text_alignment;
  };

private:
  InputHandler &input;
  StateManager &state;
  const OrbitCamera3D &camera;

  Grid menu_grid =
      Grid(0, 0, GetScreenWidth(), MENU_HEIGHT, MENU_COLS, MENU_ROWS, MENU_PAD);

  Grid board_grid = Grid(
      0, MENU_HEIGHT, GetScreenWidth() / 2, GetScreenHeight() - MENU_HEIGHT,
      state.current_state.width, state.current_state.height, BOARD_PADDING);

  Grid graph_overlay_grid =
      Grid(GetScreenWidth() / 2, MENU_HEIGHT, 200, 100, 1, 4, MENU_PAD);

  bool save_window = false;
  std::array<char, 256> preset_name = {0};
  bool help_window = false;

public:
  Gui(InputHandler &_input, StateManager &_state, const OrbitCamera3D &_camera)
      : input(_input), state(_state), camera(_camera) {
    Init();
  }

  Gui(const Gui &copy) = delete;
  Gui &operator=(const Gui &copy) = delete;
  Gui(Gui &&move) = delete;
  Gui &operator=(Gui &&move) = delete;

private:
  auto Init() const -> void;

  auto ApplyColor(Style &style, Color color) const -> void;

  auto ApplyBlockColor(Style &style, Color color) const -> void;

  auto ApplyTextColor(Style &style, Color color) const -> void;

  auto GetDefaultStyle() const -> DefaultStyle;

  auto SetDefaultStyle(const DefaultStyle &style) const -> void;

  auto GetComponentStyle(int component) const -> ComponentStyle;

  auto SetComponentStyle(int component, const ComponentStyle &style) const
      -> void;

  auto DrawButton(Rectangle bounds, const std::string &label, Color color,
                  bool enabled = true, int font_size = FONT_SIZE) const -> int;

  auto DrawMenuButton(int x, int y, int width, int height,
                      const std::string &label, Color color,
                      bool enabled = true, int font_size = FONT_SIZE) const
      -> int;

  auto DrawToggleSlider(Rectangle bounds, const std::string &off_label,
                        const std::string &on_label, int *active, Color color,
                        bool enabled = true, int font_size = FONT_SIZE) const
      -> int;

  auto DrawMenuToggleSlider(int x, int y, int width, int height,
                            const std::string &off_label,
                            const std::string &on_label, int *active,
                            Color color, bool enabled = true,
                            int font_size = FONT_SIZE) const -> int;

  auto DrawSpinner(Rectangle bounds, const std::string &label, int *value,
                   int min, int max, Color color, bool enabled = true,
                   int font_size = FONT_SIZE) const -> int;

  auto DrawMenuSpinner(int x, int y, int width, int height,
                       const std::string &label, int *value, int min, int max,
                       Color color, bool enabled = true,
                       int font_size = FONT_SIZE) const -> int;

  auto DrawLabel(Rectangle bounds, const std::string &text, Color color,
                 bool enabled = true, int font_size = FONT_SIZE) const -> int;

  auto DrawBoardBlock(int x, int y, int width, int height, Color color,
                      bool enabled = true) const -> bool;

  auto WindowOpen() const -> bool;

  // Different menu sections
  auto DrawMenuHeader(Color color) const -> void;
  auto DrawGraphInfo(Color color) const -> void;
  auto DrawGraphControls(Color color) const -> void;
  auto DrawCameraControls(Color color) const -> void;
  auto DrawPuzzleControls(Color color) const -> void;
  auto DrawEditControls(Color color) const -> void;
  auto DrawMenuFooter(Color color) -> void;

public:
  auto GetBackgroundColor() const -> Color;

  auto HelpPopup() -> void;

  auto DrawSavePresetPopup() -> void;

  auto DrawMainMenu() -> void;

  auto DrawPuzzleBoard() -> void;

  auto DrawGraphOverlay(int fps, int ups) -> void;

  auto Update() -> void;
};

#endif
