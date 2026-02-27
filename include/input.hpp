#ifndef __INPUT_HPP_
#define __INPUT_HPP_

#include "camera.hpp"
#include "config.hpp"
#include "state.hpp"

#include <functional>
#include <raylib.h>
#include <raymath.h>

class InputHandler {
  struct GenericHandler {
    std::function<void(InputHandler &)> handler;
  };

  struct MouseHandler : GenericHandler {
    MouseButton button;
  };

  struct KeyboardHandler : GenericHandler {
    KeyboardKey key;
  };

private:
  std::vector<GenericHandler> generic_handlers;
  std::vector<MouseHandler> mouse_pressed_handlers;
  std::vector<MouseHandler> mouse_released_handlers;
  std::vector<KeyboardHandler> key_pressed_handlers;
  std::vector<KeyboardHandler> key_released_handlers;

public:
  StateManager &state;
  OrbitCamera3D &camera;

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

  // Camera
  bool camera_lock = true;
  bool camera_panning = false;
  bool camera_rotating = false;

  // Mouse dragging
  Vector2 mouse = Vector2Zero();
  Vector2 last_mouse = Vector2Zero();

public:
  InputHandler(StateManager &_state, OrbitCamera3D &_camera)
      : state(_state), camera(_camera) {
    InitHandlers();
  }

  InputHandler(const InputHandler &copy) = delete;
  InputHandler &operator=(const InputHandler &copy) = delete;
  InputHandler(InputHandler &&move) = delete;
  InputHandler &operator=(InputHandler &&move) = delete;

  ~InputHandler() {}

private:
  auto InitHandlers() -> void;

public:
  // Helpers
  auto MouseInMenuPane() -> bool;
  auto MouseInBoardPane() -> bool;
  auto MouseInGraphPane() -> bool;

  // Mouse actions
  auto MouseHover() -> void;
  auto CameraStartPan() -> void;
  auto CameraPan() -> void;
  auto CameraStopPan() -> void;
  auto CameraStartRotate() -> void;
  auto CameraRotate() -> void;
  auto CameraStopRotate() -> void;
  auto CameraZoom() -> void;
  auto CameraFov() -> void;
  auto SelectBlock() -> void;
  auto StartAddBlock() -> void;
  auto ClearAddBlock() -> void;
  auto AddBlock() -> void;
  auto RemoveBlock() -> void;
  auto PlaceGoal() -> void;

  // Key actions
  auto ToggleCameraLock() -> void;
  auto ToggleCameraProjection() -> void;
  auto MoveBlockNor() -> void;
  auto MoveBlockWes() -> void;
  auto MoveBlockSou() -> void;
  auto MoveBlockEas() -> void;
  auto PrintState() const -> void;
  auto PreviousPreset() -> void;
  auto NextPreset() -> void;
  auto ResetState() -> void;
  auto FillGraph() -> void;
  auto ClearGraph() -> void;
  auto ToggleMarkSolutions() -> void;
  auto ToggleConnectSolutions() -> void;
  auto ToggleMarkPath() -> void;
  auto MakeOptimalMove() -> void;
  auto GoToWorstState() -> void;
  auto GoToNearestTarget() -> void;
  auto UndoLastMove() -> void;
  auto ToggleRestrictedMovement() -> void;
  auto ToggleTargetBlock() -> void;
  auto ToggleWallBlock() -> void;
  auto RemoveBoardColumn() -> void;
  auto AddBoardColumn() -> void;
  auto RemoveBoardRow() -> void;
  auto AddBoardRow() -> void;
  auto ToggleEditing() -> void;
  auto ClearGoal() -> void;

  // General
  auto RegisterGenericHandler(std::function<void(InputHandler &)> handler)
      -> void;

  auto RegisterMousePressedHandler(MouseButton button,
                                   std::function<void(InputHandler &)> handler)
      -> void;

  auto RegisterMouseReleasedHandler(MouseButton button,
                                    std::function<void(InputHandler &)> handler)
      -> void;

  auto RegisterKeyPressedHandler(KeyboardKey key,
                                 std::function<void(InputHandler &)> handler)
      -> void;

  auto RegisterKeyReleasedHandler(KeyboardKey key,
                                  std::function<void(InputHandler &)> handler)
      -> void;

  auto HandleInput() -> void;
};

#endif
