#include "klotski.hpp"

auto Block::Hash() const -> int {
  std::string s = std::format("{},{},{},{}", x, y, width, height);
  return std::hash<std::string>{}(s);
}

auto Block::Invalid() -> Block {
  Block block = Block(0, 0, 1, 1, false);
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
  } else {
    return std::format("{}{}", width, height);
  }
}

auto Block::GetPrincipalDirs() const -> int {
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

auto State::Hash() const -> int { return std::hash<std::string>{}(state); }

auto State::AddBlock(Block block) -> bool {
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

auto State::GetIndex(int x, int y) const -> int {
  return 5 + (y * width + x) * 2;
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

auto State::MoveBlockAt(int x, int y, Direction dir) -> bool {
  Block block = GetBlock(x, y);
  if (!block.IsValid()) {
    return false;
  }

  int dirs = restricted ? block.GetPrincipalDirs()
                        : Direction::NOR | Direction::EAS | Direction::SOU |
                              Direction::WES;

  // Get target block
  int target_x = block.x;
  int target_y = block.y;
  switch (dir) {
  case Direction::NOR:
    if (!(dirs & Direction::NOR) || target_y < 1) {
      return false;
    }
    target_y--;
    break;
  case Direction::EAS:
    if (!(dirs & Direction::EAS) || target_x + block.width >= width) {
      return false;
    }
    target_x++;
    break;
  case Direction::SOU:
    if (!(dirs & Direction::SOU) || target_y + block.height >= height) {
      return false;
    }
    target_y++;
    break;
  case Direction::WES:
    if (!(dirs & Direction::WES) || target_x < 1) {
      return false;
    }
    target_x--;
    break;
  }
  Block target =
      Block(target_x, target_y, block.width, block.height, block.target);

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
    -> std::pair<std::unordered_set<std::string>,
                 std::vector<std::pair<std::string, std::string>>> {
  std::unordered_set<std::string> states;
  std::vector<std::pair<std::string, std::string>> links;

  std::unordered_set<State> remaining_states;
  remaining_states.insert(*this);

  do {
    const State current = *remaining_states.begin();
    remaining_states.erase(current);

    std::vector<State> new_states = current.GetNextStates();
    for (State &s : new_states) {
      if (!states.contains(s.state)) {
        remaining_states.insert(s);
        states.insert(s.state);
      }
      links.emplace_back(current.state, s.state);
    }
  } while (remaining_states.size() > 0);

  std::cout << "Closure contains " << states.size() << " states with "
            << links.size() << " links." << std::endl;

  return std::make_pair(states, links);
}
