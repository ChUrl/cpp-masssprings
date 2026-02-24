#include "state.hpp"
#include "config.hpp"
#include "presets.hpp"

#include <raymath.h>

#ifdef TRACY
#include "tracy.hpp"
#include <tracy/Tracy.hpp>
#endif

auto StateManager::LoadPreset(int preset) -> void {
  current_preset = preset;
  current_state = CurrentGenerator()();
  ClearGraph();
  edited = false;
}

auto StateManager::ResetState() -> void {
  current_state = CurrentGenerator()();
  previous_state = current_state;
  if (edited) {
    // We also need to clear the graph in case the state has been edited
    // because the graph could contain states that are impossible to reach now.
    ClearGraph();
    edited = false;
  }
}

auto StateManager::PreviousPreset() -> void {
  LoadPreset((generators.size() + current_preset - 1) % generators.size());
}

auto StateManager::NextPreset() -> void {
  LoadPreset((current_preset + 1) % generators.size());
}

auto StateManager::FillGraph() -> void {
  ClearGraph();

  std::pair<std::vector<State>,
            std::vector<std::pair<std::size_t, std::size_t>>>
      closure = current_state.Closure();

  physics.ClearCmd();
  physics.AddMassSpringsCmd(closure.first.size(), closure.second);
  for (const State &state : closure.first) {
    states.insert(std::make_pair(state, states.size()));
  }
  FindWinningStates();
}

auto StateManager::UpdateGraph() -> void {
  if (previous_state == current_state) {
    return;
  }

  if (!states.contains(current_state)) {
    states.insert(std::make_pair(current_state, states.size()));
    physics.AddMassCmd();
    physics.AddSpringCmd(states.at(current_state), states.at(previous_state));
  }

  visited_states.insert(current_state);
  if (win_conditions[current_preset](current_state)) {
    winning_states.insert(current_state);
  }
}

auto StateManager::ClearGraph() -> void {
  states.clear();
  winning_states.clear();
  visited_states.clear();
  physics.ClearCmd();

  states.insert(std::make_pair(current_state, states.size()));
  visited_states.insert(current_state);
  physics.AddMassCmd();

  // These states are no longer in the graph
  previous_state = current_state;
  starting_state = current_state;
}

auto StateManager::FindWinningStates() -> void {
  winning_states.clear();
  for (const auto &[state, mass] : states) {
    if (CurrentWinCondition()(state)) {
      winning_states.insert(state);
    }
  }
}

auto StateManager::CurrentGenerator() const -> StateGenerator {
  return generators[current_preset];
}

auto StateManager::CurrentWinCondition() const -> WinCondition {
  return win_conditions[current_preset];
}

auto StateManager::CurrentMassIndex() const -> std::size_t {
  return states.at(current_state);
}
