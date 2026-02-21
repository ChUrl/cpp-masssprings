#ifndef __INPUT_HPP_
#define __INPUT_HPP_

#include "renderer.hpp"
#include "state.hpp"

class InputHandler {
public:
  StateManager &state;
  Renderer &renderer;

  int hov_x;
  int hov_y;
  int sel_x;
  int sel_y;

  bool has_block_add_xy = false;
  int block_add_x = -1;
  int block_add_y = -1;

public:
  InputHandler(StateManager &state, Renderer &renderer)
      : state(state), renderer(renderer), hov_x(-1), hov_y(-1), sel_x(-1),
        sel_y(-1), has_block_add_xy(false), block_add_x(-1), block_add_y(-1) {}

  InputHandler(const InputHandler &copy) = delete;
  InputHandler &operator=(const InputHandler &copy) = delete;
  InputHandler(InputHandler &&move) = delete;
  InputHandler &operator=(InputHandler &&move) = delete;

  ~InputHandler() {}

public:
  auto HandleMouseHover() -> void;

  auto HandleMouse() -> void;

  auto HandleKeys() -> void;

  auto HandleInput() -> void;
};

#endif
