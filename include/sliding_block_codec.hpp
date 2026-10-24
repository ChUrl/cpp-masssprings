#ifndef SLIDING_BLOCK_CODEC_HPP_
#define SLIDING_BLOCK_CODEC_HPP_

#include "puzzle_codec.hpp"

class sliding_block_codec : public puzzle_codec
{
public:
    [[nodiscard]] auto parse(std::string_view text, state_model& model) const
        -> std::optional<state_id> override;
    [[nodiscard]] auto format(const state_model& model, state_id id) const -> std::string override;
    [[nodiscard]] auto validate(const state_model& model, state_id id) const
        -> std::optional<std::string> override;
};

#endif
