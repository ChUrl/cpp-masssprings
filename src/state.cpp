#include "state.hpp"
#include "config.hpp"
#include "presets.hpp"

#include <raymath.h>

auto StateManager::LoadPreset(int preset) -> void {
  current_state = generators[preset]();
  previous_state = current_state;
  ClearGraph();
  current_preset = preset;
  edited = false;
}

auto StateManager::ResetState() -> void {
  current_state = generators[current_preset]();
  previous_state = current_state;
  if (edited) {
    // We also need to clear the graph, in case the state has been edited.
    // Then the graph would contain states that are impossible to reach.
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

  std::pair<std::unordered_set<State>, std::vector<std::pair<State, State>>>
      closure = current_state.Closure();
  for (const auto &state : closure.first) {
    mass_springs.AddMass(MASS, false, state);
  }
  for (const auto &[from, to] : closure.second) {
    mass_springs.AddSpring(from, to, SPRING_CONSTANT, DAMPENING_CONSTANT,
                           REST_LENGTH);
  }
  std::cout << "Inserted " << mass_springs.masses.size() << " masses and "
            << mass_springs.springs.size() << " springs." << std::endl;
  FindWinningStates();
  std::cout << "Consuming "
            << sizeof(decltype(*mass_springs.masses.begin())) *
                   mass_springs.masses.size()
            << " Bytes for masses." << std::endl;
  std::cout << "Consuming "
            << sizeof(decltype(*mass_springs.springs.begin())) *
                   mass_springs.springs.size()
            << " Bytes for springs." << std::endl;
}

auto StateManager::UpdateGraph() -> void {
  if (previous_state != current_state) {
    mass_springs.AddMass(MASS, false, current_state);
    mass_springs.AddSpring(current_state, previous_state, SPRING_CONSTANT,
                           DAMPENING_CONSTANT, REST_LENGTH);
    if (win_conditions[current_preset](current_state)) {
      winning_states.insert(current_state);
    }
  }
}

auto StateManager::ClearGraph() -> void {
  winning_states.clear();
  mass_springs.Clear();
  mass_springs.AddMass(MASS, false, current_state);

  // The previous_state is no longer in the graph
  previous_state = current_state;
}

auto StateManager::FindWinningStates() -> void {
  winning_states.clear();
  for (const auto &[state, mass] : mass_springs.masses) {
    if (win_conditions[current_preset](state)) {
      winning_states.insert(state);
    }
  }

  std::cout << "Found " << winning_states.size() << " winning states."
            << std::endl;
}

auto StateManager::CurrentWinCondition() -> WinCondition {
  return win_conditions[current_preset];
}
