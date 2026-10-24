#include "sudoku4_codec.hpp"

#include "sudoku4_model.hpp"

#include <bit>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace
{
constexpr int MIN_GIVENS = 4;

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

auto parse_cells(const std::string_view text, std::uint16_t& mask, std::uint64_t& code) -> bool
{
    if (static_cast<int>(text.size()) != sudoku4_model::CELLS) {
        return false;
    }

    mask = 0;
    code = 0;
    for (int cell = 0; cell < sudoku4_model::CELLS; ++cell) {
        const char digit = text[static_cast<size_t>(cell)];
        if (digit == '_' || digit == '0') {
            continue;
        }
        if (digit < '1' || digit > '4') {
            return false;
        }

        const int value = digit - '0';
        mask |= static_cast<std::uint16_t>(1u << cell);
        code |= static_cast<std::uint64_t>(value) << (4u * static_cast<unsigned int>(cell));
    }

    return true;
}
} // namespace

auto sudoku4_codec::parse(const std::string_view text, state_model& model) const
    -> std::optional<state_id>
{
    auto* sudoku = dynamic_cast<sudoku4_model*>(&model);
    if (sudoku == nullptr) {
        return std::nullopt;
    }

    if (const auto size = find_field(text, "S4")) {
        if (*size != "4x4") {
            return std::nullopt;
        }
    }

    const auto givens_field = find_field(text, "G");
    const auto state_field = find_field(text, "B");
    if (!givens_field || !state_field) {
        return std::nullopt;
    }

    std::uint16_t given_mask = 0;
    std::uint64_t given_code = 0;
    if (!parse_cells(*givens_field, given_mask, given_code)) {
        return std::nullopt;
    }
    if (std::popcount(static_cast<unsigned int>(given_mask)) < MIN_GIVENS) {
        return std::nullopt;
    }
    if (!sudoku4_model::is_consistent_code(given_code, given_mask, given_code)) {
        return std::nullopt;
    }

    std::uint16_t state_mask = 0;
    std::uint64_t state_code = 0;
    if (!parse_cells(*state_field, state_mask, state_code)) {
        return std::nullopt;
    }
    (void)state_mask;
    if (!sudoku4_model::is_consistent_code(state_code, given_mask, given_code)) {
        return std::nullopt;
    }

    sudoku->configure(given_mask, given_code);
    return sudoku->find_or_insert(state_code);
}

auto sudoku4_codec::format(const state_model& model, const state_id id) const -> std::string
{
    const auto* sudoku = dynamic_cast<const sudoku4_model*>(&model);
    if (sudoku == nullptr) {
        return {};
    }
    return sudoku->describe(id);
}

auto sudoku4_codec::validate(const state_model& model, const state_id id) const
    -> std::optional<std::string>
{
    const auto* sudoku = dynamic_cast<const sudoku4_model*>(&model);
    if (sudoku == nullptr) {
        return "Wrong model type";
    }
    if (!sudoku->is_valid(id)) {
        return "Invalid Sudoku state";
    }
    return std::nullopt;
}
