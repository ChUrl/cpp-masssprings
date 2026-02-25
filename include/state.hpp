#ifndef __STATE_HPP_
#define __STATE_HPP_

#include "config.hpp"
#include "distance.hpp"
#include "physics.hpp"
#include "puzzle.hpp"

#include <raymath.h>
#include <unordered_map>
#include <unordered_set>

class StateManager {
public:
  ThreadedPhysics &physics;

  std::vector<State> presets;

  // Some stuff is faster to map from state to mass (e.g. in the renderer)
  std::unordered_map<State, std::size_t> states;
  std::unordered_set<State> winning_states;
  std::unordered_set<State> visited_states;

  // Other stuff maps from mass to state :/
  std::unordered_map<std::size_t, State> masses;
  std::vector<std::size_t> winning_path;

  // Fuck it, duplicate the springs too, we don't even need to copy them from
  // the physics thread then...
  std::vector<std::pair<std::size_t, std::size_t>> springs;

  // Distance calculation result can be buffered and reused to calculate a new
  // path on the same graph
  DistanceResult target_distances;

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

  auto NextPath() -> void;

  auto FillGraph() -> void;

  auto UpdateGraph() -> void;

  auto ClearGraph() -> void;

  auto FindWinningStates() -> void;

  auto FindTargetDistances() -> void;

  auto FindTargetPath() -> void;

  auto FindWorstState() -> State;

  auto GoToWorst() -> void;

  auto CurrentMassIndex() const -> std::size_t;
};

#endif
