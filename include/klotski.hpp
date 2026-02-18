#ifndef __KLOTSKI_HPP_
#define __KLOTSKI_HPP_

#include <array>
#include <cstddef>
#include <format>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

// #define DBG_PRINT

enum Direction {
  NOR = 1 << 0,
  EAS = 1 << 1,
  SOU = 1 << 2,
  WES = 1 << 3,
};

// A block is represented as a 2-digit string "wh", where w is the block width
// and h the block height.
// The target block (to remove from the board) is represented as a 2-letter
// string "xy", where x is the block width and y the block height and
// width/height are represented by [abcdefghi] (~= [123456789]).
class Block {
public:
  int x;
  int y;
  int width;
  int height;
  bool target;

public:
  Block(int x, int y, int width, int height, bool target)
      : x(x), y(y), width(width), height(height), target(target) {
    if (x < 0 || x + width >= 10 || y < 0 || y + height >= 10) {
      std::cerr << "Block must fit on a 9x9 board!" << std::endl;
      exit(1);
    }

#ifdef DBG_PRINT
    std::cout << ToString() << std::endl;
#endif
  }

  Block(int x, int y, std::string block) : x(x), y(y) {
    if (block == "..") {
      this->x = 0;
      this->y = 0;
      width = 0;
      height = 0;
      target = false;
      return;
    }

    const std::array<char, 9> chars{'a', 'b', 'c', 'd', 'e',
                                    'f', 'g', 'h', 'i'};

    target = false;
    for (const char c : chars) {
      if (block.contains(c)) {
        target = true;
        break;
      }
    }

    if (target) {
      width = static_cast<int>(block.at(0)) - static_cast<int>('a') + 1;
      height = static_cast<int>(block.at(1)) - static_cast<int>('a') + 1;
    } else {
      width = std::stoi(block.substr(0, 1));
      height = std::stoi(block.substr(1, 1));
    }

    if (x < 0 || x + width >= 10 || y < 0 || y + height >= 10) {
      std::cerr << "Block must fit on a 9x9 board!" << std::endl;
      exit(1);
    }
    if (block.length() != 2) {
      std::cerr << "Block representation must have length [2]!" << std::endl;
      exit(1);
    }

#ifdef DBG_PRINT
    std::cout << "At: (" << x << ", " << y << "), Size: (" << width << ", "
              << height << "), Target: " << target << std::endl;
#endif
  }

  Block(const Block &copy)
      : x(copy.x), y(copy.y), width(copy.width), height(copy.height),
        target(copy.target) {}

  Block &operator=(const Block &copy) = delete;

  Block(Block &&move)
      : x(move.x), y(move.y), width(move.width), height(move.height),
        target(move.target) {}

  Block &operator=(Block &&move) = delete;

  bool operator==(const Block &other) {
    return x == other.x && y == other.y && width && other.width &&
           target == other.target;
  }

  bool operator!=(const Block &other) { return !(*this == other); }

  ~Block() {}

public:
  auto Hash() const -> int;

  static auto Invalid() -> Block;

  auto IsValid() const -> bool;

  auto ToString() const -> std::string;

  auto GetPrincipalDirs() const -> int;

  auto Covers(int xx, int yy) const -> bool;

  auto Collides(const Block &other) const -> bool;
};

// A state is represented by a string "WxH:blocks", where W is the board width,
// H is the board height and blocks is an enumeration of each of the board's
// cells, with each cell being a 2-letter or 2-digit block representation (a 3x3
// board would have a string representation with length 4 + 3*3 * 2).
// The board's cells are enumerated from top-left to bottom-right with each
// block's pivot being its top-left corner.
class State {
public:
  int width;
  int height;
  bool restricted; // Only allow blocks to move in their principal direction
  std::string state;

  // https://en.cppreference.com/w/cpp/iterator/input_iterator.html
  class BlockIterator {
  public:
    using difference_type = std::ptrdiff_t;
    using value_type = Block;

  private:
    const State &state;
    int current_pos;

  public:
    BlockIterator(const State &state) : state(state), current_pos(0) {}

    BlockIterator(const State &state, int current_pos)
        : state(state), current_pos(current_pos) {}

    Block operator*() const {
      return Block(current_pos % state.width, current_pos / state.width,
                   state.state.substr(current_pos * 2 + 5, 2));
    }

    BlockIterator &operator++() {
      do {
        current_pos++;
      } while (state.state.substr(current_pos * 2 + 5, 2) == "..");
      return *this;
    }

    bool operator==(const BlockIterator &other) {
      return state == other.state && current_pos == other.current_pos;
    }

    bool operator!=(const BlockIterator &other) { return !(*this == other); }
  };

public:
  State(int width, int height, bool restricted)
      : width(width), height(height), restricted(restricted),
        state(std::format("{}{}x{}:{}", restricted ? "R" : "F", width, height,
                          std::string(width * height * 2, '.'))) {
    if (width <= 0 || width >= 10 || height <= 0 || height >= 10) {
      std::cerr << "State width/height must be in [1, 9]!" << std::endl;
      exit(1);
    }

#ifdef DBG_PRINT
    std::cout << "State(" << width << ", " << height << "): \"" << state << "\""
              << std::endl;
#endif
  }

  State(std::string state)
      : width(std::stoi(state.substr(1, 1))),
        height(std::stoi(state.substr(3, 1))),
        restricted(state.substr(0, 1) == "R"), state(state) {
    if (width <= 0 || width >= 10 || height <= 0 || height >= 10) {
      std::cerr << "State width/height must be in [1, 9]!" << std::endl;
      exit(1);
    }
    if (state.length() != width * height * 2 + 5) {
      std::cerr
          << "State representation must have length [width * height * 2 + 5]!"
          << std::endl;
      exit(1);
    }
  }

  State(const State &copy)
      : width(copy.width), height(copy.height), restricted(copy.restricted),
        state(copy.state) {}

  State &operator=(const State &copy) {
    if (*this != copy) {
      width = copy.width;
      height = copy.height;
      restricted = copy.restricted;
      state = copy.state;
    }
    return *this;
  }

  State(State &&move)
      : width(move.width), height(move.height), restricted(move.restricted),
        state(std::move(move.state)) {}

  State &operator=(State &&move) {
    if (*this != move) {
      width = move.width;
      height = move.height;
      restricted = move.restricted, state = std::move(move.state);
    }
    return *this;
  };

  bool operator==(const State &other) const { return state == other.state; }

  bool operator!=(const State &other) const { return !(*this == other); }

  BlockIterator begin() const { return BlockIterator(*this); }

  BlockIterator end() const { return BlockIterator(*this, width * height); }

  ~State() {}

public:
  auto Hash() const -> int;

  auto AddBlock(Block block) -> bool;

  auto GetBlock(int x, int y) const -> Block;

  auto GetBlockAt(int x, int y) const -> std::string;

  auto GetIndex(int x, int y) const -> int;

  auto RemoveBlock(int x, int y) -> bool;

  auto MoveBlockAt(int x, int y, Direction dir) -> bool;

  auto GetNextStates() const -> std::vector<State>;

  auto Closure() const
      -> std::pair<std::unordered_set<std::string>,
                   std::vector<std::pair<std::string, std::string>>>;
};

template <> struct std::hash<State> {
  std::size_t operator()(const State &s) const noexcept { return s.Hash(); }
};

using WinCondition = std::function<bool(const State &)>;

#endif
