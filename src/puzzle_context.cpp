#include "puzzle_context.hpp"

#include <stdexcept>

auto puzzle_context::replace(std::unique_ptr<puzzle_kind> next) -> void
{
    kind = std::move(next);
    ++generation_value;
}

auto puzzle_context::model() -> state_model&
{
    if (!kind || !kind->model) {
        throw std::runtime_error("No active puzzle model");
    }
    return *kind->model;
}

auto puzzle_context::model() const -> const state_model&
{
    if (!kind || !kind->model) {
        throw std::runtime_error("No active puzzle model");
    }
    return *kind->model;
}

auto puzzle_context::presenter() -> puzzle_presenter&
{
    if (!kind || !kind->presenter) {
        throw std::runtime_error("No active puzzle presenter");
    }
    return *kind->presenter;
}

auto puzzle_context::presenter() const -> const puzzle_presenter&
{
    if (!kind || !kind->presenter) {
        throw std::runtime_error("No active puzzle presenter");
    }
    return *kind->presenter;
}

auto puzzle_context::codec() -> puzzle_codec&
{
    if (!kind || !kind->codec) {
        throw std::runtime_error("No active puzzle codec");
    }
    return *kind->codec;
}

auto puzzle_context::codec() const -> const puzzle_codec&
{
    if (!kind || !kind->codec) {
        throw std::runtime_error("No active puzzle codec");
    }
    return *kind->codec;
}

auto puzzle_context::type_id() const -> const std::string&
{
    static const std::string empty;
    return kind ? kind->id : empty;
}

auto puzzle_context::generation() const -> std::uint64_t
{
    return generation_value;
}

auto puzzle_context::valid() const -> bool
{
    return kind != nullptr;
}
