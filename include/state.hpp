#ifndef __STATE_HPP_
#define __STATE_HPP_

#include "config.hpp"
#include "klotski.hpp"
#include "mass_springs.hpp"
#include "presets.hpp"

#include <raymath.h>

class StateManager {
public:
  MassSpringSystem &mass_springs;

  int current_preset;
  State current_state;
  State previous_state;

  bool edited = false;

  std::unordered_set<State> winning_states;

public:
  StateManager(MassSpringSystem &mass_springs)
      : mass_springs(mass_springs), current_preset(0),
        current_state(generators[current_preset]()),
        previous_state(current_state), edited(false) {
    mass_springs.AddMass(MASS, Vector3Zero(), false, current_state);
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

  auto CurrentWinCondition() -> WinCondition;
};

#endif
