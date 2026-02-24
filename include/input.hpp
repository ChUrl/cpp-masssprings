#ifndef __INPUT_HPP_
#define __INPUT_HPP_

#include "state.hpp"

class InputHandler {
public:
  StateManager &state;

  int hov_x;
  int hov_y;
  int sel_x;
  int sel_y;

  bool has_block_add_xy;
  int block_add_x;
  int block_add_y;

  bool mark_solutions;
  bool connect_solutions;

public:
  InputHandler(StateManager &_state)
      : state(_state), hov_x(-1), hov_y(-1), sel_x(0), sel_y(0),
        has_block_add_xy(false), block_add_x(-1), block_add_y(-1),
        mark_solutions(false), connect_solutions(false) {}

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
