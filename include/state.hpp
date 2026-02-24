#ifndef __STATE_HPP_
#define __STATE_HPP_

#include "physics.hpp"
#include "presets.hpp"
#include "puzzle.hpp"

#include <raymath.h>
#include <unordered_map>
#include <unordered_set>

class StateManager {
public:
  ThreadedPhysics &physics;

  std::unordered_map<State, std::size_t> states;
  std::unordered_set<State> winning_states;
  std::unordered_set<State> visited_states;

  int current_preset;
  State starting_state;
  State current_state;
  State previous_state;

  bool edited = false;

public:
  StateManager(ThreadedPhysics &_physics)
      : physics(_physics), current_preset(0),
        starting_state(generators[current_preset]()),
        current_state(starting_state), previous_state(starting_state),
        edited(false) {
    ClearGraph();
  }

  StateManager(const StateManager &copy) = delete;
  StateManager &operator=(const StateManager &copy) = delete;
  StateManager(StateManager &&move) = delete;
  StateManager &operator=(StateManager &&move) = delete;

  ~StateManager() {}

public:
  auto LoadPreset(int preset) -> void;

  auto ResetState() -> void;

  auto PreviousPreset() -> void;

  auto NextPreset() -> void;

  auto FillGraph() -> void;

  auto UpdateGraph() -> void;

  auto ClearGraph() -> void;

  auto FindWinningStates() -> void;

  auto CurrentGenerator() const -> StateGenerator;

  auto CurrentWinCondition() const -> WinCondition;

  auto CurrentMassIndex() const -> std::size_t;
};

#endif
