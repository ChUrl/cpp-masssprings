#include "load_save.hpp"

#include <fstream>

namespace
{
auto trim_carriage_return(std::string& line) -> void
{
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
}

auto write_preset(std::fstream& file, const std::string& type, const std::string& comment,
                  const std::string& payload) -> void
{
    file << "\n@type " << type << "\n# " << (comment.empty() ? type : comment) << "\n"
         << payload << std::flush;
}
} // namespace

auto parse_preset_file(const std::string& preset_file) -> std::vector<preset_entry>
{
    std::fstream file(preset_file, std::ios::in);
    if (!file) {
        infoln("Preset file \"{}\" couldn't be opened.", preset_file);
        return {};
    }

    std::vector<preset_entry> presets;
    std::string current_type = "sliding_block";
    std::string pending_comment;

    std::string line;
    while (std::getline(file, line)) {
        trim_carriage_return(line);

        if (line.empty()) {
            continue;
        }

        if (line.starts_with("@type ")) {
            current_type = line.substr(6);
            while (!current_type.empty() &&
                   (current_type.back() == ' ' || current_type.back() == '\t')) {
                current_type.pop_back();
            }
            continue;
        }

        if (line.starts_with("#")) {
            pending_comment = line;
            continue;
        }

        presets.push_back(preset_entry{
            current_type,
            pending_comment.empty() ? std::string("# ") + current_type : pending_comment,
            line,
        });
        pending_comment.clear();
    }

    infoln("Loaded {} presets from \"{}\".", presets.size(), preset_file);
    return presets;
}

auto append_preset_file(const std::string& preset_file, const std::string& type,
                        const std::string& preset_name, const std::string& payload) -> bool
{
    infoln(R"(Saving preset "{}" to "{}")", preset_name, preset_file);

    std::fstream file(preset_file, std::ios_base::app | std::ios_base::out);
    if (!file) {
        infoln("Preset file \"{}\" couldn't be opened.", preset_file);
        return false;
    }

    write_preset(file, type, preset_name, payload);
    return true;
}
