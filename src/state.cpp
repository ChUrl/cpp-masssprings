#include "state.hpp"
#include "config.hpp"

#include <fstream>
#include <ios>
#include <raymath.h>

#ifdef TRACY
#include "tracy.hpp"
#include <tracy/Tracy.hpp>
#endif

auto StateManager::ParsePresetFile(const std::string &preset_file) -> void {
  std::ifstream file(preset_file);
  if (!file) {
    std::cout << "Preset file \"" << preset_file << "\" couldn't be loaded."
              << std::endl;
    return;
  }

  std::string line;
  std::vector<std::string> lines;
  while (std::getline(file, line)) {
    if (line.starts_with("F") || line.starts_with("R")) {
      lines.push_back(line);
    }
  }

  if (lines.size() == 0) {
    std::cout << "Preset file \"" << preset_file << "\" couldn't be loaded."
              << std::endl;
    return;
  }

  presets.clear();
  for (const auto &preset : lines) {
    presets.emplace_back(preset);
  }

  std::cout << "Loaded " << lines.size() << " presets." << std::endl;
}

auto StateManager::LoadPreset(int preset) -> void {
  current_preset = preset;
  current_state = presets.at(current_preset);
  ClearGraph();
  edited = false;
}

auto StateManager::ResetState() -> void {
  current_state = presets.at(current_preset);
  previous_state = current_state;
  FindTargetPath();
  if (edited) {
    // We also need to clear the graph in case the state has been edited
    // because the graph could contain states that are impossible to reach
    // now.
    ClearGraph();
    edited = false;
  }
}

auto StateManager::PreviousPreset() -> void {
  LoadPreset((presets.size() + current_preset - 1) % presets.size());
}

auto StateManager::NextPreset() -> void {
  LoadPreset((current_preset + 1) % presets.size());
}

auto StateManager::NextPath() -> void {
  if (target_distances.Empty()) {
    return;
  }

  // Already there
  if (target_distances.distances[CurrentMassIndex()] == 0) {
    return;
  }

  std::size_t parent = target_distances.parents[CurrentMassIndex()];
  // std::cout << "Parent of node " << CurrentMassIndex() << " is " << parent
  //           << std::endl;
  current_state = masses.at(parent);
  FindTargetPath();
}

auto StateManager::FillGraph() -> void {
  ZoneScoped;

  ClearGraph();

  std::pair<std::vector<State>,
            std::vector<std::pair<std::size_t, std::size_t>>>
      closure = current_state.Closure();

  physics.ClearCmd();
  physics.AddMassSpringsCmd(closure.first.size(), closure.second);
  for (const State &state : closure.first) {
    states.insert(std::make_pair(state, states.size()));
    masses.insert(std::make_pair(states.size() - 1, state));
  }
  for (const auto &[from, to] : closure.second) {
    springs.emplace_back(from, to);
  }
  FindWinningStates();
  FindTargetDistances();
  FindTargetPath();

  // Sanity check. Both values need to be equal
  // for (const auto &[mass, state] : masses) {
  //   std::cout << "Masses: " << mass << ", States: " << states.at(state)
  //             << std::endl;
  // }
}

auto StateManager::UpdateGraph() -> void {
  if (previous_state == current_state) {
    return;
  }

  if (!states.contains(current_state)) {
    states.insert(std::make_pair(current_state, states.size()));
    masses.insert(std::make_pair(states.size() - 1, current_state));
    springs.emplace_back(states.at(current_state), states.at(previous_state));
    physics.AddMassCmd();
    physics.AddSpringCmd(states.at(current_state), states.at(previous_state));

    if (current_state.IsWon()) {
      winning_states.insert(current_state);
    }
    FindTargetDistances();
  }

  visited_states.insert(current_state);
  FindTargetPath();
}

auto StateManager::ClearGraph() -> void {
  states.clear();
  winning_states.clear();
  visited_states.clear();
  masses.clear();
  winning_path.clear();
  springs.clear();
  target_distances.Clear();
  physics.ClearCmd();

  // Re-add the default stuff to the graph
  states.insert(std::make_pair(current_state, states.size()));
  masses.insert(std::make_pair(states.size() - 1, current_state));
  visited_states.insert(current_state);
  physics.AddMassCmd();

  // These states are no longer in the graph
  previous_state = current_state;
  starting_state = current_state;
}

auto StateManager::FindWinningStates() -> void {
  winning_states.clear();
  for (const auto &[state, mass] : states) {
    if (state.IsWon()) {
      winning_states.insert(state);
    }
  }
}

auto StateManager::FindTargetDistances() -> void {
  ZoneScoped;

  if (springs.size() == 0 || winning_states.size() == 0) {
    return;
  }

  // Find target indices
  std::vector<std::size_t> targets;
  targets.reserve(winning_states.size());
  for (const auto &_state : winning_states) {
    targets.push_back(states.at(_state));
  }

  target_distances = CalculateDistances(states.size(), springs, targets);

  // std::cout << "Calculated " << target_distances.distances.size()
  //           << " distances to " << targets.size() << " targets." <<
  //           std::endl;
}

auto StateManager::FindTargetPath() -> void {
  if (target_distances.Empty()) {
    return;
  }

  winning_path = GetPath(target_distances, CurrentMassIndex());
  // std::cout << "Nearest target is " << winning_path.size() << " moves away."
  //           << std::endl;
}

auto StateManager::CurrentMassIndex() const -> std::size_t {
  return states.at(current_state);
}
