#ifndef __PRESETS_HPP_
#define __PRESETS_HPP_

#include <functional>
#include <vector>

#include "klotski.hpp"

using StateGenerator = std::function<State(void)>;

inline auto state_simple_1r() -> State {
  State s = State(4, 5, true);
  s.AddBlock(Block(0, 0, 1, 2, true));

  return s;
}

inline auto state_simple_1r_wc(const State &state) -> bool { return false; }

inline auto state_simple_1f() -> State {
  State s = State(4, 5, false);
  s.AddBlock(Block(0, 0, 1, 2, false));

  return s;
}

inline auto state_simple_1f_wc(const State &state) -> bool { return false; }

inline auto state_simple_2r() -> State {
  State s = State(4, 5, true);
  s.AddBlock(Block(0, 0, 1, 2, false));
  s.AddBlock(Block(1, 0, 1, 2, false));

  return s;
}

inline auto state_simple_2r_wc(const State &state) -> bool { return false; }

inline auto state_simple_2f() -> State {
  State s = State(4, 5, false);
  s.AddBlock(Block(0, 0, 1, 2, false));
  s.AddBlock(Block(1, 0, 1, 2, false));

  return s;
}

inline auto state_simple_2f_wc(const State &state) -> bool { return false; }

inline auto state_simple_3r() -> State {
  State s = State(4, 5, true);
  s.AddBlock(Block(0, 0, 1, 2, false));
  s.AddBlock(Block(1, 0, 1, 2, false));
  s.AddBlock(Block(2, 0, 1, 2, false));

  return s;
}

inline auto state_simple_3r_wc(const State &state) -> bool { return false; }

inline auto state_simple_3f() -> State {
  State s = State(4, 5, false);
  s.AddBlock(Block(0, 0, 1, 2, false));
  s.AddBlock(Block(1, 0, 1, 2, false));
  s.AddBlock(Block(2, 0, 1, 2, false));

  return s;
}

inline auto state_simple_3f_wc(const State &state) -> bool { return false; }

inline auto state_complex_1r() -> State {
  State s = State(6, 6, true);
  s.AddBlock(Block(1, 0, 1, 3, false));
  s.AddBlock(Block(3, 0, 2, 1, false));
  s.AddBlock(Block(5, 0, 1, 3, false));
  s.AddBlock(Block(3, 2, 2, 1, true));
  s.AddBlock(Block(3, 3, 1, 2, false));
  s.AddBlock(Block(4, 4, 2, 1, false));

  return s;
}

inline auto state_complex_1r_wc(const State &state) -> bool {
  return state.GetBlockAt(4, 2) == "ba";
}

inline auto state_complex_2r() -> State {
  State s = State(6, 6, true);
  s.AddBlock(Block(2, 0, 1, 3, false));
  s.AddBlock(Block(0, 2, 2, 1, true));
  s.AddBlock(Block(1, 3, 2, 1, false));
  s.AddBlock(Block(1, 4, 2, 1, false));
  s.AddBlock(Block(5, 4, 1, 2, false));
  s.AddBlock(Block(0, 5, 3, 1, false));

  return s;
}

inline auto state_complex_2r_wc(const State &state) -> bool {
  return state.GetBlockAt(4, 2) == "ba";
}

inline auto state_complex_3r() -> State {
  State s = State(6, 6, true);
  s.AddBlock(Block(0, 0, 3, 1, false));
  s.AddBlock(Block(5, 0, 1, 3, false));
  s.AddBlock(Block(2, 2, 1, 2, false));
  s.AddBlock(Block(3, 2, 2, 1, true));
  s.AddBlock(Block(3, 3, 1, 2, false));
  s.AddBlock(Block(4, 3, 2, 1, false));
  s.AddBlock(Block(0, 4, 1, 2, false));
  s.AddBlock(Block(2, 4, 1, 2, false));
  s.AddBlock(Block(4, 4, 2, 1, false));
  s.AddBlock(Block(3, 5, 3, 1, false));

  return s;
}

inline auto state_complex_3r_wc(const State &state) -> bool {
  return state.GetBlockAt(4, 2) == "ba";
}

inline auto state_complex_4f() -> State {
  State s = State(4, 4, false);
  s.AddBlock(Block(0, 0, 2, 1, false));
  s.AddBlock(Block(3, 0, 1, 1, false));
  s.AddBlock(Block(0, 1, 1, 2, false));
  s.AddBlock(Block(1, 1, 2, 2, false));
  s.AddBlock(Block(3, 1, 1, 1, false));
  s.AddBlock(Block(3, 2, 1, 1, false));
  s.AddBlock(Block(0, 3, 1, 1, false));
  s.AddBlock(Block(1, 3, 1, 1, false));

  return s;
}

inline auto state_complex_4f_wc(const State &state) -> bool { return false; }

inline auto state_complex_5r() -> State {
  return State("R6x6:31....12....1221......12..ba..1212..21..12....12......21.."
               "....21..21....");
}

inline auto state_complex_5r_wc(const State &state) -> bool {
  return state.GetBlockAt(4, 2) == "ba";
}

inline auto state_complex_6r() -> State {
  return State(
      "R6x6:1231....1213..31........ba..121212..21................1221....."
      ".....21..");
}

inline auto state_complex_6r_wc(const State &state) -> bool {
  return state.GetBlockAt(4, 2) == "ba";
}

inline auto state_klotski() -> State {
  State s = State(4, 5, false);
  s.AddBlock(Block(0, 0, 1, 2, false));
  s.AddBlock(Block(1, 0, 2, 2, true));
  s.AddBlock(Block(3, 0, 1, 2, false));
  s.AddBlock(Block(0, 2, 1, 2, false));
  s.AddBlock(Block(1, 2, 2, 1, false));
  s.AddBlock(Block(3, 2, 1, 2, false));
  s.AddBlock(Block(1, 3, 1, 1, false));
  s.AddBlock(Block(2, 3, 1, 1, false));
  s.AddBlock(Block(0, 4, 1, 1, false));
  s.AddBlock(Block(3, 4, 1, 1, false));

  return s;
}

inline auto state_klotski_wc(const State &state) -> bool {
  return state.GetBlockAt(1, 3) == "bb";
}

inline auto state_century() -> State {
  return State("F4x5:11bb..1112....12..12....11....1121..21..");
}

inline auto state_century_wc(const State &state) -> bool {
  return state.GetBlockAt(1, 3) == "bb";
}

inline auto state_super_century() -> State {
  return State("F4x5:12111111..12bb..12........21..11....21..");
}

inline auto state_super_century_wc(const State &state) -> bool {
  return state.GetBlockAt(1, 3) == "bb";
}

inline auto state_new_century() -> State {
  return State("F4x5:12111111..12bb..12........21..11....21..");
}

inline auto state_new_century_wc(const State &state) -> bool {
  // What kind of brain do you need for this???
  return state.state == "F4x5:21......1121..12bb..12........12111111..";
}

static std::vector<StateGenerator> generators{
    state_simple_1r,  state_simple_2r,  state_simple_3r,  state_complex_1r,
    state_complex_2r, state_complex_3r, state_complex_4f, state_complex_5r,
    state_complex_6r, state_klotski,    state_century,    state_super_century,
    state_new_century};

static std::vector<WinCondition> win_conditions{
    state_simple_1r_wc,  state_simple_2r_wc,  state_simple_3r_wc,
    state_complex_1r_wc, state_complex_2r_wc, state_complex_3r_wc,
    state_complex_4f_wc, state_complex_5r_wc, state_complex_6r_wc,
    state_klotski_wc,    state_century_wc,    state_super_century_wc,
    state_new_century_wc};

#endif
