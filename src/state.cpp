#include "state.hpp"
#include "config.hpp"
#include "distance.hpp"

#include <fstream>
#include <ios>
#include <print>
#include <raymath.h>

#ifdef TRACY
#include "tracy.hpp"
#include <tracy/Tracy.hpp>
#endif

auto StateManager::ParsePresetFile(const std::string &_preset_file) -> bool {
  preset_file = _preset_file;

  std::ifstream file(preset_file);
  if (!file) {
    std::println("Preset file \"{}\" couldn't be loaded.", preset_file);
    return false;
  }

  std::string line;
  std::vector<std::string> comment_lines;
  std::vector<std::string> preset_lines;
  while (std::getline(file, line)) {
    if (line.starts_with("F") || line.starts_with("R")) {
      preset_lines.push_back(line);
    } else if (line.starts_with("#")) {
      comment_lines.push_back(line);
    }
  }

  if (preset_lines.size() == 0 || comment_lines.size() != preset_lines.size()) {
    std::println("Preset file \"{}\" couldn't be loaded.", preset_file);
    return false;
  }

  presets.clear();
  for (const auto &preset : preset_lines) {
    presets.emplace_back(preset);
  }
  comments = comment_lines;

  std::println("Loaded {} presets from \"{}\".", preset_lines.size(),
               preset_file);

  return true;
}

auto StateManager::AppendPresetFile(const std::string preset_name) -> void {
  std::println("Saving preset \"{}\" to \"{}\"", preset_name, preset_file);

  std::ofstream file(preset_file, std::ios_base::app | std::ios_base::out);
  if (!file) {
    std::println("Preset file \"{}\" couldn't be loaded.", preset_file);
    return;
  }

  file << "\n# " << preset_name << "\n" << current_state.state << std::flush;

  std::println("Refreshing presets...");
  if (ParsePresetFile(preset_file)) {
    LoadPreset(presets.size() - 1);
  }
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
  for (auto &[state, visits] : visited_states) {
    visits = 0;
  }
  visited_states[current_state]++;
  total_moves = 0;
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
  current_state = masses.at(parent);
  FindTargetPath();
}

auto StateManager::FillGraph() -> void {
#ifdef TRACY
  ZoneScoped;
#endif

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
  //   std::println("Masses: {}, States: {}", mass, states.at(state));
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

  // Adds the element with 0 if it doesn't exist
  visited_states[current_state]++;
  total_moves++;

  if (history.size() > 0 && history.top() == current_state) {
    // We don't pop the stack when moving backwards to indicate if we need to
    // push or pop here
    history.pop();
  } else {
    history.push(previous_state);
  }

  FindTargetPath();
  previous_state = current_state;
}

auto StateManager::ClearGraph() -> void {
  states.clear();
  winning_states.clear();
  visited_states.clear();
  masses.clear();
  winning_path.clear();
  springs.clear();
  history = std::stack<State>();
  target_distances.Clear();
  physics.ClearCmd();

  // Re-add the default stuff to the graph
  states.insert(std::make_pair(current_state, states.size()));
  masses.insert(std::make_pair(states.size() - 1, current_state));
  visited_states.insert(std::make_pair(current_state, 1));
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
#ifdef TRACY
  ZoneScoped;
#endif

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

  // std::println("Calculated {} distances to {} targets.",
  //              target_distances.distances.size(), targets.size());
}

auto StateManager::FindTargetPath() -> void {
  if (target_distances.Empty()) {
    return;
  }

  winning_path = GetPath(target_distances, CurrentMassIndex());
  // std::println("Nearest target is {} moves away.", winning_path.size());
}

auto StateManager::FindWorstState() -> State {
  if (target_distances.Empty()) {
    return current_state;
  }

  int max = 0;
  int index = 0;
  for (std::size_t i = 0; i < target_distances.distances.size(); ++i) {
    if (target_distances.distances.at(i) > max) {
      max = target_distances.distances.at(i);
      index = i;
    }
  }

  return masses.at(index);
}

auto StateManager::GoToWorst() -> void { current_state = FindWorstState(); }

auto StateManager::GoToNearestTarget() -> void {
  if (target_distances.Empty()) {
    return;
  }

  current_state =
      masses.at(target_distances.nearest_targets.at(CurrentMassIndex()));
}

auto StateManager::PopHistory() -> void {
  if (history.size() == 0) {
    return;
  }

  current_state = history.top();
  // history.pop(); // Done in UpdateGraph();
}

auto StateManager::CurrentMassIndex() const -> std::size_t {
  return states.at(current_state);
}
