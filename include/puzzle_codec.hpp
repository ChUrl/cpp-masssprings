#ifndef PUZZLE_CODEC_HPP_
#define PUZZLE_CODEC_HPP_

#include "state_model.hpp"

#include <optional>
#include <string>
#include <string_view>

/**
 * Parses and formats a puzzle state for preset files.
 */
class puzzle_codec
{
public:
    virtual ~puzzle_codec() = default;

    [[nodiscard]] virtual auto parse(std::string_view text, state_model& model) const
        -> std::optional<state_id> = 0;
    [[nodiscard]] virtual auto format(const state_model& model, state_id id) const
        -> std::string = 0;
    [[nodiscard]] virtual auto validate(const state_model& model, state_id id) const
        -> std::optional<std::string> = 0;
};

#endif
