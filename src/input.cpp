#include "input.hpp"
#include "config.hpp"

#include <algorithm>
#include <raylib.h>

#ifdef TRACY
#include "tracy.hpp"
#include <tracy/Tracy.hpp>
#endif

auto InputHandler::HandleMouseHover() -> void {
  const int board_width = GetScreenWidth() / 2.0 - 2 * BOARD_PADDING;
  const int board_height = GetScreenHeight() - MENU_HEIGHT - 2 * BOARD_PADDING;
  int block_size = std::min(board_width / state.current_state.width,
                            board_height / state.current_state.height) -
                   2 * BLOCK_PADDING;
  int x_offset = (board_width - (block_size + 2 * BLOCK_PADDING) *
                                    state.current_state.width) /
                 2.0;
  int y_offset = (board_height - (block_size + 2 * BLOCK_PADDING) *
                                     state.current_state.height) /
                 2.0;

  Vector2 m = GetMousePosition();
  if (m.x < x_offset) {
    hov_x = 100;
  } else {
    hov_x = (m.x - x_offset) / (block_size + 2 * BLOCK_PADDING);
  }
  if (m.y - MENU_HEIGHT < y_offset) {
    hov_y = 100;
  } else {
    hov_y = (m.y - MENU_HEIGHT - y_offset) / (block_size + 2 * BLOCK_PADDING);
  }
}

auto InputHandler::HandleMouse() -> void {
  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    // If we clicked a block...
    if (state.current_state.GetBlock(hov_x, hov_y).IsValid()) {
      sel_x = hov_x;
      sel_y = hov_y;
    }
    // If we clicked empty space...
    else {
      // Select a position
      if (!has_block_add_xy) {
        if (hov_x >= 0 && hov_x < state.current_state.width && hov_y >= 0 &&
            hov_y < state.current_state.height) {
          block_add_x = hov_x;
          block_add_y = hov_y;
          has_block_add_xy = true;
        }
      }
      // If we have already selected a position
      else {
        int block_add_width = hov_x - block_add_x + 1;
        int block_add_height = hov_y - block_add_y + 1;
        if (block_add_width <= 0 || block_add_height <= 0) {
          block_add_x = -1;
          block_add_y = -1;
          has_block_add_xy = false;
        } else if (block_add_x >= 0 &&
                   block_add_x + block_add_width <= state.current_state.width &&
                   block_add_y >= 0 &&
                   block_add_y + block_add_height <=
                       state.current_state.height) {
          bool success = state.current_state.AddBlock(
              Block(block_add_x, block_add_y, block_add_width, block_add_height,
                    false));

          if (success) {
            block_add_x = -1;
            block_add_y = -1;
            has_block_add_xy = false;
            state.ClearGraph();
            state.edited = true;
          }
        }
      }
    }
  } else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
    if (state.current_state.RemoveBlock(hov_x, hov_y)) {
      state.ClearGraph();
      state.edited = true;
    } else if (has_block_add_xy) {
      block_add_x = -1;
      block_add_y = -1;
      has_block_add_xy = false;
    }
  } else if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
    if (hov_x >= 0 && hov_x < state.current_state.width && hov_y >= 0 &&
        hov_y < state.current_state.height) {
      if (state.current_state.SetGoal(hov_x, hov_y)) {
        // We can't just call state.FindWinningStates() because the
        // state is entirely different if it has a different win condition.
        state.ClearGraph();
      }
    }
  }
}

auto InputHandler::HandleKeys() -> void {
  if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_K)) {
    if (state.current_state.MoveBlockAt(sel_x, sel_y, Direction::NOR)) {
      sel_y--;
    }
  } else if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_H)) {
    if (state.current_state.MoveBlockAt(sel_x, sel_y, Direction::WES)) {
      sel_x--;
    }
  } else if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_J)) {
    if (state.current_state.MoveBlockAt(sel_x, sel_y, Direction::SOU)) {
      sel_y++;
    }
  } else if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_L)) {
    if (state.current_state.MoveBlockAt(sel_x, sel_y, Direction::EAS)) {
      sel_x++;
    }
  } else if (IsKeyPressed(KEY_P)) {
    std::cout << "State: " << state.current_state.state << std::endl;
    Block sel = state.current_state.GetBlock(sel_x, sel_y);
    int idx = state.current_state.GetIndex(sel.x, sel.y) - 5;
    if (sel.IsValid()) {
      std::cout << "Sel:   " << state.current_state.state.substr(0, 5)
                << std::string(idx, '.') << sel.ToString()
                << std::string(state.current_state.state.length() - idx - 7,
                               '.')
                << std::endl;
    }
  } else if (IsKeyPressed(KEY_N)) {
    block_add_x = -1;
    block_add_y = -1;
    has_block_add_xy = false;
    state.PreviousPreset();
  } else if (IsKeyPressed(KEY_M)) {
    block_add_x = -1;
    block_add_y = -1;
    has_block_add_xy = false;
    state.NextPreset();
  } else if (IsKeyPressed(KEY_R)) {
    state.ResetState();
  } else if (IsKeyPressed(KEY_G)) {
    state.FillGraph();
  } else if (IsKeyPressed(KEY_C)) {
    state.ClearGraph();
  } else if (IsKeyPressed(KEY_I)) {
    mark_solutions = !mark_solutions;
  } else if (IsKeyPressed(KEY_O)) {
    connect_solutions = !connect_solutions;
  } else if (IsKeyPressed(KEY_F)) {
    state.current_state.ToggleRestricted();
    state.ClearGraph();
    state.edited = true;
  } else if (IsKeyPressed(KEY_T)) {
    state.current_state.ToggleTarget(sel_x, sel_y);
    state.ClearGraph();
    state.edited = true;
  } else if (IsKeyPressed(KEY_LEFT) && state.current_state.width > 1) {
    state.current_state = state.current_state.RemoveColumn();
    state.ClearGraph();
    state.edited = true;
  } else if (IsKeyPressed(KEY_RIGHT) && state.current_state.width < 9) {
    state.current_state = state.current_state.AddColumn();
    state.ClearGraph();
    state.edited = true;
  } else if (IsKeyPressed(KEY_UP) && state.current_state.height > 1) {
    state.current_state = state.current_state.RemoveRow();
    state.ClearGraph();
    state.edited = true;
  } else if (IsKeyPressed(KEY_DOWN) && state.current_state.height < 9) {
    state.current_state = state.current_state.AddRow();
    state.ClearGraph();
    state.edited = true;
  }
}

auto InputHandler::HandleInput() -> void {
  HandleMouseHover();
  HandleMouse();
  HandleKeys();
}
