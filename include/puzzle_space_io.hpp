#ifndef PUZZLE_SPACE_IO_HPP_
#define PUZZLE_SPACE_IO_HPP_

#include "puzzle_space.hpp"

#include <optional>
#include <string>

struct puzzle_space_io_result
{
    std::optional<puzzle_space_result> result;
    std::string error;
};

auto save_puzzle_space_file(const std::string& path, const puzzle_space_result& result)
    -> std::optional<std::string>;
auto load_puzzle_space_file(const std::string& path) -> puzzle_space_io_result;

#endif
