#include "hanoi_codec.hpp"

#include "hanoi_model.hpp"

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

auto parse_int(const std::string_view text) -> std::optional<int>
{
    try {
        return std::stoi(std::string(text));
    }
    catch (const std::exception&) {
        return std::nullopt;
    }
}
} // namespace

auto hanoi_codec::parse(const std::string_view text, state_model& model) const
    -> std::optional<state_id>
{
    auto* hanoi = dynamic_cast<hanoi_model*>(&model);
    if (hanoi == nullptr) {
        return std::nullopt;
    }

    int disks = hanoi_model::DEFAULT_DISKS;
    int target_peg = 2;

    if (const auto field = find_field(text, "H")) {
        const std::optional<int> parsed = parse_int(*field);
        if (!parsed || *parsed < hanoi_model::MIN_DISKS || *parsed > hanoi_model::MAX_DISKS) {
            return std::nullopt;
        }
        disks = *parsed;
    }

    if (const auto field = find_field(text, "T")) {
        const std::optional<int> parsed = parse_int(*field);
        if (!parsed || *parsed < 0 || *parsed >= hanoi_model::PEG_COUNT) {
            return std::nullopt;
        }
        target_peg = *parsed;
    }

    const auto state_field = find_field(text, "B");
    if (!state_field || static_cast<int>(state_field->size()) != disks) {
        return std::nullopt;
    }

    std::uint32_t code = 0;
    for (int disk = 0; disk < disks; ++disk) {
        const char digit = (*state_field)[static_cast<size_t>(disk)];
        if (digit < '0' || digit >= static_cast<char>('0' + hanoi_model::PEG_COUNT)) {
            return std::nullopt;
        }
        const int peg = digit - '0';
        code |= static_cast<std::uint32_t>(peg) << (2u * static_cast<unsigned int>(disk));
    }

    hanoi->configure(disks, target_peg);
    return hanoi->find_or_insert(code);
}

auto hanoi_codec::format(const state_model& model, const state_id id) const -> std::string
{
    const auto* hanoi = dynamic_cast<const hanoi_model*>(&model);
    if (hanoi == nullptr) {
        return {};
    }
    return hanoi->describe(id);
}

auto hanoi_codec::validate(const state_model& model, const state_id id) const
    -> std::optional<std::string>
{
    const auto* hanoi = dynamic_cast<const hanoi_model*>(&model);
    if (hanoi == nullptr) {
        return "Wrong model type";
    }
    if (!hanoi->is_valid(id)) {
        return "Invalid state";
    }
    return std::nullopt;
}
