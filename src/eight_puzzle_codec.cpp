#include "eight_puzzle_codec.hpp"

#include "eight_puzzle_model.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

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
} // namespace

auto eight_puzzle_codec::parse(const std::string_view text, state_model& model) const
    -> std::optional<state_id>
{
    auto* puzzle = dynamic_cast<eight_puzzle_model*>(&model);
    if (puzzle == nullptr) {
        return std::nullopt;
    }

    if (const auto size = find_field(text, "P8")) {
        if (*size != "3x3") {
            return std::nullopt;
        }
    }

    const auto state_field = find_field(text, "B");
    if (!state_field || static_cast<int>(state_field->size()) != eight_puzzle_model::CELLS) {
        return std::nullopt;
    }

    std::uint64_t code = 0;
    std::uint16_t seen = 0;
    for (int cell = 0; cell < eight_puzzle_model::CELLS; ++cell) {
        const char digit = (*state_field)[static_cast<size_t>(cell)];
        int value = 0;
        if (digit == '_' || digit == '0') {
            value = 0;
        } else if (digit >= '1' && digit <= '8') {
            value = digit - '0';
        } else {
            return std::nullopt;
        }

        const std::uint16_t bit = static_cast<std::uint16_t>(1u << value);
        if ((seen & bit) != 0) {
            return std::nullopt;
        }
        seen |= bit;

        code |= static_cast<std::uint64_t>(value) << (4u * static_cast<unsigned int>(cell));
    }

    if (seen != 0x1FFu || !eight_puzzle_model::is_solvable_code(code)) {
        return std::nullopt;
    }

    puzzle->configure();
    return puzzle->find_or_insert(code);
}

auto eight_puzzle_codec::format(const state_model& model, const state_id id) const -> std::string
{
    const auto* puzzle = dynamic_cast<const eight_puzzle_model*>(&model);
    if (puzzle == nullptr) {
        return {};
    }
    return puzzle->describe(id);
}

auto eight_puzzle_codec::validate(const state_model& model, const state_id id) const
    -> std::optional<std::string>
{
    const auto* puzzle = dynamic_cast<const eight_puzzle_model*>(&model);
    if (puzzle == nullptr) {
        return "Wrong model type";
    }
    if (!puzzle->is_valid(id)) {
        return "Invalid state";
    }
    if (!puzzle->is_solvable(id)) {
        return "Unsolvable permutation";
    }
    return std::nullopt;
}
