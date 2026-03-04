#include "load_save.hpp"

#include <fstream>

auto parse_preset_file(const std::string& preset_file) -> std::pair<std::vector<puzzle>, std::vector<std::string>>
{
    std::fstream file(preset_file, std::ios::in);
    if (!file) {
        infoln("Preset file \"{}\" couldn't be opened.", preset_file);
        return {};
    }

    std::string line;
    std::vector<std::string> comment_lines;
    std::vector<std::string> preset_lines;
    while (std::getline(file, line)) {
        if (line.starts_with("S")) {
            preset_lines.push_back(line);
        } else if (line.starts_with("#")) {
            comment_lines.push_back(line);
        }
    }

    if (preset_lines.empty() || comment_lines.size() != preset_lines.size()) {
        infoln("Preset file \"{}\" couldn't be opened.", preset_file);
        return {};
    }

    std::vector<puzzle> preset_states;
    for (const auto& preset : preset_lines) {
        // Each char is a bit
        const puzzle& p = puzzle(preset);

        if (const std::optional<std::string>& reason = p.try_get_invalid_reason()) {
            infoln("Preset file \"{}\" contained invalid presets: {}", preset_file, *reason);
            return {};
        }
        preset_states.emplace_back(p);
    }

    infoln("Loaded {} presets from \"{}\".", preset_lines.size(), preset_file);

    return {preset_states, comment_lines};
}

auto append_preset_file(const std::string& preset_file, const std::string& preset_name, const puzzle& p) -> bool
{
    infoln(R"(Saving preset "{}" to "{}")", preset_name, preset_file);

    if (p.try_get_invalid_reason()) {
        return false;
    }

    std::fstream file(preset_file, std::ios_base::app | std::ios_base::out);
    if (!file) {
        infoln("Preset file \"{}\" couldn't be opened.", preset_file);
        return false;
    }

    file << "\n# " << preset_name << "\n" << p.string_repr() << std::flush;

    return true;
}

auto append_preset_file_quiet(const std::string& preset_file, const std::string& preset_name, const puzzle& p, const bool validate) -> bool
{
    if (validate && p.try_get_invalid_reason()) {
        return false;
    }

    std::fstream file(preset_file, std::ios_base::app | std::ios_base::out);
    if (!file) {
        return false;
    }

    file << "\n# " << preset_name << "\n" << p.string_repr() << std::flush;

    return true;
}