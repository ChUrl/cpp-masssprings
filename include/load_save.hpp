#ifndef LOAD_SAVE_HPP_
#define LOAD_SAVE_HPP_

#include "preset.hpp"
#include "puzzle.hpp"

#include <string>
#include <vector>

auto parse_preset_file(const std::string& preset_file) -> std::vector<preset_entry>;

auto append_preset_file(const std::string& preset_file, const std::string& type,
                        const std::string& preset_name, const std::string& payload) -> bool;

#endif
