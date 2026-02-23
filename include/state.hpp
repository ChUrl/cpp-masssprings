#ifndef __STATE_HPP_
#define __STATE_HPP_

#include "config.hpp"
#include "physics.hpp"
#include "presets.hpp"
#include "puzzle.hpp"

#include <raymath.h>

class StateManager {
public:
  MassSpringSystem &mass_springs;

  int current_preset;
  State starting_state;
  State current_state;
  State previous_state;

  bool edited = false;

  std::unordered_set<State> winning_states;
  std::unordered_set<State> visited_states;

public:
  StateManager(MassSpringSystem &_mass_springs)
      : mass_springs(_mass_springs), current_preset(0),
        starting_state(generators[current_preset]()),
        current_state(starting_state), previous_state(starting_state),
        edited(false) {
    mass_springs.AddMass(MASS, false, current_state);
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

  auto CurrentGenerator() -> StateGenerator;

  auto CurrentWinCondition() -> WinCondition;
};

#endif
