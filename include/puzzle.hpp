#ifndef __PUZZLE_HPP_
#define __PUZZLE_HPP_

#include <array>
#include <cstddef>
#include <format>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

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
  Block(int _x, int _y, int _width, int _height, bool _target)
      : x(_x), y(_y), width(_width), height(_height), target(_target) {
    if (_x < 0 || _x + _width >= 10 || _y < 0 || _y + _height >= 10) {
      std::cerr << "Block must fit on a 9x9 board!" << std::endl;
      exit(1);
    }
  }

  Block(int _x, int _y, std::string block) : x(_x), y(_y) {
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

    if (_x < 0 || _x + width >= 10 || _y < 0 || _y + height >= 10) {
      std::cerr << "Block must fit on a 9x9 board!" << std::endl;
      exit(1);
    }
    if (block.length() != 2) {
      std::cerr << "Block representation must have length [2]!" << std::endl;
      exit(1);
    }
  }

  bool operator==(const Block &other) {
    return x == other.x && y == other.y && width && other.width &&
           target == other.target;
  }

  bool operator!=(const Block &other) { return !(*this == other); }

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
    BlockIterator(const State &_state) : state(_state), current_pos(0) {}

    BlockIterator(const State &_state, int _current_pos)
        : state(_state), current_pos(_current_pos) {}

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
  State(int _width, int _height, bool _restricted)
      : width(_width), height(_height), restricted(_restricted),
        state(std::format("{}{}x{}:{}", _restricted ? "R" : "F", _width,
                          _height, std::string(_width * _height * 2, '.'))) {
    if (_width <= 0 || _width >= 10 || _height <= 0 || _height >= 10) {
      std::cerr << "State width/height must be in [1, 9]!" << std::endl;
      exit(1);
    }
  }

  explicit State(std::string _state)
      : width(std::stoi(_state.substr(1, 1))),
        height(std::stoi(_state.substr(3, 1))),
        restricted(_state.substr(0, 1) == "R"), state(_state) {
    if (width <= 0 || width >= 10 || height <= 0 || height >= 10) {
      std::cerr << "State width/height must be in [1, 9]!" << std::endl;
      exit(1);
    }
    if (static_cast<int>(_state.length()) != width * height * 2 + 5) {
      std::cerr
          << "State representation must have length [width * height * 2 + 5]!"
          << std::endl;
      exit(1);
    }
  }

  bool operator==(const State &other) const { return state == other.state; }

  bool operator!=(const State &other) const { return !(*this == other); }

  BlockIterator begin() const {
    BlockIterator it = BlockIterator(*this);
    if (!(*it).IsValid()) {
      ++it;
    }
    return it;
  }

  BlockIterator end() const { return BlockIterator(*this, width * height); }

public:
  auto Hash() const -> int;

  auto AddColumn() const -> State;

  auto RemoveColumn() const -> State;

  auto AddRow() const -> State;

  auto RemoveRow() const -> State;

  auto AddBlock(const Block &block) -> bool;

  auto GetBlock(int x, int y) const -> Block;

  auto GetBlockAt(int x, int y) const -> std::string;

  auto GetIndex(int x, int y) const -> int;

  auto RemoveBlock(int x, int y) -> bool;

  auto ToggleTarget(int x, int y) -> bool;

  auto ToggleRestricted() -> void;

  auto MoveBlockAt(int x, int y, Direction dir) -> bool;

  auto GetNextStates() const -> std::vector<State>;

  auto Closure() const -> std::pair<std::unordered_set<State>,
                                    std::vector<std::pair<State, State>>>;
};

// Provide hash functions so we can use State and <State, State> as hash-set
// keys for masses and springs.
template <> struct std::hash<State> {
  std::size_t operator()(const State &s) const noexcept { return s.Hash(); }
};

template <> struct std::hash<std::pair<State, State>> {
  std::size_t operator()(const std::pair<State, State> &s) const noexcept {
    auto h1 = std::hash<State>{}(s.first);
    auto h2 = std::hash<State>{}(s.second);
    return h1 + h2 + (h1 * h2);
  }
};

template <> struct std::equal_to<std::pair<State, State>> {
  bool operator()(const std::pair<State, State> &a,
                  const std::pair<State, State> &b) const noexcept {
    return (a.first == b.first && a.second == b.second) ||
           (a.first == b.second && a.second == b.first);
  }
};

using WinCondition = std::function<bool(const State &)>;

#endif
