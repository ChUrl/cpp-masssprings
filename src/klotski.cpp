#include "klotski.hpp"

auto Block::Hash() -> int {
  std::string s = std::format("{},{},{},{}", x, y, width, height);
  return std::hash<std::string>{}(s);
}

auto Block::Invalid() -> Block const {
  Block block = Block(0, 0, 1, 1, false);
  block.width = 0;
  block.height = 0;
  return block;
}

auto Block::IsValid() -> bool { return width != 0 && height != 0; }

auto Block::ToString() -> std::string {
  if (target) {
    return std::format("{}{}",
                       static_cast<char>(width + static_cast<int>('a') - 1),
                       static_cast<char>(height + static_cast<int>('a') - 1));
  } else {
    return std::format("{}{}", width, height);
  }
}

auto Block::Covers(int xx, int yy) -> bool {
  return xx >= x && xx < x + width && yy >= y && yy < y + height;
}

auto Block::Collides(const Block &other) -> bool {
  return x < other.x + other.width && x + width > other.x &&
         y < other.y + other.height && y + height > other.y;
}

auto State::Hash() -> int { return std::hash<std::string>{}(state); }

auto State::AddBlock(Block block) -> bool {
  if (block.x + block.width > width || block.y + block.height > height) {
    return false;
  }

  for (Block b : *this) {
    if (b.Collides(block)) {
      return false;
    }
  }

  int index = 4 + (width * block.y + block.x) * 2;
  state.replace(index, 2, block.ToString());

  return true;
}

auto State::GetBlock(int x, int y) -> Block {
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

auto State::RemoveBlock(int x, int y) -> bool {
  Block b = GetBlock(x, y);
  if (!b.IsValid()) {
    return false;
  }

  int index = 4 + (width * b.y + b.x) * 2;
  state.replace(index, 2, "..");

  return true;
}

auto State::MoveBlockAt(int x, int y, Direction dir) -> bool {
  Block block = GetBlock(x, y);
  if (!block.IsValid()) {
    return false;
  }

  // Get target block
  int target_x = block.x;
  int target_y = block.y;
  switch (dir) {
  case Direction::NOR:
    if (target_y < 1) {
      return false;
    }
    target_y--;
    break;
  case Direction::EAS:
    if (target_x + block.width >= width) {
      return false;
    }
    target_x++;
    break;
  case Direction::SOU:
    if (target_y + block.height >= height) {
      return false;
    }
    target_y++;
    break;
  case Direction::WES:
    if (target_x < 1) {
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
