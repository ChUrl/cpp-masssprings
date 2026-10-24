#include "lights_out_codec.hpp"

#include "lights_out_model.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace
{
auto find_field(const std::string_view text, const std::string_view key)
    -> std::optional<std::string_view>
{
    const std::string marker = std::string(key) + ":[";
    const size_t start = text.find(marker);
    if (start == std::string_view::npos) {
        return std::nullopt;
    }

    const size_t value_start = start + marker.size();
    const size_t end = text.find(']', value_start);
    if (end == std::string_view::npos) {
        return std::nullopt;
    }

    return text.substr(value_start, end - value_start);
}

auto parse_int(const std::string_view text) -> std::optional<int>
{
    try {
        return std::stoi(std::string(text));
    }
    catch (const std::exception&) {
        return std::nullopt;
    }
}

auto parse_size(const std::string_view text) -> std::optional<std::pair<int, int>>
{
    const size_t separator = text.find('x');
    if (separator == std::string_view::npos) {
        return std::nullopt;
    }

    const std::optional<int> width = parse_int(text.substr(0, separator));
    const std::optional<int> height = parse_int(text.substr(separator + 1));
    if (!width || !height) {
        return std::nullopt;
    }
    return std::pair<int, int>{*width, *height};
}

auto parse_bits(const std::string_view text, const int cell_count) -> std::optional<std::uint32_t>
{
    if (static_cast<int>(text.size()) != cell_count) {
        return std::nullopt;
    }

    std::uint32_t mask = 0;
    for (int i = 0; i < cell_count; ++i) {
        const char bit = text[static_cast<size_t>(i)];
        if (bit == '1') {
            mask |= 1u << i;
        } else if (bit != '0') {
            return std::nullopt;
        }
    }
    return mask;
}

auto parse_digits(const std::string_view text, const int base, const int cell_count)
    -> std::optional<std::uint64_t>
{
    if (static_cast<int>(text.size()) != cell_count) {
        return std::nullopt;
    }

    std::uint64_t code = 0;
    std::uint64_t power = 1;
    for (int i = 0; i < cell_count; ++i) {
        const int digit = text[static_cast<size_t>(i)] - '0';
        if (digit < 0 || digit >= base) {
            return std::nullopt;
        }
        code += static_cast<std::uint64_t>(digit) * power;
        power *= static_cast<std::uint64_t>(base);
    }
    return code;
}

auto parse_neighborhood(const std::string_view text) -> std::optional<lights_out_neighborhood>
{
    if (text == "orth") {
        return lights_out_neighborhood::orthogonal;
    }
    if (text == "diag") {
        return lights_out_neighborhood::diagonal;
    }
    if (text == "orth_wrap") {
        return lights_out_neighborhood::orthogonal_wrap;
    }
    if (text == "diag_wrap") {
        return lights_out_neighborhood::diagonal_wrap;
    }
    return std::nullopt;
}
} // namespace

auto lights_out_codec::parse(const std::string_view text, state_model& model) const
    -> std::optional<state_id>
{
    auto* lights = dynamic_cast<lights_out_model*>(&model);
    if (lights == nullptr) {
        return std::nullopt;
    }

    lights_out_rules rules;

    if (const auto size = find_field(text, "LO")) {
        const auto parsed_size = parse_size(*size);
        if (!parsed_size) {
            return std::nullopt;
        }
        rules.width = parsed_size->first;
        rules.height = parsed_size->second;
    }

    const int cells = rules.cell_count();
    if (cells <= 0 || cells > 32) {
        return std::nullopt;
    }

    if (const auto base = find_field(text, "V")) {
        const auto parsed_base = parse_int(*base);
        if (!parsed_base || (*parsed_base != 2 && *parsed_base != 3)) {
            return std::nullopt;
        }
        rules.base = *parsed_base;
    }

    if (rules.base == 3 && cells > 25) {
        return std::nullopt;
    }

    if (const auto neighborhood = find_field(text, "N")) {
        const auto parsed_neighborhood = parse_neighborhood(*neighborhood);
        if (!parsed_neighborhood) {
            return std::nullopt;
        }
        rules.neighborhood = *parsed_neighborhood;
    }

    if (const auto pressable = find_field(text, "P")) {
        const auto parsed_mask = parse_bits(*pressable, cells);
        if (!parsed_mask) {
            return std::nullopt;
        }
        rules.pressable_mask = *parsed_mask;
    } else {
        rules.pressable_mask = cells >= 32 ? 0xFFFFFFFFu : ((1u << cells) - 1u);
    }

    if (const auto target = find_field(text, "T")) {
        const auto parsed_target = parse_digits(*target, rules.base, cells);
        if (!parsed_target) {
            return std::nullopt;
        }
        rules.target = *parsed_target;
    }

    const auto state_field = find_field(text, "B");
    if (!state_field) {
        return std::nullopt;
    }
    const auto state = parse_digits(*state_field, rules.base, cells);
    if (!state) {
        return std::nullopt;
    }

    rules.name = rules.is_classic() ? "Lights Out 4x4" : "Lights Out Custom";

    lights->configure(rules);
    return lights->find_or_insert(*state);
}

auto lights_out_codec::format(const state_model& model, const state_id id) const -> std::string
{
    const auto* lights = dynamic_cast<const lights_out_model*>(&model);
    if (lights == nullptr) {
        return {};
    }
    return lights->describe(id);
}

auto lights_out_codec::validate(const state_model& model, const state_id id) const
    -> std::optional<std::string>
{
    const auto* lights = dynamic_cast<const lights_out_model*>(&model);
    if (lights == nullptr) {
        return "Wrong model type";
    }
    if (!lights->is_valid(id)) {
        return "Invalid state";
    }
    return std::nullopt;
}
