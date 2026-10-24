#ifndef PUZZLE_CONTEXT_HPP_
#define PUZZLE_CONTEXT_HPP_

#include "puzzle_codec.hpp"
#include "puzzle_presenter.hpp"
#include "state_model.hpp"
#include "util.hpp"

#include <cstdint>
#include <memory>
#include <string>

struct puzzle_kind
{
    std::string id;
    std::unique_ptr<state_model> model;
    std::unique_ptr<puzzle_presenter> presenter;
    std::unique_ptr<puzzle_codec> codec;
};

/**
 * Owns the active puzzle kind. The context itself is stable for the lifetime of the application;
 * replacing the kind is how runtime puzzle switching works. Consumers should hold a reference to
 * the context, not to the model or presenter.
 */
class puzzle_context
{
private:
    std::unique_ptr<puzzle_kind> kind;
    std::uint64_t generation_value = 0;

public:
    puzzle_context() = default;
    NO_COPY_NO_MOVE(puzzle_context);

    auto replace(std::unique_ptr<puzzle_kind> next) -> void;

    [[nodiscard]] auto model() -> state_model&;
    [[nodiscard]] auto model() const -> const state_model&;
    [[nodiscard]] auto presenter() -> puzzle_presenter&;
    [[nodiscard]] auto presenter() const -> const puzzle_presenter&;
    [[nodiscard]] auto codec() -> puzzle_codec&;
    [[nodiscard]] auto codec() const -> const puzzle_codec&;

    [[nodiscard]] auto type_id() const -> const std::string&;
    [[nodiscard]] auto generation() const -> std::uint64_t;
    [[nodiscard]] auto valid() const -> bool;
};

#endif
