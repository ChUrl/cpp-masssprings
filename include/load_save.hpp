#ifndef LOAD_SAVE_HPP_
#define LOAD_SAVE_HPP_

#include "puzzle.hpp"

#include <string>

auto parse_preset_file(const std::string& preset_file) -> std::pair<std::vector<puzzle>, std::vector<std::string>>;
auto append_preset_file(const std::string& preset_file, const std::string& preset_name, const puzzle& p) -> bool;
auto append_preset_file_quiet(const std::string& preset_file,
                              const std::string& preset_name,
                              const puzzle& p,
                              bool validate) -> bool;

#endif