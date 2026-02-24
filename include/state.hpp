#ifndef __STATE_HPP_
#define __STATE_HPP_

#include "config.hpp"
#include "physics.hpp"
#include "puzzle.hpp"

#include <raymath.h>
#include <unordered_map>
#include <unordered_set>

class StateManager {
public:
  ThreadedPhysics &physics;

  std::vector<State> presets;

  std::unordered_map<State, std::size_t> states;
  std::unordered_set<State> winning_states;
  std::unordered_set<State> visited_states;

  int current_preset;
  State starting_state;
  State current_state;
  State previous_state;

  bool edited = false;

public:
  StateManager(ThreadedPhysics &_physics, const std::string &preset_file)
      : physics(_physics), presets({State()}), current_preset(0),
        edited(false) {
    ParsePresetFile(preset_file);
    current_state = presets.at(current_preset);
    ClearGraph();
  }

  StateManager(const StateManager &copy) = delete;
  StateManager &operator=(const StateManager &copy) = delete;
  StateManager(StateManager &&move) = delete;
  StateManager &operator=(StateManager &&move) = delete;

  ~StateManager() {}

private:
  auto ParsePresetFile(const std::string &preset_file) -> void;

public:
  auto LoadPreset(int preset) -> void;

  auto ResetState() -> void;

  auto PreviousPreset() -> void;

  auto NextPreset() -> void;

  auto FillGraph() -> void;

  auto UpdateGraph() -> void;

  auto ClearGraph() -> void;

  auto FindWinningStates() -> void;

  auto CurrentMassIndex() const -> std::size_t;
};

#endif
