#include "sliding_block_codec.hpp"

#include "sliding_block_model.hpp"

auto sliding_block_codec::parse(const std::string_view text, state_model& model) const
    -> std::optional<state_id>
{
    auto* sliding = dynamic_cast<sliding_block_model*>(&model);
    if (sliding == nullptr) {
        return std::nullopt;
    }

    try {
        const puzzle state{std::string(text)};
        if (state.try_get_invalid_reason()) {
            return std::nullopt;
        }
        return sliding->find_or_insert(state);
    }
    catch (const std::exception&) {
        return std::nullopt;
    }
}

auto sliding_block_codec::format(const state_model& model, const state_id id) const -> std::string
{
    const auto* sliding = dynamic_cast<const sliding_block_model*>(&model);
    if (sliding == nullptr) {
        return {};
    }
    return sliding->get(id).string_repr();
}

auto sliding_block_codec::validate(const state_model& model, const state_id id) const
    -> std::optional<std::string>
{
    const auto* sliding = dynamic_cast<const sliding_block_model*>(&model);
    if (sliding == nullptr) {
        return "Wrong model type";
    }
    return sliding->get(id).try_get_invalid_reason();
}
