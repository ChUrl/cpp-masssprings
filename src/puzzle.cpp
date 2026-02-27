#include "puzzle.hpp"
#include "config.hpp"

#include <unordered_set>

#ifdef TRACY
#include "tracy.hpp"
#include <tracy/Tracy.hpp>
#endif

auto Block::Hash() const -> std::size_t {
  std::string s = std::format("{},{},{},{}", x, y, width, height);
  return std::hash<std::string>{}(s);
}

auto Block::Invalid() -> Block {
  Block block = Block(0, 0, 1, 1, false, false);
  block.width = 0;
  block.height = 0;
  return block;
}

auto Block::IsValid() const -> bool { return width != 0 && height != 0; }

auto Block::ToString() const -> std::string {
  if (target) {
    return std::format("{}{}",
                       static_cast<char>(width + static_cast<int>('a') - 1),
                       static_cast<char>(height + static_cast<int>('a') - 1));
  } else if (immovable) {
    return std::format("{}{}",
                       static_cast<char>(width + static_cast<int>('A') - 1),
                       static_cast<char>(height + static_cast<int>('A') - 1));
  } else {
    return std::format("{}{}", width, height);
  }
}

auto Block::GetPrincipalDirs() const -> int {
  if (immovable) {
    return 0;
  }

  if (width > height) {
    return Direction::EAS | Direction::WES;
  } else if (height > width) {
    return Direction::NOR | Direction::SOU;
  } else {
    return Direction::NOR | Direction::EAS | Direction::SOU | Direction::WES;
  }
}

auto Block::Covers(int xx, int yy) const -> bool {
  return xx >= x && xx < x + width && yy >= y && yy < y + height;
}

auto Block::Collides(const Block &other) const -> bool {
  return x < other.x + other.width && x + width > other.x &&
         y < other.y + other.height && y + height > other.y;
}

auto State::Hash() const -> std::size_t {
  return std::hash<std::string>{}(state);
}

auto State::HasWinCondition() const -> bool {
  return target_x != 9 && target_y != 9;
}

auto State::IsWon() const -> bool {
  if (!HasWinCondition()) {
    return false;
  }

  for (const auto &block : *this) {
    if (block.target) {
      return block.x == target_x && block.y == target_y;
    }
  }

  return false;
}

auto State::SetGoal(int x, int y) -> bool {
  Block target_block = GetTargetBlock();
  if (!target_block.IsValid() || x < 0 || x + target_block.width > width ||
      y < 0 || y + target_block.height > height) {
    return false;
  }

  if (target_x == x && target_y == y) {
    target_x = 9;
    target_y = 9;
  } else {
    target_x = x;
    target_y = y;
  }

  state.replace(3, 1, std::format("{}", target_x));
  state.replace(4, 1, std::format("{}", target_y));

  return true;
}

auto State::ClearGoal() -> void {
  target_x = 9;
  target_y = 9;
  state.replace(3, 2, "99");
}

auto State::AddColumn() const -> State {
  State newstate = State(width + 1, height, restricted);

  for (const auto &block : *this) {
    newstate.AddBlock(block);
  }

  return newstate;
}

auto State::RemoveColumn() const -> State {
  State newstate = State(width - 1, height, restricted);

  for (const auto &block : *this) {
    newstate.AddBlock(block);
  }

  return newstate;
}

auto State::AddRow() const -> State {
  State newstate = State(width, height + 1, restricted);

  for (const auto &block : *this) {
    newstate.AddBlock(block);
  }

  return newstate;
}

auto State::RemoveRow() const -> State {
  State newstate = State(width, height - 1, restricted);

  for (const auto &block : *this) {
    newstate.AddBlock(block);
  }

  return newstate;
}

auto State::AddBlock(const Block &block) -> bool {
  if (block.x + block.width > width || block.y + block.height > height) {
    return false;
  }

  for (Block b : *this) {
    if (b.Collides(block)) {
      return false;
    }
  }

  int index = GetIndex(block.x, block.y);
  state.replace(index, 2, block.ToString());

  return true;
}

auto State::GetBlock(int x, int y) const -> Block {
  if (x >= width || y >= height) {
    return Block::Invalid();
  }

  for (Block b : *this) {
    if (b.Covers(x, y)) {
      return b;
    }
  }

  return Block::Invalid();
}

auto State::GetBlockAt(int x, int y) const -> std::string {
  return state.substr(GetIndex(x, y), 2);
}

auto State::GetTargetBlock() const -> Block {
  for (Block b : *this) {
    if (b.target) {
      return b;
    }
  }

  return Block::Invalid();
}

auto State::GetIndex(int x, int y) const -> int {
  return prefix + (y * width + x) * 2;
}

auto State::RemoveBlock(int x, int y) -> bool {
  Block block = GetBlock(x, y);
  if (!block.IsValid()) {
    return false;
  }

  int index = GetIndex(block.x, block.y);
  state.replace(index, 2, "..");

  return true;
}

auto State::ToggleTarget(int x, int y) -> bool {
  Block block = GetBlock(x, y);
  if (!block.IsValid() || block.immovable) {
    return false;
  }

  // Remove the current target
  int index;
  for (const auto &b : *this) {
    if (b.target) {
      index = GetIndex(b.x, b.y);
      state.replace(index, 2,
                    Block(b.x, b.y, b.width, b.height, false).ToString());
      break;
    }
  }

  // Add the new target
  block.target = !block.target;
  index = GetIndex(block.x, block.y);
  state.replace(index, 2, block.ToString());

  return true;
}

auto State::ToggleWall(int x, int y) -> bool {
  Block block = GetBlock(x, y);
  if (!block.IsValid() || block.target) {
    return false;
  }

  // Add the new target
  block.immovable = !block.immovable;
  int index = GetIndex(block.x, block.y);
  state.replace(index, 2, block.ToString());

  return true;
}

auto State::ToggleRestricted() -> void {
  restricted = !restricted;
  state.replace(0, 1, restricted ? "R" : "F");
}

auto State::MoveBlockAt(int x, int y, Direction dir) -> bool {
  Block block = GetBlock(x, y);
  if (!block.IsValid() || block.immovable) {
    return false;
  }

  int dirs = restricted ? block.GetPrincipalDirs()
                        : Direction::NOR | Direction::EAS | Direction::SOU |
                              Direction::WES;

  // Get target block
  int _target_x = block.x;
  int _target_y = block.y;
  switch (dir) {
  case Direction::NOR:
    if (!(dirs & Direction::NOR) || _target_y < 1) {
      return false;
    }
    _target_y--;
    break;
  case Direction::EAS:
    if (!(dirs & Direction::EAS) || _target_x + block.width >= width) {
      return false;
    }
    _target_x++;
    break;
  case Direction::SOU:
    if (!(dirs & Direction::SOU) || _target_y + block.height >= height) {
      return false;
    }
    _target_y++;
    break;
  case Direction::WES:
    if (!(dirs & Direction::WES) || _target_x < 1) {
      return false;
    }
    _target_x--;
    break;
  }
  Block target =
      Block(_target_x, _target_y, block.width, block.height, block.target);

  // Check collisions
  for (Block b : *this) {
    if (b != block && b.Collides(target)) {
      return false;
    }
  }

  RemoveBlock(x, y);
  AddBlock(target);

  return true;
}

auto State::GetNextStates() const -> std::vector<State> {
  std::vector<State> new_states;

  for (const Block &b : *this) {
    int dirs = restricted ? b.GetPrincipalDirs()
                          : Direction::NOR | Direction::EAS | Direction::SOU |
                                Direction::WES;

    if (b.immovable) {
      continue;
    }

    if (dirs & Direction::NOR) {
      State north = *this;
      if (north.MoveBlockAt(b.x, b.y, Direction::NOR)) {
        new_states.push_back(north);
      }
    }

    if (dirs & Direction::EAS) {
      State east = *this;
      if (east.MoveBlockAt(b.x, b.y, Direction::EAS)) {
        new_states.push_back(east);
      }
    }

    if (dirs & Direction::SOU) {
      State south = *this;
      if (south.MoveBlockAt(b.x, b.y, Direction::SOU)) {
        new_states.push_back(south);
      }
    }

    if (dirs & Direction::WES) {
      State west = *this;
      if (west.MoveBlockAt(b.x, b.y, Direction::WES)) {
        new_states.push_back(west);
      }
    }
  }

  return new_states;
}

auto State::Closure() const
    -> std::pair<std::vector<State>,
                 std::vector<std::pair<std::size_t, std::size_t>>> {
#ifdef TRACY
  ZoneScoped;
#endif

  std::vector<State> states;
  std::vector<std::pair<std::size_t, std::size_t>> links;

  // Helper to construct the links vector
  std::unordered_map<State, std::size_t> state_indices;

  // Buffer for all states we want to call GetNextStates() on
  std::unordered_set<State> remaining_states;
  remaining_states.insert(*this);

  do {
    const State current = *remaining_states.begin();
    remaining_states.erase(current);

    if (!state_indices.contains(current)) {
      state_indices.emplace(current, states.size());
      states.push_back(current);
    }

    for (const State &s : current.GetNextStates()) {
      if (!state_indices.contains(s)) {
        remaining_states.insert(s);
        state_indices.emplace(s, states.size());
        states.push_back(s);
      }
      links.emplace_back(state_indices.at(current), state_indices.at(s));
    }
  } while (remaining_states.size() > 0);

  std::cout << std::format("State space has size {} with {} transitions.",
                           states.size(), links.size())
            << std::endl;

  return std::make_pair(states, links);
}
