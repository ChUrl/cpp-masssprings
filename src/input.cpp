#include "input.hpp"
#include "config.hpp"

#include <algorithm>
#include <print>
#include <raylib.h>

#ifdef TRACY
#include "tracy.hpp"
#include <tracy/Tracy.hpp>
#endif

auto InputHandler::InitHandlers() -> void {
  // The order matters if multiple handlers are registered to the same key

  RegisterGenericHandler(&InputHandler::CameraPan);
  RegisterGenericHandler(&InputHandler::CameraRotate);
  RegisterGenericHandler(&InputHandler::CameraZoom);
  RegisterGenericHandler(&InputHandler::CameraFov);
  RegisterGenericHandler(&InputHandler::MouseHover);

  RegisterMousePressedHandler(MOUSE_BUTTON_LEFT, &InputHandler::CameraStartPan);
  RegisterMousePressedHandler(MOUSE_BUTTON_LEFT, &InputHandler::SelectBlock);
  RegisterMousePressedHandler(MOUSE_BUTTON_LEFT, &InputHandler::AddBlock);
  RegisterMousePressedHandler(MOUSE_BUTTON_LEFT, &InputHandler::StartAddBlock);
  RegisterMousePressedHandler(MOUSE_BUTTON_RIGHT,
                              &InputHandler::CameraStartRotate);
  RegisterMousePressedHandler(MOUSE_BUTTON_RIGHT, &InputHandler::RemoveBlock);
  RegisterMousePressedHandler(MOUSE_BUTTON_RIGHT, &InputHandler::ClearAddBlock);
  RegisterMousePressedHandler(MOUSE_BUTTON_MIDDLE, &InputHandler::PlaceGoal);

  RegisterMouseReleasedHandler(MOUSE_BUTTON_LEFT, &InputHandler::CameraStopPan);
  RegisterMouseReleasedHandler(MOUSE_BUTTON_RIGHT,
                               &InputHandler::CameraStopRotate);

  RegisterKeyPressedHandler(KEY_W, &InputHandler::MoveBlockNor);
  RegisterKeyPressedHandler(KEY_D, &InputHandler::MoveBlockEas);
  RegisterKeyPressedHandler(KEY_S, &InputHandler::MoveBlockSou);
  RegisterKeyPressedHandler(KEY_A, &InputHandler::MoveBlockWes);
  RegisterKeyPressedHandler(KEY_P, &InputHandler::PrintState);
  RegisterKeyPressedHandler(KEY_N, &InputHandler::PreviousPreset);
  RegisterKeyPressedHandler(KEY_M, &InputHandler::NextPreset);
  RegisterKeyPressedHandler(KEY_R, &InputHandler::ResetState);
  RegisterKeyPressedHandler(KEY_G, &InputHandler::FillGraph);
  RegisterKeyPressedHandler(KEY_C, &InputHandler::ClearGraph);
  RegisterKeyPressedHandler(KEY_I, &InputHandler::ToggleMarkSolutions);
  RegisterKeyPressedHandler(KEY_O, &InputHandler::ToggleConnectSolutions);
  RegisterKeyPressedHandler(KEY_U, &InputHandler::ToggleMarkPath);
  RegisterKeyPressedHandler(KEY_SPACE, &InputHandler::MakeOptimalMove);
  RegisterKeyPressedHandler(KEY_V, &InputHandler::GoToWorstState);
  RegisterKeyPressedHandler(KEY_B, &InputHandler::GoToNearestTarget);
  RegisterKeyPressedHandler(KEY_BACKSPACE, &InputHandler::UndoLastMove);
  RegisterKeyPressedHandler(KEY_F, &InputHandler::ToggleRestrictedMovement);
  RegisterKeyPressedHandler(KEY_T, &InputHandler::ToggleTargetBlock);
  RegisterKeyPressedHandler(KEY_Y, &InputHandler::ToggleWallBlock);
  RegisterKeyPressedHandler(KEY_UP, &InputHandler::AddBoardRow);
  RegisterKeyPressedHandler(KEY_RIGHT, &InputHandler::AddBoardColumn);
  RegisterKeyPressedHandler(KEY_DOWN, &InputHandler::RemoveBoardRow);
  RegisterKeyPressedHandler(KEY_LEFT, &InputHandler::RemoveBoardColumn);
  RegisterKeyPressedHandler(KEY_TAB, &InputHandler::ToggleEditing);
  RegisterKeyPressedHandler(KEY_L, &InputHandler::ToggleCameraLock);
  RegisterKeyPressedHandler(KEY_LEFT_ALT,
                            &InputHandler::ToggleCameraProjection);
  RegisterKeyPressedHandler(KEY_X, &InputHandler::ClearGoal);
}

auto InputHandler::MouseInMenuPane() -> bool { return mouse.y < MENU_HEIGHT; }

auto InputHandler::MouseInBoardPane() -> bool {
  return mouse.x < GetScreenWidth() / 2.0 && mouse.y >= MENU_HEIGHT;
}

auto InputHandler::MouseInGraphPane() -> bool {
  return mouse.x >= GetScreenWidth() / 2.0 && mouse.y >= MENU_HEIGHT;
}

auto InputHandler::MouseHover() -> void {
  last_mouse = mouse;
  mouse = GetMousePosition();
}

auto InputHandler::CameraStartPan() -> void {
  if (!MouseInGraphPane()) {
    return;
  }

  camera_panning = true;
  // Enable this if the camera should be pannable even when locked (releasing
  // the lock in the process):
  // camera_lock = false;
}

auto InputHandler::CameraPan() -> void {
  if (camera_panning) {
    camera.Pan(last_mouse, mouse);
  }
}

auto InputHandler::CameraStopPan() -> void { camera_panning = false; }

auto InputHandler::CameraStartRotate() -> void {
  if (!MouseInGraphPane()) {
    return;
  }

  camera_rotating = true;
}

auto InputHandler::CameraRotate() -> void {
  if (camera_rotating) {
    camera.Rotate(last_mouse, mouse);
  }
}

auto InputHandler::CameraStopRotate() -> void { camera_rotating = false; }

auto InputHandler::CameraZoom() -> void {
  if (!MouseInGraphPane() || IsKeyDown(KEY_LEFT_CONTROL) ||
      camera.projection == CAMERA_ORTHOGRAPHIC) {
    return;
  }

  float wheel = GetMouseWheelMove();

  if (IsKeyDown(KEY_LEFT_SHIFT)) {
    camera.distance -= wheel * ZOOM_SPEED * ZOOM_MULTIPLIER;
  } else {
    camera.distance -= wheel * ZOOM_SPEED;
  }
}

auto InputHandler::CameraFov() -> void {
  if (!MouseInGraphPane() || !IsKeyDown(KEY_LEFT_CONTROL) ||
      IsKeyDown(KEY_LEFT_SHIFT)) {
    return;
  }

  float wheel = GetMouseWheelMove();
  camera.fov -= wheel * FOV_SPEED;
}

auto InputHandler::ToggleCameraLock() -> void {
  if (!camera_lock) {
    camera_panning = false;
  }

  camera_lock = !camera_lock;
}

auto InputHandler::ToggleCameraProjection() -> void {
  camera.projection = camera.projection == CAMERA_PERSPECTIVE
                          ? CAMERA_ORTHOGRAPHIC
                          : CAMERA_PERSPECTIVE;
}

auto InputHandler::SelectBlock() -> void {
  if (state.current_state.GetBlock(hov_x, hov_y).IsValid()) {
    sel_x = hov_x;
    sel_y = hov_y;
  }
}

auto InputHandler::StartAddBlock() -> void {
  if (!editing || state.current_state.GetBlock(hov_x, hov_y).IsValid() ||
      has_block_add_xy) {
    return;
  }

  if (hov_x >= 0 && hov_x < state.current_state.width && hov_y >= 0 &&
      hov_y < state.current_state.height) {
    block_add_x = hov_x;
    block_add_y = hov_y;
    has_block_add_xy = true;
  }
}

auto InputHandler::ClearAddBlock() -> void {
  if (!editing || !has_block_add_xy) {
    return;
  }

  block_add_x = -1;
  block_add_y = -1;
  has_block_add_xy = false;
}

auto InputHandler::AddBlock() -> void {
  if (!editing || state.current_state.GetBlock(hov_x, hov_y).IsValid() ||
      !has_block_add_xy) {
    return;
  }

  int block_add_width = hov_x - block_add_x + 1;
  int block_add_height = hov_y - block_add_y + 1;
  if (block_add_width <= 0 || block_add_height <= 0) {
    block_add_x = -1;
    block_add_y = -1;
    has_block_add_xy = false;
  } else if (block_add_x >= 0 &&
             block_add_x + block_add_width <= state.current_state.width &&
             block_add_y >= 0 &&
             block_add_y + block_add_height <= state.current_state.height) {
    bool success = state.current_state.AddBlock(Block(
        block_add_x, block_add_y, block_add_width, block_add_height, false));

    if (success) {
      sel_x = block_add_x;
      sel_y = block_add_y;
      block_add_x = -1;
      block_add_y = -1;
      has_block_add_xy = false;
      state.ClearGraph();
      state.edited = true;
    }
  }
}

auto InputHandler::RemoveBlock() -> void {
  Block block = state.current_state.GetBlock(hov_x, hov_y);
  if (!editing || has_block_add_xy || !block.IsValid() ||
      !state.current_state.RemoveBlock(hov_x, hov_y)) {
    return;
  }

  if (block.Covers(sel_x, sel_y)) {
    sel_x = 0;
    sel_y = 0;
  }

  state.ClearGraph();
  state.edited = true;
}

auto InputHandler::PlaceGoal() -> void {
  if (!editing || hov_x < 0 || hov_x >= state.current_state.width ||
      hov_y < 0 || hov_y >= state.current_state.height) {
    return;
  }

  if (state.current_state.SetGoal(hov_x, hov_y)) {
    // We can't just call state.FindWinningStates() because the
    // state is entirely different if it has a different win condition.
    state.ClearGraph();
  }
}

auto InputHandler::MoveBlockNor() -> void {
  if (state.current_state.MoveBlockAt(sel_x, sel_y, Direction::NOR)) {
    sel_y--;
  }
}

auto InputHandler::MoveBlockEas() -> void {
  if (state.current_state.MoveBlockAt(sel_x, sel_y, Direction::EAS)) {
    sel_x++;
  }
}

auto InputHandler::MoveBlockSou() -> void {
  if (state.current_state.MoveBlockAt(sel_x, sel_y, Direction::SOU)) {
    sel_y++;
  }
}

auto InputHandler::MoveBlockWes() -> void {
  if (state.current_state.MoveBlockAt(sel_x, sel_y, Direction::WES)) {
    sel_x--;
  }
}

auto InputHandler::PrintState() const -> void {
  std::println("State: \"{}\"", state.current_state.state);
  Block sel = state.current_state.GetBlock(sel_x, sel_y);
  int idx = state.current_state.GetIndex(sel.x, sel.y) - State::prefix;
  if (sel.IsValid()) {
    std::println("Sel:   \"{}{}{}{}\"",
                 state.current_state.state.substr(0, State::prefix),
                 std::string(idx, '.'), sel.ToString(),
                 std::string(state.current_state.state.length() - idx -
                                 State::prefix - 2,
                             '.'));
  }
}

auto InputHandler::PreviousPreset() -> void {
  if (editing) {
    return;
  }

  block_add_x = -1;
  block_add_y = -1;
  has_block_add_xy = false;
  state.PreviousPreset();
}

auto InputHandler::NextPreset() -> void {
  if (editing) {
    return;
  }

  block_add_x = -1;
  block_add_y = -1;
  has_block_add_xy = false;
  state.NextPreset();
}

auto InputHandler::ResetState() -> void {
  if (editing) {
    return;
  }

  state.ResetState();
}

auto InputHandler::FillGraph() -> void { state.FillGraph(); }

auto InputHandler::ClearGraph() -> void { state.ClearGraph(); }

auto InputHandler::ToggleMarkSolutions() -> void {
  mark_solutions = !mark_solutions;
}

auto InputHandler::ToggleConnectSolutions() -> void {
  connect_solutions = !connect_solutions;
}

auto InputHandler::ToggleMarkPath() -> void { mark_path = !mark_path; }

auto InputHandler::MakeOptimalMove() -> void { state.NextPath(); }

auto InputHandler::GoToWorstState() -> void { state.GoToWorst(); }

auto InputHandler::GoToNearestTarget() -> void { state.GoToNearestTarget(); }

auto InputHandler::UndoLastMove() -> void { state.PopHistory(); }

auto InputHandler::ToggleRestrictedMovement() -> void {
  if (!editing) {
    return;
  }

  state.current_state.ToggleRestricted();
  state.ClearGraph();
  state.edited = true;
}

auto InputHandler::ToggleTargetBlock() -> void {
  if (!editing) {
    return;
  }

  state.current_state.ToggleTarget(sel_x, sel_y);
  state.ClearGraph();
  state.edited = true;
}

auto InputHandler::ToggleWallBlock() -> void {
  if (!editing) {
    return;
  }

  state.current_state.ToggleWall(sel_x, sel_y);
  state.ClearGraph();
  state.edited = true;
}

auto InputHandler::RemoveBoardColumn() -> void {
  if (!editing || state.current_state.width <= 1) {
    return;
  }

  state.current_state = state.current_state.RemoveColumn();
  state.ClearGraph();
  state.edited = true;
}

auto InputHandler::AddBoardColumn() -> void {
  if (!editing || state.current_state.width >= 9) {
    return;
  }

  state.current_state = state.current_state.AddColumn();
  state.ClearGraph();
  state.edited = true;
}

auto InputHandler::RemoveBoardRow() -> void {
  if (!editing || state.current_state.height <= 1) {
    return;
  }

  state.current_state = state.current_state.RemoveRow();
  state.ClearGraph();
  state.edited = true;
}

auto InputHandler::AddBoardRow() -> void {
  if (!editing || state.current_state.height >= 9) {
    return;
  }

  state.current_state = state.current_state.AddRow();
  state.ClearGraph();
  state.edited = true;
}

auto InputHandler::ToggleEditing() -> void {
  if (editing) {
    has_block_add_xy = false;
    block_add_x = -1;
    block_add_y = -1;
  }

  editing = !editing;
}

auto InputHandler::ClearGoal() -> void {
  if (!editing) {
    return;
  }

  state.current_state.ClearGoal();
  state.ClearGraph();
  state.edited = true;
}

auto InputHandler::RegisterGenericHandler(
    std::function<void(InputHandler &)> handler) -> void {
  generic_handlers.push_back({handler});
}

auto InputHandler::RegisterMousePressedHandler(
    MouseButton button, std::function<void(InputHandler &)> handler) -> void {
  mouse_pressed_handlers.push_back({handler, button});
}

auto InputHandler::RegisterMouseReleasedHandler(
    MouseButton button, std::function<void(InputHandler &)> handler) -> void {
  mouse_released_handlers.push_back({handler, button});
}

auto InputHandler::RegisterKeyPressedHandler(
    KeyboardKey key, std::function<void(InputHandler &)> handler) -> void {
  key_pressed_handlers.push_back({handler, key});
}

auto InputHandler::RegisterKeyReleasedHandler(
    KeyboardKey key, std::function<void(InputHandler &)> handler) -> void {
  key_released_handlers.push_back({handler, key});
}

auto InputHandler::HandleInput() -> void {
  if (disable) {
    return;
  }

  for (const GenericHandler &handler : generic_handlers) {
    handler.handler(*this);
  }

  for (const MouseHandler &handler : mouse_pressed_handlers) {
    if (IsMouseButtonPressed(handler.button)) {
      handler.handler(*this);
    }
  }

  for (const MouseHandler &handler : mouse_released_handlers) {
    if (IsMouseButtonReleased(handler.button)) {
      handler.handler(*this);
    }
  }

  for (const KeyboardHandler &handler : key_pressed_handlers) {
    if (IsKeyPressed(handler.key)) {
      handler.handler(*this);
    }
  }

  for (const KeyboardHandler &handler : key_released_handlers) {
    if (IsKeyReleased(handler.key)) {
      handler.handler(*this);
    }
  }
}
